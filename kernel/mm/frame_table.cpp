#include "frame_table.h"

#include <assert.h>
#include <logging.h>
#include <panic.h>

#define FRAME_LIST_ENTRIES 3
#define NULL_FRAME 0

namespace mm {

Frame *frames;
uintptr_t frame_metadata_phys;

struct FrameList {
    uint64_t head;
    uint64_t tail;

    size_t length;

    void insert(Frame *frame) {
        frame->next = head;
        frame->prev = NULL_FRAME;

        uint64_t ref = frame - frames;
        assert(ref);

        if (head) {
            (frames + head)->prev = ref;
        }

        head = ref;

        if (!tail) {
            tail = ref;
        }

        length++;
    }

    void remove(Frame *frame) {
        uint64_t ref = frame - frames;
        assert(ref);

        if (ref == head) {
            head = frame->next;
        }
        
        if (ref == tail) {
            tail = frame->prev;
        }

        if (frame->prev) {
            (frames + frame->prev)->next = frame->next;
        }
        
        if (frame->next) {
            (frames + frame->next)->prev = frame->prev;
        }

        length--;
    }
};

FrameList free_frames;
FrameList non_paged_frames;
FrameList paged_frames;

FrameList *lists[FRAME_LIST_ENTRIES] = {
    &free_frames,
    &non_paged_frames,
    &paged_frames
};

void init() {
    for(int i = 0; i < FRAME_LIST_ENTRIES; i++) {
        lists[i]->head = lists[i]->tail = NULL_FRAME;
        lists[i]->length = 0;
    }
}

void populate_frames(uintptr_t base, size_t len, bool in_use) {
    len >>= PAGE_BITS_4K;

    // Never map the zero frame
    if (base == 0) {
        base += PAGE_SIZE_4K;
        len--;
    }

    Frame *frame = frames + (base >> PAGE_BITS_4K);

    while (len--) {
        if (in_use) {
            non_paged_frames.insert(frame);
        } else {
            free_frames.insert(frame);
        }

        frame++;
    }
}

Frame *alloc_frame() {
    if (free_frames.length > 0) {
        Frame *frame = &frames[free_frames.head];
        assert(frame - frames);

        free_frames.remove(frame);
        non_paged_frames.insert(frame);

        return frame;
    } else {
        log_info("Used frames: {}", num_non_paged_frames());
        lemon_panic("Out of frames!");
    }

    return NULL_FRAME;
}

void free_frame(Frame *frame) {
    FrameList *list = lists[frame->list];

    list->remove(frame);

    free_frames.insert(frame);
}

size_t num_free_frames() {
    return free_frames.length;
}

size_t num_non_paged_frames() {
    return non_paged_frames.length;
}

};
