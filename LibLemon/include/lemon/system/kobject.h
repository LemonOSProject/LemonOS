#pragma once

#include <lemon/syscall.h>
#include <lemon/types.h>

#include <stddef.h>

namespace Lemon{
    /////////////////////////////
    /// \brief WaitForKernelObject (object)
    ///
    /// Wait on one KernelObject
    ///
    /// \param object Object to wait on
    ///
    /// \return negative error code on failure
    /////////////////////////////
    inline long WaitForKernelObject(const handle_t& obj){
        return syscall(SYS_KERNELOBJECT_WAIT_ONE, obj);
    }

    /////////////////////////////
    /// \brief WaitForKernelObject (objects, count)
    ///
    /// Wait on one KernelObject
    ///
    /// \param objects Pointer to array of handles to wait on
    /// \param count Amount of objects to wait on
    ///
    /// \return negative error code on failure
    /////////////////////////////
    inline long WaitForKernelObject(const handle_t* const objects, const size_t count){
        return syscall(SYS_KERNELOBJECT_WAIT, objects, count);
    }

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