#pragma once

#include <utility>
#include <new>

#include <stddef.h>
#include <stdint.h>

template<typename T>
class Vector {
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

        inline T& operator*() const { return vector.m_data[pos]; }

        inline T* operator->() const { return &vector.m_data[pos]; }

        inline friend bool operator==(const VectorIterator& l, const VectorIterator& r) {
            return l.pos == r.pos;
        }

        inline friend bool operator!=(const VectorIterator& l, const VectorIterator& r) {
            return l.pos != r.pos;
        }
    };

public:
    Vector() {}
    ~Vector() {
        clear();
    }

    void push_back(T value) {
        ensure(m_size + 1);

        new (&m_data[m_size]) T(std::move(value));
        m_size++;
    }

    size_t size() const {
        return m_size;
    }

    void clear() {
        delete[] m_data;

        m_size = 0;
        m_capacity = 0;

        m_data = nullptr;
    }

    void ensure(size_t capacity) {
        if (m_capacity >= capacity) {
            return;
        }

        if (m_capacity == 0) {
            m_capacity = 1;
        }

        while (m_capacity < capacity) {
            m_capacity *= 2;
        }

        if (m_data) {
            T *new_data = (T*)new uint8_t[m_capacity * sizeof(T)];
            for (size_t i = 0; i < m_size; i++) {
                new (new_data + i) T(std::move(m_data[i]));
            }

            delete[] m_data;
            m_data = new_data;
        } else {
            m_data = (T*)new uint8_t[m_capacity * sizeof(T)];
        }
    }

    VectorIterator begin() {
        return VectorIterator{ *this };
    }

    VectorIterator end() {
        VectorIterator it{ *this };
        it.pos = m_size;

        return it;
    }

private:
    T *m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
};
