#pragma once

#include <lemon/syscall.h>
#include <lemon/types.h>

namespace Lemon{
    /////////////////////////////
    /// \brief DestroyKObject (object)
    ///
    /// Destroy a KernelObject
    ///
    /// \param object (handle_t) Object to destroy
    ///
    /// \return negative error code on failure
    /////////////////////////////
    inline long DestroyKObject(const handle_t& obj){
        return syscall(SYS_KERNELOBJECT_DESTROY, obj);
    }
}