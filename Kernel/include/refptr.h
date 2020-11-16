#pragma once

#include <debug.h>
#include <ttraits.h>

template<typename T>
class FancyRefPtr{
protected:
    unsigned* refCount = nullptr; // Reference Count
    T* obj = nullptr;

public:
    FancyRefPtr(){
        refCount = new unsigned;
        *(refCount) = 1;

        obj = new T();

        #ifdef DEBUG_REFPTR
            Log::Info("New RefPtr containing %s object, now %d references to object", TTraits<T>.name(), refCount);
        #endif
    }

    FancyRefPtr(T&& v){
        refCount = new unsigned;
        *(refCount) = 1;

        obj = new T();
        obj = v;

        #ifdef DEBUG_REFPTR
            Log::Info("New RefPtr containing %s object, now %d references to object", TTraits<T>.name(), refCount);
        #endif
    }

    FancyRefPtr(T* p){
        refCount = new unsigned;
        *(refCount) = 1;

        obj = p;

        #ifdef DEBUG_REFPTR
            Log::Info("New RefPtr containing existing pointer to %s object, now %d references to object", TTraits<T>.name(), refCount);
        #endif
    }

    FancyRefPtr(const FancyRefPtr<T>& ptr){
        obj = ptr.obj;
        refCount = ptr.refCount;

        (*refCount)++;
    }

    FancyRefPtr(FancyRefPtr<T>&& ptr){
        obj = ptr.obj;
        refCount = ptr.refCount;
    }

    virtual ~FancyRefPtr(){
        (*refCount)--;

        #ifdef DEBUG_REFPTR
            Log::Info("RefPtr: %s object unreferenced, %d references to object", TTraits<T>.name(), refCount);
        #endif

        if(refCount <= 0){
            delete refCount;
            delete obj;

            #ifdef DEBUG_REFPTR
                Log::Info("RefPtr: Deleting object");
            #endif
        }
    }

    FancyRefPtr<T>& operator=(const FancyRefPtr<T>& ptr){
        obj = ptr.obj;
        refCount = ptr.refCount;
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