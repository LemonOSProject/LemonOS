#pragma once

#include <CString.h>
#include <Memory.h>
#include <Move.h>
#include <Spinlock.h>
#include <TTraits.h>

template <typename T> class Vector {
    class VectorIterator {
        friend class Vector;

    protected:
        size_t pos = 0;
        const Vector<T>& vector;

    public:
        VectorIterator(const Vector<T>& newVector) : vector(newVector){};
        VectorIterator(const VectorIterator& it) : vector(it.vector) { pos = it.pos; };

        VectorIterator& operator++() {
            pos++;
            return *this;
        }

        VectorIterator& operator++(int) {
            auto it = *this;

            pos++;
            return it;
        }

        VectorIterator& operator=(const VectorIterator& other) {
            VectorIterator(other.vector);

            pos = other.pos;

            return *this;
        }

        ALWAYS_INLINE T& operator*() const { return vector.data[pos]; }

        ALWAYS_INLINE T* operator->() const { return &vector.data[pos]; }

        ALWAYS_INLINE friend bool operator==(const VectorIterator& l, const VectorIterator& r) {
            return l.pos == r.pos;
        }

        ALWAYS_INLINE friend bool operator!=(const VectorIterator& l, const VectorIterator& r) {
            return l.pos != r.pos;
        }
    };

private:
    T* data = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    lock_t lock = 0;

public:
    Vector() = default;

    Vector(const Vector<T>& x) {
        EnsureCapacity(x.get_length());

        for (unsigned i = 0; i < x.get_length(); i++) {
            data[i] = x.data[i];
        }
    }

    template <typename... D> Vector(D... data) { (add_back(data), ...); }

    ALWAYS_INLINE T& at(size_t pos) {
        assert(pos < count);

        return data[pos];
    }
    ALWAYS_INLINE const T& at(size_t pos) const {
        assert(pos < count);

        return data[pos];
    }

    ALWAYS_INLINE const T& get_at(size_t pos) const { return at(pos); }

    ALWAYS_INLINE T& operator[](size_t pos) { return at(pos); }
    ALWAYS_INLINE const T& operator[](size_t pos) const { return at(pos); }

    ALWAYS_INLINE size_t size() const { return count; }

    ALWAYS_INLINE size_t get_length() const { return count; }

    void erase(unsigned pos) {
        acquireLock(&lock);

        EraseUnlocked(pos);

        releaseLock(&lock);
    }

    void reserve(size_t allocatedSize) {
        acquireLock(&lock);

        EnsureCapacity(allocatedSize);

        releaseLock(&lock);
    }

    void resize(size_t newSize) {
        acquireLock(&lock);
        EnsureCapacity(newSize);

        size_t oldCount = count;
        count = newSize;

        if (count > oldCount) {
            for (unsigned i = oldCount; i < count; i++) {
                new (&data[i]) T();
            }
        } else if (count < oldCount) {
            if constexpr (!TTraits<T>::is_trivial()) {
                for (unsigned i = count; i < oldCount; i++) {
                    data[i].~T();
                }
            }
        }

        releaseLock(&lock);
    }

    T& add_back(const T& val) {
        acquireLock(&lock);

        EnsureCapacity(count + 1);

        T& ref = data[count];
        count++;

        new (&ref) T(val);
        releaseLock(&lock);

        return ref;
    }

    T& add_back(T&& val) {
        acquireLock(&lock);

        EnsureCapacity(count + 1);

        T& ref = data[count];
        count++;

        new (&ref) T(static_cast<T&&>(val));
        releaseLock(&lock);

        return ref;
    }

    T& pop_back() {
        assert(count);

        return data[--count];
    }

    ALWAYS_INLINE T* Data() { return data; }

    void remove(const T& val) {
        acquireLock(&lock);

        for (unsigned i = 0; i < count; i++) {
            if (data[i] == val) {
                EraseUnlocked(i);
                return;
            }
        }

        releaseLock(&lock);
    }

    ~Vector() {
        if (data) {
            kfree(data);
        }
        data = nullptr;
    }

    VectorIterator begin() const {
        VectorIterator it = VectorIterator(*this);

        it.pos = 0;

        return it;
    }

    VectorIterator end() const {
        VectorIterator it = VectorIterator(*this);

        it.pos = count;

        return it;
    }

    void clear() {
        acquireLock(&lock);

        if (data) {
            if constexpr (!TTraits<T>::is_trivial()) {
                for(unsigned i = 0; i < count; i++) {
                    data[i].~T();
                }
            }
            kfree(data);
        }

        count = capacity = 0;
        data = nullptr;
        releaseLock(&lock);
    }

private:
    ALWAYS_INLINE void EnsureCapacity(unsigned size) {
        if (size >= capacity) {
            size_t newCapacity = capacity + (size << 1) + 1;

            if (data) {
                T* oldData = data;
                data = reinterpret_cast<T*>(kmalloc(newCapacity * sizeof(T)));

                if constexpr (TTraits<T>::is_trivial()) {
                    memcpy(data, oldData, capacity * sizeof(T));
                    memset(data + capacity, 0, sizeof(T) * (size - capacity));
                } else {
                    for (unsigned i = 0; i < count && i < capacity; i++) {
                        new (&data[i]) T(std::move(oldData[i]));
                        oldData[i].~T();
                    }

                    for (unsigned i = capacity; i < size; i++) {
                        new (&data[i]) T();
                    }
                }

                kfree(oldData);
            } else {
                data = reinterpret_cast<T*>(kmalloc(newCapacity * sizeof(T)));
            }

            capacity = newCapacity;
        }
    }

    ALWAYS_INLINE void EraseUnlocked(unsigned pos) {
        assert(pos < count);

        if constexpr (TTraits<T>::is_trivial()) {
            memcpy(&data[pos], &data[pos + 1], (count - pos - 1) * sizeof(T));
        } else {
            for (unsigned i = pos; i < count - 1; i++) {
                data[i] = std::move(data[i + 1]);
            }
        }

        count--;
    }
};