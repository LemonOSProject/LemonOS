#pragma once

#include <Assert.h>
#include <CString.h>
#include <Compiler.h>
#include <MM/KMalloc.h>
#include <Move.h>
#include <Spinlock.h>

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

// Type is required to be a pointer with the members next and prev
template <typename T> class FastList final {
public:
    FastList() {
        front = NULL;
        back = NULL;
        num = 0;
    }

    FastList(T&& other) {
        front = other.front;
        back = other.back;
        num = other.num;

        memset(&other, 0, sizeof(FastList<T>));
    }

    ALWAYS_INLINE FastList<T>& operator=(FastList<T>&& other) {
        front = other.front;
        back = other.back;
        num = other.num;

        memset(&other, 0, sizeof(FastList<T>));

        return *this;
    }

    // Copying a FastList is asking for trouble
    FastList(const FastList& other) = delete;
    ALWAYS_INLINE FastList<T>& operator=(const FastList<T>& other) = delete;

    ~FastList() {}

    void clear() {
        front = NULL;
        back = NULL;
        num = 0;
    }

    void add_back(const T& obj) {
        obj->next = front;

        if (!front) {
            obj->prev = obj;
            obj->next = obj;
            front = obj;
        } else {
            assert(back);

            obj->prev = back;
            back->next = obj;
            front->prev = obj;
        }

        back = obj;
        num++; // By having this being the last thing we do, when consumers take from the front, the producer should not
               // have to acquire a lock
    }

    void add_front(const T& obj) {
        if (!back) {
            obj->prev = obj;
            obj->next = obj;
            back = obj;
        } else {
            assert(front);

            obj->next = front;
            front->prev = obj;
            back->next = obj;
        }

        front = obj;
        obj->prev = back;

        num++;
    }

    void insert(const T& obj, T& it) {
        if (it == front) {
            add_front(obj);
            return;
        }

        assert(it);

        obj->prev = it->prev;
        obj->next = it;

        if (it->prev) {
            it->prev->next = obj;
        }
        it->prev = obj;

        num++;
    }

    T operator[](unsigned pos) { return get_at(pos); }

    T get_at(unsigned pos) {
        assert(num > 0 && pos < num && front);

        T current = front;

        for (unsigned int i = 0; i < pos && i < num && current->next; i++)
            current = current->next;

        return current;
    }

    __attribute__((always_inline)) inline unsigned get_length() const { return num; }

    T remove_at(unsigned pos) {
        assert(num > 0);
        assert(pos < num);
        assert(front != nullptr);

        T current = front;

        for (unsigned int i = 0; i < pos && current; i++)
            current = current->next;

        assert(current);

        if (current->next)
            current->next->prev = current->prev;
        if (current->prev)
            current->prev->next = current->next;
        if (front == current)
            front = current->next;
        if (back == current)
            back = current->prev;

        current->next = current->prev = nullptr;

        if (!(--num))
            front = back = nullptr;

        return current;
    }

    void remove(T obj) {
        if (!front || !num) {
            assert(front && num);
            return;
        }

        if (obj->next && obj->next != obj) {
            assert(obj->next->prev == obj);

            obj->next->prev = obj->prev;
        }
        if (obj->prev && obj->prev != obj) {
            assert(obj->prev->next == obj);

            obj->prev->next = obj->next;
        }
        if (front == obj)
            front = obj->next;
        if (back == obj)
            back = obj->prev;

        obj->next = obj->prev = nullptr;

        --num;

        if (!num)
            front = back = nullptr;
    }

    __attribute__((always_inline)) inline T next(const T& o) const {
        if (o->next == front) {
            return nullptr;
        }

        return o->next;
    }

    __attribute__((always_inline)) inline T get_front() const { return front; }

    __attribute__((always_inline)) inline T get_back() const { return back; }

public:
    T front;
    T back;
    unsigned num;
};

