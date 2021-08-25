#pragma once

#include <Compiler.h>

// Class for handling usermode pointers
// 
template<typename T>
class UserPointer {
    ALWAYS_INLINE bool Check(){

    }

private:
    T* m_ptr;
};

template<typename T>
class UserBuffer {
    ALWAYS_INLINE bool Check(){
        
    }

private:
    T* m_buffer;
    unsigned m_count;
};