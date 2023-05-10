#pragma once

#include <lemon/system/KernelObject.h>
#include <lemon/types.h>

namespace Lemon {
/////////////////////////////
/// \brief Reference counted \a handle_t wrapper
/////////////////////////////
class Handle final {
  public:
    inline Handle() : m_refCount(0), m_handle(-1) {}
    explicit inline Handle(le_handle_t handle) : m_handle(handle) {
        if (m_handle) {
            m_refCount = new unsigned(1);
    }
    }
    inline Handle(const Handle& handle) : m_refCount(handle.m_refCount), m_handle(handle.m_handle) { (*m_refCount)++; }
    inline Handle(Handle&& handle) : m_refCount(handle.m_refCount), m_handle(handle.m_handle) {
        handle.m_refCount = nullptr;
        handle.m_handle = -1;
    }

    inline ~Handle() { Dereference(); }

    inline Handle& operator=(const Handle& handle) {
        Dereference();

        m_refCount = handle.m_refCount;
        m_handle = handle.m_handle;

        (*m_refCount)++;

        return *this;
    }

    inline Handle& operator=(Handle&& handle) {
        Dereference();

        m_refCount = handle.m_refCount;
        m_handle = handle.m_handle;

        handle.m_refCount = nullptr;
        handle.m_handle = -1;

        return *this;
    }

    inline le_handle_t get() const { return m_handle; }
    inline operator le_handle_t() const { return m_handle; }

  private:
    inline void Dereference() {
        if (m_handle > 0) {
            if (!(m_refCount && --(*m_refCount))) { // Check that either refCount doesn't exist or is 0
                DestroyKObject(m_handle);
            }
        }

        m_handle = -1;
        m_refCount = nullptr;
    }

    unsigned* m_refCount;
    le_handle_t m_handle;
};

inline bool operator==(const Handle& l, const Handle& r){
    return l.get() == r.get();
}
} // namespace Lemon
