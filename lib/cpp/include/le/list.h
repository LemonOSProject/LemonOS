#pragma once

#include <assert.h>
#include <stdint.h>

#include <utility>
#include <new>

template <typename T> struct ListNode {
    ListNode* next = nullptr;
    ListNode* prev = nullptr;
    T obj;
};

template <typename T> class List;

template <typename T> class ListIterator {
    friend class List<T>;

protected:
    ListNode<T>* node = nullptr;

public:
    ListIterator() = default;
    ListIterator(const ListIterator<T>&) = default;

    ListIterator& operator++() {
        assert(node);
        node = node->next;

        return *this;
    }

    ListIterator operator++(int) { // Post decrement
        ListIterator<T> v = ListIterator<T>(*this);

        assert(node);
        node = node->next;

        return v;
    }

    ListIterator& operator=(const ListIterator& other) {
        node = other.node;

        return *this;
    }

    T& operator*() {
        assert(node);

        return node->obj;
    }

    T* operator->() {
        assert(node);

        return &node->obj;
    }

    friend bool operator==(const ListIterator& l, const ListIterator& r) {
        if (l.node == r.node) {
            return true;
        } else {
            return false;
        }
    }

    friend bool operator!=(const ListIterator& l, const ListIterator& r) {
        if (l.node != r.node) {
            return true;
        } else {
            return false;
        }
    }
};

template <typename T> class List {
public:
    List() {
        m_front = nullptr;
        m_back = nullptr;
        m_num = 0;
    }

    ~List() {
        clear();
    }

    List &operator=(const List &l) {
        clear();

        for (const auto &i : l) {
            add_back(i);
        }

        return *this;
    }

    List& operator=(List&& l) {
        m_front = l.m_front;
        m_back = l.m_back;
        m_num = l.m_num;

        l.m_front = nullptr;
        l.m_back = nullptr;
        l.m_num = 0;

        return *this;
    }

    void clear() {
        ListNode<T>* node = m_front;
        while (node) {
            ListNode<T>* n = node->next;

            delete node;
            node = n;
        }

        m_front = nullptr;
        m_back = nullptr;
        m_num = 0;
    }

    T &add_back(T obj) {
        ListNode<T>* node = allocate_node();
        assert(node);

        new (&node->obj) T(std::move(obj));
        node->next = node->prev = nullptr;

        if (!m_front) {
            m_front = node;
        } else if (m_back) {
            m_back->next = node;
            node->prev = m_back;
        }

        m_back = node;

        m_num++;

        return node->obj;
    }

    T& add_front(T obj) {
        ListNode<T>* node = allocate_node();

        new (&node->obj) T(std::move(obj));
        node->next = node->prev = nullptr;

        if (!m_back) {
            m_back = node;
        } else if (m_front) {
            m_front->prev = node;
            node->next = m_front;
        }

        m_front = node;
        m_num++;

        return node->obj;
    }

    T& operator[](unsigned pos) { return get_at(pos); }

    T& get_at(unsigned pos) {
        assert(m_num > 0 && pos < m_num && m_front != nullptr);

        ListNode<T>* current = m_front;

        for (unsigned int i = 0; i < pos && i < m_num && current->next; i++)
            current = current->next;

        return current->obj;
    }

    unsigned get_length() const { return m_num; }

    T remove_at(unsigned pos) {
        assert(m_num > 0);
        assert(pos < m_num);

        assert(m_front != nullptr);

        ListNode<T>* current = m_front;

        for (unsigned int i = 0; i < pos && current; i++)
            current = current->next;

        assert(current);

        T obj = std::move(current->obj);

        m_num--;

        if (m_front == current)
            m_front = current->next;
        if (m_back == current)
            m_back = current->prev;
        if (current->next)
            current->next->prev = current->prev;
        if (current->prev)
            current->prev->next = current->next;

        delete current;

        if (!m_num)
            m_front = m_back = nullptr;

        return obj;
    }

    void remove(ListIterator<T>& it) {
        assert(it.node);

        ListNode<T>* current = it.node;
        if (current) {
            if (current->prev)
                current->prev->next = current->next;
            if (current->next)
                current->next->prev = current->prev;
            if (m_front == current)
                m_front = current->next;
            if (m_back == current)
                m_back = current->prev;

            it.node = current->prev;

            m_num--;

            delete current;
        }
    }

    T& get_front() const {
        assert(m_front);
        return m_front->obj;
    }

    T& get_back() const {
        assert(m_back);
        return m_back->obj;
    }

    ListIterator<T> begin() {
        ListIterator<T> it;

        if (!m_front) {
            it.node = nullptr;
        } else {
            it.node = m_front;
        }

        return it;
    }

    ListIterator<T> end() {
        ListIterator<T> it;

        it.node = nullptr;

        return it;
    }

private:
    inline ListNode<T>* allocate_node() {
        return (ListNode<T> *)(new uint8_t[sizeof(ListNode<T>)]);
    }

    ListNode<T> *m_front;
    ListNode<T> *m_back;

    unsigned m_num;
};
