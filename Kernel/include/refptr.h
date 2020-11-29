#pragma once

#include <debug.h>
#include <ttraits.h>

#ifdef REFPTR_DEBUG
    #include <logging.h>
#endif

template<typename T>
class FancyRefPtr{
private:
    unsigned* refCount = nullptr; // Reference Count
    T* obj = nullptr;

public:
    template<class U>
    FancyRefPtr(const FancyRefPtr<U>& s, T* p){
        obj = p;
        refCount = s.getRefCount();

        __sync_fetch_and_add(refCount, 1);
    }

    FancyRefPtr(){
        refCount = new unsigned;
        *(refCount) = 1;

        obj = nullptr;
    }

    FancyRefPtr(T&& v){
        refCount = new unsigned;
        *(refCount) = 1;

        obj = new T(v);

        #ifdef REFPTR_DEBUG
            Log::Info("New RefPtr containing %s object, now %d references to object", TTraits<T>::name(), *refCount);
        #endif
    }

    FancyRefPtr(T* p){
        refCount = new unsigned;
        *(refCount) = 1;

        obj = p;

        #ifdef REFPTR_DEBUG
            Log::Info("New RefPtr containing existing pointer to %s object, now %d references to object", TTraits<T>::name(), *refCount);
        #endif
    }

    FancyRefPtr(const FancyRefPtr<T>& ptr){
        obj = ptr.obj;
        refCount = ptr.refCount;

        __sync_fetch_and_add(refCount, 1);
    }

    FancyRefPtr(FancyRefPtr<T>&& ptr){
        obj = ptr.obj;
        refCount = ptr.refCount;
    }

    virtual ~FancyRefPtr(){
        if(refCount && obj){
            if(*refCount > 0){
                __sync_fetch_and_sub(refCount, 1);
            } else {
                return; // Another thread deleting?
            }
        } else {
            return;
        }

        #ifdef REFPTR_ASSERTIONS
            assert(refCount);
        #endif

        #ifdef REFPTR_DEBUG
            Log::Info("RefPtr: %s object unreferenced, %d references to object", TTraits<T>::name(), *refCount);
        #endif

        if((*refCount) <= 0){
            delete refCount;
            delete obj;

            #ifdef REFPTR_DEBUG
                Log::Info("RefPtr: Deleting object");
            #endif
        }
    }

    inline T* get() const { return obj; }
    inline unsigned* getRefCount() const { return refCount; }

    FancyRefPtr<T>& operator=(const FancyRefPtr<T>& ptr){
        obj = ptr.obj;
        refCount = ptr.refCount;

        __sync_fetch_and_add(refCount, 1);

        return *this;
    }

    inline bool operator==(const T* p){
        if(obj == p){
            return true;
        }
        return false;
    }

    inline bool operator!=(const T* p){
        return !operator==(p);
    }

    inline T& operator* (){
        #ifdef REFPTR_ASSERTIONS
            assert(obj);
        #endif

        return *(obj);
    }

    inline T* operator->(){
        #ifdef REFPTR_ASSERTIONS
            assert(obj);
        #endif

        return obj;
    }
};

template<class T, class U>
inline static FancyRefPtr<T> static_pointer_cast(const FancyRefPtr<U>& src){
    return FancyRefPtr<T>(src, src.get());
}