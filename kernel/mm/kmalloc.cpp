#include "kmalloc.h"

#include <le/lazy_constructed.h>
#include <mm/frame_table.h>
#include <util/bit.h>

#include <assert.h>
#include <logging.h>
#include <vmem.h>

#include <limits>

#define SLAB_END (slab_size - sizeof(Slab))

namespace mm {

const size_t slab_size = PAGE_SIZE_4K;

// Store a freelist in each slab, with a stack-like structure,
// Layout:
// page start [offset to next free chunk] allocated chunks [...] [struct Slab] page end
struct Slab {
    inline uint32_t *start_of_slab() {
        return (uint32_t *)((uintptr_t)this & ~PAGE_MASK_4K);
    }

    static Slab *create(void *p, uint32_t chunk_size) {
        assert(chunk_size % sizeof(uint32_t) == 0);
        assert(chunk_size >= 8);

        Slab *slab = (Slab *)((uint8_t *)p + PAGE_SIZE_4K - sizeof(Slab));
        slab->next = nullptr;
        slab->chunk_size = chunk_size;
        slab->first_free = 0;

        uint32_t *chunk_ptr = slab->start_of_slab();
        for (uint32_t off = 0; off < (PAGE_SIZE_4K - sizeof(Slab)); off += chunk_size) {
            *chunk_ptr = chunk_size;
            chunk_ptr += chunk_size >> 2;
        }

        return slab;
    }

    void *alloc() {
        // Pop the front
        uint32_t *const data = start_of_slab() + (first_free >> 2);
        assert(first_free < SLAB_END);

        first_free = first_free + *data;
        return data;
    }

    void free(void *p) {
        uint32_t off = (uintptr_t)p & PAGE_MASK_4K;

        // This code assumes that sizeof(struct Slab) % 4 == 0
        if (off < first_free) {
            *(uint32_t *)p = first_free - off;

            first_free = off;
        } else {
            uint32_t *next_free = start_of_slab() + (first_free >> 2);
            
            off -= first_free;
            
            while (next_free < (uint32_t *)this) {
                if (off < *next_free) {
                    // The offset of the next chunk needs to be decreased by the offset
                    // of the chunk we are trying to insert
                    *(uint32_t *)p = *next_free - off;
                    *next_free = off;

                    return;
                }

                off -= *next_free;
                next_free += (*next_free) >> 2;
            }

            // If we reached the end of the freelist, something bad probably happened
            assert(!"Failed to free chunk in slab");
        }
    }

    Slab *next;
    uint32_t chunk_size;

    // 4 byte chunks until the first free chunk
    uint32_t first_free;
};

static_assert(sizeof(struct Slab) % 4 == 0);

struct SlabAllocator {
    SlabAllocator() {
        for (size_t i = 0; i < num_buckets; i++) {
            buckets[i] = nullptr;
            bucket_sizes[i] = min_bucket_size << i;
        }
    }

    static consteval size_t max_bucket_size() {
        return min_bucket_size << (num_buckets - 1);
    }

    void *alloc(size_t sz) {
        assert(sz >= 8);

        if (sz <= max_bucket_size()) {
            // __builtin_clzl takes unsigned long
            size_t bucket_index = std::numeric_limits<unsigned long>::digits -
                                  __builtin_clzl(sz / min_bucket_size) - 1;

            // If the number is not a power of 2,
            // move up to the next bucket so we can fit sz
            if (sz & (sz - 1)) {
                bucket_index++;
            }

            Slab **bucket = buckets + bucket_index;
            while (*bucket) {
                // If the bucket has remaining space, alloc!
                if ((*bucket)->first_free < SLAB_END) {
                    return (*bucket)->alloc();
                }

                // Set the bucket pointer to be the next of this bucket
                bucket = &(*bucket)->next;
            }

            *bucket = alloc_slab(bucket_sizes[bucket_index]);

            return (*bucket)->alloc();
        } else {
            log_error("alloc() does not support allocations > {} bytes", max_bucket_size());

            return nullptr;
        }
    }

    void free(void *p) {
        // TODO: Free frames for empty slabs

        // The struct Slab * sits at the end of the page, so round up to the next page,
        // and subtract the slab
        Slab *slab = (Slab *)(((uintptr_t)p & ~(PAGE_SIZE_4K - 1)) + PAGE_SIZE_4K - sizeof(Slab));

        slab->free(p);
    }

    Slab *alloc_slab(size_t chunk_size) {
        auto *frame = mm::alloc_frame();

        if (frame) {
            *(uint32_t *)(frame->virtual_mapping()) = 0;

            return Slab::create(frame->virtual_mapping(), chunk_size);
        }

        return nullptr;
    }

    // Max bucket size 256,
    // Min bucket size 8 for alignment purposes
    static constexpr size_t num_buckets = 6;
    static constexpr size_t min_bucket_size = 8;

    Slab *buckets[num_buckets];
    size_t bucket_sizes[num_buckets];
};

LazyConstructed<SlabAllocator> slab_allocator;

void *kmalloc(size_t sz) {
    return slab_allocator->alloc(sz);
}

void kfree(void *p) {
    return slab_allocator->free(p);
}

} // namespace mm
