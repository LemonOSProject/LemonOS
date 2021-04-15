#pragma once

#include <Debug.h>
#include <TTraits.h>

#ifdef REFPTR_DEBUG
    #include <Logging.h>
#endif

template<typename T>
class FancyRefPtr{
public:
    template<class U>
    FancyRefPtr(const FancyRefPtr<U>& s, T* p) : refCount(s.GetRefCount()), obj(p) {
        __sync_fetch_and_add(refCount, 1);
    }

    FancyRefPtr() : refCount(nullptr), obj(nullptr) {

    }

    FancyRefPtr(nullptr_t) : refCount(nullptr), obj(nullptr) {

    }

    FancyRefPtr(T&& v){
        refCount = new unsigned(1);

        obj = new T(v);

        #ifdef REFPTR_DEBUG
            Log::Info("New RefPtr containing %s object, now %d references to object", TTraits<T>::name(), *refCount);
        #endif
    }

    FancyRefPtr(T* p) : obj(p) {
        if(obj){
            refCount = new unsigned(1);
        } else {
            refCount = nullptr;
        }

        #ifdef REFPTR_DEBUG
            Log::Info("New RefPtr containing existing pointer to %s object, now %d references to object", TTraits<T>::name(), *refCount);
        #endif
    }

    FancyRefPtr(const FancyRefPtr<T>& ptr){
        obj = ptr.obj;
        refCount = ptr.refCount;

        if(obj && refCount){
            __sync_fetch_and_add(refCount, 1);
        }
    }

    FancyRefPtr(FancyRefPtr<T>&& ptr){
        obj = ptr.obj;
        refCount = ptr.refCount;

        ptr.obj = nullptr;
        ptr.refCount = nullptr;
    }

    virtual ~FancyRefPtr(){
        Dereference();
    }

    ALWAYS_INLINE T* get() const { return obj; }
    ALWAYS_INLINE unsigned* GetRefCount() const { return refCount; }

    FancyRefPtr<T>& operator=(const FancyRefPtr<T>& ptr){
        Dereference();

        obj = ptr.obj;
        refCount = ptr.refCount;

        if(obj && refCount){
            __sync_fetch_and_add(refCount, 1);
        }

        return *this;
    }

    FancyRefPtr<T>& operator=(FancyRefPtr<T>&& ptr){
        Dereference();

        obj = ptr.obj;
        refCount = ptr.refCount;

        ptr.obj = nullptr;
        ptr.refCount = nullptr;

        return *this;
    }

    ALWAYS_INLINE FancyRefPtr& operator=(nullptr_t){
        Dereference();

        obj = nullptr;
        refCount = nullptr;

        return *this;
    }

    ALWAYS_INLINE bool operator==(const T* p){
        if(obj == p){
            return true;
        }
        return false;
    }

    ALWAYS_INLINE bool operator!=(const T* p){
        return !operator==(p);
    }

    ALWAYS_INLINE T& operator* () const {
        #ifdef REFPTR_ASSERTIONS
            assert(obj);
        #endif

        return *(obj);
    }

    ALWAYS_INLINE T* operator->() const {
        #ifdef REFPTR_ASSERTIONS
            assert(obj);
        #endif

        return obj;
    }

    ALWAYS_INLINE operator bool(){
        return obj;
    }

protected:
    ALWAYS_INLINE void Dereference(){
        if(refCount && obj){
            __sync_fetch_and_sub(refCount, 1);

            if(*refCount <= 0 && obj){
                delete obj;
                delete refCount;
            }

            refCount = nullptr;
            obj = nullptr;
        }
    }

    unsigned* refCount = nullptr; // Reference Count
    T* obj = nullptr;
};

template<class T, class U>
ALWAYS_INLINE static constexpr FancyRefPtr<T> static_pointer_cast(const FancyRefPtr<U>& src){
    return FancyRefPtr<T>(src, src.get());
}