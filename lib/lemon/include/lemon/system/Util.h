#pragma once

#ifndef __lemon__
    #error "Lemon OS Only"
#endif

#include <lemon/system/Info.h>
#include <lemon/system/abi/process.h>

#include <sys/types.h>
#include <stdint.h>
#include <limits.h>

#include <vector>

namespace Lemon{
    /////////////////////////////
    /// \brief Yields CPU timeslice to next process
    /////////////////////////////
    void Yield();

    /////////////////////////////
    /// \brief Interrupts blocking thread
    /////////////////////////////
    long InterruptThread(pid_t tid);

    /////////////////////////////
    /// \brief Get information about process
    ///
    /// Fill a lemon_process_info struct with information about the process specified.
    ///
    /// \param pid Process ID
    /// \param pInfo Reference to process info structure
    ///
    /// \return 0 on success, -1 on failure (errno is set)
    /////////////////////////////
    int GetProcessInfo(pid_t pid, lemon_process_info_t& pInfo);

    /////////////////////////////
    /// \brief Get information about next process
    ///
    /// Fill a lemon_process_info struct with information about the process specified.
    ///
    /// \param pid Pointer of previous process ID
    /// \param pInfo Reference to process info structure
    ///
    /// \return 0 on success, 1 on end, -1 on failure (errno is set)
    /////////////////////////////
    int GetNextProcessInfo(pid_t* pid, lemon_process_info_t& pInfo);

    /////////////////////////////
    /// \brief Retrieve a list of all processes
    ///
    /// Fill a vector with information about all running processes
    ///
    /// \param list Reference to a std::vector<lemon_process_info_t>
    /////////////////////////////
    void GetProcessList(std::vector<lemon_process_info_t>& list);
}