template <typename T> class List {
public:
    List() {
        front = NULL;
        back = NULL;
        num = 0;
        lock = 0;
    }

    ~List() {
        releaseLock(&lock);

        clear();

        while (cache.get_length()) {
            kfree(cache.remove_at(0));
        }
    }

    List& operator=(const List& l) {
        clear();

        for (auto& i : l) {
            add_back(i);
        }

        return *this;
    }

    List& operator=(List&& l) {
        acquireLock(&l.lock);

        front = l.front;
        back = l.back;
        num = l.num;
        cache = std::move(l.cache);

        l.front = nullptr;
        l.back = nullptr;
        l.num = 0;

        releaseLock(&l.lock);

        return *this;
    }

    void clear() {
        acquireLock(&lock);

        ListNode<T>* node = front;
        while (node) {
            ListNode<T>* n = node->next;

            DestroyNode(node);
            node = n;
        }
        front = NULL;
        back = NULL;
        num = 0;

        releaseLock(&lock);
    }

    T& add_back(T&& obj) {
        acquireLock(&lock);

        ListNode<T>* node = AllocateNode();
        assert(node);

        new (&node->obj) T(std::move(obj));
        node->next = node->prev = nullptr;

        if (!front) {
            front = node;
        } else if (back) {
            back->next = node;
            node->prev = back;
        }
        back = node;
        num++;

        releaseLock(&lock);

        return node->obj;
    }

    T& add_back(const T& obj) {
        acquireLock(&lock);

        ListNode<T>* node = AllocateNode();
        assert(node);

        new (&node->obj) T(obj);
        node->next = node->prev = nullptr;

        if (!front) {
            front = node;
        } else if (back) {
            back->next = node;
            node->prev = back;
        }
        back = node;
        num++;

        releaseLock(&lock);

        return node->obj;
    }

    void add_back_unlocked(const T& obj) {
        acquireLock(&lock);

        ListNode<T>* node = AllocateNode();

        assert(node);

        new (&node->obj) T(obj);
        node->next = node->prev = nullptr;

        if (!front) {
            front = node;
        } else if (back) {
            back->next = node;
            node->prev = back;
        }
        back = node;
        num++;

        releaseLock(&lock);
    }

    T& add_front(const T& obj) {
        acquireLock(&lock);

        ListNode<T>* node = AllocateNode();

        new (&node->obj) T(obj);
        node->next = node->prev = nullptr;

        if (!back) {
            back = node;
        } else if (front) {
            front->prev = node;
            node->next = front;
        }

        front = node;
        num++;

        releaseLock(&lock);

        return node->obj;
    }

    void insert(const T& obj, size_t pos) {
        if (!num) {
            add_back(obj);
            return;
        }

        if (!pos) {
            add_front(obj);
            return;
        }

        acquireLock(&lock);
        ListNode<T>* current = front;

        for (unsigned int i = 0; i < pos && i < num && current->next; i++)
            current = current->next;

        ListNode<T>* node = AllocateNode();

        assert(node);

        new (&node->obj) T(obj);
        InsertNodeAfter(current);

        releaseLock(&lock);
    }

    T& insert(const T& obj, ListIterator<T>& it) {
        if (it.node == back) {
            return add_back(obj);
        }

        assert(it.node);
        ListNode<T>* current = it.node;

        acquireLock(&lock);

        ListNode<T>* node = AllocateNode();
        assert(node);

        new (&node->obj) T(obj);
        InsertNodeBefore(node, current);

        releaseLock(&lock);

        return node->obj;
    }

    T& insert(T&& obj, ListIterator<T>& it) {
        if (it.node == back) {
            return add_back(obj);
        }

        assert(it.node);
        ListNode<T>* current = it.node;

        acquireLock(&lock);

        ListNode<T>* node = AllocateNode();
        assert(node);

        new (&node->obj) T(std::move(obj));
        InsertNodeBefore(node, current);

        releaseLock(&lock);

        return node->obj;
    }

    ALWAYS_INLINE T& operator[](unsigned pos) { return get_at(pos); }

    T& get_at(unsigned pos) {
        assert(num > 0 && pos < num && front != nullptr);

        ListNode<T>* current = front;

        for (unsigned int i = 0; i < pos && i < num && current->next; i++)
            current = current->next;

        return current->obj;
    }

    void replace_at(unsigned pos, const T& obj) {
        assert(num > 0 && pos < num);

        acquireLock(&lock);

        ListNode<T>* current = front;

        for (unsigned int i = 0; i < pos; i++)
            current = current->next;

        if (current) {
            current->obj = obj;
        }

        releaseLock(&lock);
    }

    void replace_at(unsigned pos, T&& obj) {
        assert(num > 0 && pos < num);

        acquireLock(&lock);

        ListNode<T>* current = front;

        for (unsigned int i = 0; i < pos; i++)
            current = current->next;

        if (current) {
            current->obj = obj;
        }

        releaseLock(&lock);
    }

    ALWAYS_INLINE unsigned get_length() const { return num; }

    T remove_at(unsigned pos) {
        assert(num > 0);
        assert(pos < num);

        acquireLock(&lock);
        assert(front != nullptr);

        ListNode<T>* current = front;

        for (unsigned int i = 0; i < pos && current; i++)
            current = current->next;

        assert(current);

        T obj = std::move(current->obj);

        num--;

        if (front == current)
            front = current->next;
        if (back == current)
            back = current->prev;
        if (current->next)
            current->next->prev = current->prev;
        if (current->prev)
            current->prev->next = current->next;

        DestroyNode(current);

        if (!num)
            front = back = nullptr;

        releaseLock(&lock);

        return obj;
    }

    // Used when lock on heap cannot be obtained
    T remove_at_force_cache(unsigned pos) {
        assert(num > 0);
        assert(pos < num);
        assert(front != nullptr);

        acquireLock(&lock);

        ListNode<T>* current = front;

        for (unsigned int i = 0; i < pos && current; i++)
            current = current->next;

        assert(current);

        T obj = std::move(current->obj);

        num--;

        if (front == current)
            front = current->next;
        if (back == current)
            back = current->prev;
        if (current->next)
            current->next->prev = current->prev;
        if (current->prev)
            current->prev->next = current->next;

        current->obj.~T();
        cache.add_back(current);

        if (!num)
            front = back = nullptr;

        releaseLock(&lock);

        return obj;
    }

    void remove(T val) {
        if (num <= 0 || !front) {
            return;
        }

        acquireLock(&lock);

        ListNode<T>* current = front;

        while (current && current != back && current->obj != val)
            current = current->next;

        if (current) {
            if (current->prev)
                current->prev->next = current->next;
            if (current->next)
                current->next->prev = current->prev;
            if (front == current)
                front = current->next;
            if (back == current)
                back = current->prev;

            num--;

            DestroyNode(current);
        }

        releaseLock(&lock);
    }

    void remove(ListIterator<T>& it) {
        assert(it.node);

        ListNode<T>* current = it.node;
        if (current) {
            acquireLock(&lock);

            if (current->prev)
                current->prev->next = current->next;
            if (current->next)
                current->next->prev = current->prev;
            if (front == current)
                front = current->next;
            if (back == current)
                back = current->prev;

            it.node = current->prev;

            num--;

            DestroyNode(current);

            releaseLock(&lock);
        }
    }

    void allocate(unsigned count) {
        acquireLock(&lock);

        while (count--) {
            cache.add_back((ListNode<T>*)kmalloc(sizeof(ListNode<T>)));
        }

        releaseLock(&lock);
    }

    T& get_front() const {
        assert(front);
        return front->obj;
    }

    T& get_back() const {
        assert(back);
        return back->obj;
    }

    ListIterator<T> begin() const {
        ListIterator<T> it;

        if (!num || !front) {
            it.node = nullptr;
        } else {
            it.node = front;
        }

        return it;
    }

    ListIterator<T> end() const {
        ListIterator<T> it;
        it.node = nullptr;
        return it;
    }

private:
    ALWAYS_INLINE void InsertNodeBefore(ListNode<T>* node, ListNode<T>* existingNode) {
        assert(node && existingNode);

        node->prev = existingNode->prev;
        node->next = existingNode;

        if (existingNode->prev) {
            existingNode->prev->next = node;
        }
        existingNode->prev = node;

        if (existingNode == front) {
            front = node;
        }

        num++;
    }

    ALWAYS_INLINE void InsertNodeAfter(ListNode<T>* node, ListNode<T>* existingNode) {
        assert(node && existingNode);

        node->prev = existingNode;
        node->next = existingNode->next;

        if (existingNode->next) {
            existingNode->next->prev = node;
        }
        existingNode->next = node;

        if (existingNode == back) {
            back = node;
        }

        num++;
    }

    ALWAYS_INLINE ListNode<T>* AllocateNode() {
        if (!cache.get_length()) {
            return (ListNode<T>*)kmalloc(sizeof(ListNode<T>));
        } else {
            return cache.remove_at(0);
        }
    }

    ALWAYS_INLINE void DestroyNode(ListNode<T>* node) {
        node->obj.~T();
        if (cache.get_length() >= maxCache) {
            kfree(node);
        } else {
            cache.add_back(node);
        }
    }

    ListNode<T>* front;
    ListNode<T>* back;
    unsigned num;
    volatile int lock = 0;

    const unsigned maxCache = 6;
    FastList<ListNode<T>*> cache; // Prevent allocations by caching ListNodes
};