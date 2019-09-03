/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

// Even in freestanding environments, GCC requires memcpy(), memmove(), memset()
// and memcmp() to be present. Thus, we just use them directly.
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);

//---------------------------------------------------------------------------------------
// Debugging and logging functions.
//---------------------------------------------------------------------------------------

void lai_debug(const char *, ...);
void lai_warn(const char *, ...);
__attribute__((noreturn)) void lai_panic(const char *, ...);

#define LAI_STRINGIFY(x) #x
#define LAI_EXPAND_STRINGIFY(x) LAI_STRINGIFY(x)

#define LAI_ENSURE(cond) \
    do { \
        if(!(cond)) \
            lai_panic("assertion failed: " #cond " at " \
                       __FILE__ ":" LAI_EXPAND_STRINGIFY(__LINE__) "\n"); \
    } while(0)

//---------------------------------------------------------------------------------------
// Misc. utility functions.
//---------------------------------------------------------------------------------------

static inline void lai_cleanup_free_string(char **v) {
    if (*v)
        laihost_free(*v);
}

#define LAI_CLEANUP_FREE_STRING __attribute__((cleanup(lai_cleanup_free_string)))

//---------------------------------------------------------------------------------------
// Reference counting functions.
//---------------------------------------------------------------------------------------

typedef int lai_rc_t;

__attribute__((always_inline))
inline void lai_rc_ref(lai_rc_t *rc_ptr) {
    lai_rc_t nrefs = (*rc_ptr)++;
    LAI_ENSURE(nrefs > 0);
}

__attribute__((always_inline))
inline int lai_rc_unref(lai_rc_t *rc_ptr) {
    lai_rc_t nrefs = --(*rc_ptr);
    LAI_ENSURE(nrefs >= 0);
    return !nrefs;
}

//---------------------------------------------------------------------------------------
// List data structure.
//---------------------------------------------------------------------------------------

struct lai_list_item {
    struct lai_list_item *next;
    struct lai_list_item *prev;
};

struct lai_list {
    struct lai_list_item hook;
};

//---------------------------------------------------------------------------------------
// Hash table data structure.
//---------------------------------------------------------------------------------------

struct lai_hashtable {
    int elem_capacity;   // Capacity of elem_{ptr,hash}_tab.
    int bucket_capacity; // Size of bucket_tab. *Must* be a power of 2.
    int num_elems;       // Number of elements in the table.
    void **elem_ptr_tab; // Stores the pointer of each element.
    int *elem_hash_tab;  // Stores the hash of each element.
    int *bucket_tab;     // Indexes into elem_{ptr,hash}_tab.
};
