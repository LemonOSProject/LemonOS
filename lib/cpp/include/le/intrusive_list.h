#pragma once

template <typename T, typename Tag = void> struct IntrusiveListNode {
    T *create_list() {
        next = prev = nullptr;

        return static_cast<T *>(this);
    }

    inline T *get_next() {
        return static_cast<T *>(next);
    }

    inline T *get_prev() {
        return static_cast<T *>(prev);
    }

    [[nodiscard]] inline T *remove() {
        // Set new head to be the next node,
        // later if prev exists, set the new head to be previous node
        IntrusiveListNode *new_head = next;

        if (prev) {
            prev->next = next;
            new_head = prev;
        }

        if (next) {
            next->prev = prev;
        }

        prev = next = nullptr;

        return static_cast<T *>(new_head);
    }

    void push_back(T *val) {
        IntrusiveListNode *node = this;

        while (node->next) {
            node = node->next;
        }

        node->next = val;

        val->prev = node;
        val->next = nullptr;
    }

    void insert_after(T *val) {
        val->prev = this;
        val->next = next;

        next = val;
    }

    [[nodiscard]] T *insert_before(T *val) {
        val->prev = prev;
        val->next = this;

        prev = val;

        return val;
    }

private:
    IntrusiveListNode *next = nullptr;
    IntrusiveListNode *prev = nullptr;
};
