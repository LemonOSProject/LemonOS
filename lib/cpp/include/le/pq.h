#pragma once

#include <type_traits>
#include <utility>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

template<typename T>
concept PQValue = requires(T t) {
    std::is_trivially_copyable<T>();
};

template<PQValue T, typename PriorityType>
class PQ {
public:
    ~PQ() {
        delete (uint8_t *)m_data;
    }

    T &peek() {
        return m_data->v;
    }

    PriorityType &peek_priority() const {
        return m_data->p;
    }

    [[nodiscard]] int push(T v, PriorityType p) {
        if (grow()) {
            return 1;
        }

        // Start at the end of the heap and move the items around until we find a slot for the node
        size_t i = m_size - 1;
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (m_data[parent].p > p) {
                m_data[i] = std::move(m_data[parent]);
                i = parent;
            } else {
                break;
            }
        }

        m_data[i] = {
            std::move(v),
            std::move(p)
        };

        return 0;
    }

    T pop() {
        auto v = pop_nofree();

        if (m_size * 4 < m_capacity) {
            auto *old_data = (uint8_t *)m_data;

            m_capacity /= 2;

            if (m_capacity == 0) {
                m_data = nullptr;
            } else {
                auto *new_data = new uint8_t[m_capacity];

                memcpy(new_data, old_data, m_size * sizeof(Node));

                m_data = (Node *)new_data;
            }

            delete old_data;
        }

        return v;
    }

    T pop_nofree() {
        auto v = std::move(m_data->v);

        // Put the last item at the front and fix down
        m_size--;
        m_data[0] = m_data[m_size];

        int i = 0;
        while (i * 2 < m_size) {
            int child = i * 2 + 1;

            // Select the smallest child
            if (child < m_size && m_data[child].p > m_data[child + 1].p)
                child = child + 1;

            if (m_data[i].p <= m_data[child].p) {
                break;
            }

            // Swap as the child is smaller than the current node
            std::swap(m_data[i], m_data[child]);

            i = child;
        }

        return v;
    }

private:
    struct Node {
        T v;
        PriorityType p;
    };

    [[nodiscard]] int grow() {
        m_size++;

        if (m_size >= m_capacity) {
            m_capacity *= 2;
            if (!m_capacity) {
                m_capacity = 1;
            }

            auto *data = (Node *)new uint8_t[m_capacity * sizeof(Node)];
            if (!data) {
                return 1;
            }

            if (m_data) {
                memcpy(data, m_data, (m_size - 1) * sizeof(Node));
                delete (uint8_t *)m_data;
            }

            m_data = data;
        }

        return 0;
    }

    Node *m_data = nullptr;

    size_t m_size = 0;
    size_t m_capacity = 0;
};
