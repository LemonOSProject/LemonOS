/*
 * Lightweight ACPI Implementation
 * Copyright (C) 2018-2019 the lai authors
 */

#pragma once

// Note that struct lai_list and struct lai_list_item is defined in <lai/internal-util.h>.
// However, the functions are defined in this file.

#include <lai/internal-util.h>

static inline void lai_list_init(struct lai_list *list) {
    struct lai_list_item *hook = &list->hook;
    hook->next = hook;
    hook->prev = hook;
}

static inline void lai_list_link(struct lai_list *list, struct lai_list_item *item) {
    struct lai_list_item *hook = &list->hook;
    struct lai_list_item *tail = hook->prev;
    item->next = hook;
    item->prev = tail;
    tail->next = item;
    hook->prev = item;
}

static inline void lai_list_unlink(struct lai_list_item *item) {
    struct lai_list_item *prev = item->prev;
    struct lai_list_item *next = item->next;
    item->prev = NULL;
    item->next = NULL;
    prev->next = next;
    next->prev = prev;
}

static inline struct lai_list_item *lai_list_first(struct lai_list *list) {
    struct lai_list_item *next = list->hook.next;
    if (next == &list->hook)
        return NULL;
    return next;
}

static inline struct lai_list_item *lai_list_next(struct lai_list *list,
                                                  struct lai_list_item *item) {
    struct lai_list_item *next = item->next;
    if (next == &list->hook)
        return NULL;
    return next;
}
