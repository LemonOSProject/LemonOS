/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

// Note that struct lai_hashtable is defined in <lai/internal-util.h>.
// However, the functions are defined in this file.

#include <lai/internal-util.h>

static void lai_hashtable_grow(struct lai_hashtable *ht, int n, int m) {
    LAI_ENSURE(n >= ht->elem_capacity);
    LAI_ENSURE(m >= ht->bucket_capacity);

    void **new_elem_ptr_tab = laihost_malloc(n * sizeof(void *));
    int *new_elem_hash_tab = laihost_malloc(n * sizeof(int));
    int *new_bucket_tab = laihost_malloc(m * sizeof(int));
    if (!new_elem_ptr_tab || !new_elem_hash_tab || !new_bucket_tab)
        lai_panic("could not allocate memory for children table");

    memset(new_elem_ptr_tab, 0, n * sizeof(void *));
    for (int i = 0; i < m; i++)
        new_bucket_tab[i] = -1;

    for (int k = 0; k < ht->elem_capacity; k++) {
        if (!ht->elem_ptr_tab[k])
            continue;
        new_elem_ptr_tab[k] = ht->elem_ptr_tab[k];
        new_elem_hash_tab[k] = ht->elem_hash_tab[k];

        for (int i = 0; ; i++) {
            LAI_ENSURE(i < m);
            int b = (ht->elem_hash_tab[k] + i) & (m - 1);
            if (new_bucket_tab[b] < 0) {
                new_bucket_tab[b] = k;
                break;
            }
        }
    }

    if (ht->elem_capacity) {
        laihost_free(ht->elem_ptr_tab);
        laihost_free(ht->elem_hash_tab);
    }
    if (ht->bucket_capacity)
        laihost_free(ht->bucket_tab);

    ht->elem_ptr_tab = new_elem_ptr_tab;
    ht->elem_hash_tab = new_elem_hash_tab;
    ht->bucket_tab = new_bucket_tab;
    ht->elem_capacity = n;
    ht->bucket_capacity = m;
}

static inline void lai_hashtable_insert(struct lai_hashtable *ht, int h, void *elem) {
    LAI_ENSURE(elem);

    if (!ht->elem_capacity || !ht->bucket_capacity) {
        lai_hashtable_grow(ht, 2, 4);
    } else if (ht->num_elems + 1 >= ht->elem_capacity) {
        // TODO: We should grow more aggressively to avoid O(n) behavior
        //       if the table is almost full.
        int n = 2 * ht->elem_capacity;
        int m = 2 * ht->bucket_capacity;
        lai_hashtable_grow(ht, n, m);
    }

    // Find a free slot in the element table.
    int k;
    for (k = 0; k < ht->elem_capacity; k++)
        if (!ht->elem_ptr_tab[k])
            break;
    LAI_ENSURE(k < ht->elem_capacity);

    ht->elem_ptr_tab[k] = elem;
    ht->elem_hash_tab[k] = h;
    ht->num_elems++;

    // Add an entry in the bucket table.
    for (int i = 0; ; i++) {
        LAI_ENSURE(i < ht->bucket_capacity);
        int b = (h + i) & (ht->bucket_capacity - 1);
        if (ht->bucket_tab[b] < 0) {
            ht->bucket_tab[b] = k;
            break;
        }
    }
}

struct lai_hashtable_chain {
    int i;
};

#define LAI_HASHTABLE_CHAIN_INITIALIZER {.i = -1}

// TODO: When searching for a non-existing hash, we alreadys walk through the entire table.
//       Improve the algorithm to avoid this.
static inline int lai_hashtable_chain_advance(struct lai_hashtable *ht, int h,
        struct lai_hashtable_chain *chain) {
    LAI_ENSURE(chain->i < ht->bucket_capacity);
    for (;;) {
        chain->i++;
        LAI_ENSURE(chain->i >= 0);
        if(chain->i == ht->bucket_capacity)
            return 1;
        int b = (h + chain->i) & (ht->bucket_capacity - 1);
        int k = ht->bucket_tab[b];
        if (k >= 0 && ht->elem_hash_tab[k] == h)
            return 0;
    }
}

static inline void *lai_hashtable_chain_get(struct lai_hashtable *ht, int h,
        struct lai_hashtable_chain *chain) {
    LAI_ENSURE(chain->i >= 0);
    LAI_ENSURE(chain->i < ht->bucket_capacity);

    int b = (h + chain->i) & (ht->bucket_capacity - 1);
    int k = ht->bucket_tab[b];
    LAI_ENSURE(k >= 0);
    void *elem = ht->elem_ptr_tab[k];
    LAI_ENSURE(elem);
    return elem;
}

static inline void lai_hashtable_chain_remove(struct lai_hashtable *ht, int h,
        struct lai_hashtable_chain *chain) {
    LAI_ENSURE(chain->i >= 0);
    LAI_ENSURE(chain->i < ht->bucket_capacity);

    int b = (h + chain->i) & (ht->bucket_capacity - 1);
    int k = ht->bucket_tab[b];
    LAI_ENSURE(k >= 0);
    ht->elem_ptr_tab[k] = NULL;
    ht->bucket_tab[b] = -1;
    ht->num_elems--;
}
