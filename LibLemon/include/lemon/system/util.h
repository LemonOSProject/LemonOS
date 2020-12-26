#pragma once

#ifndef __lemon__
    #error "Lemon OS Only"
#endif

#include <stdint.h>
#include <limits.h>
#include <vector>

#include <lemon/system/info.h>

typedef struct {
	uint64_t pid; // Process ID

	uint32_t threadCount; // Process Thread Count

	int32_t uid; // User ID
	int32_t gid; // Group ID

	uint8_t state; // Process State

	char name[NAME_MAX]; // Process Name

	uint64_t runningTime; // Amount of time in seconds that the process has been running
    uint64_t activeUs; // Microseconds the process has been active for
} lemon_process_info_t;

namespace Lemon{
    /////////////////////////////
    /// \brief Yields CPU timeslice to next process
    /////////////////////////////
    void Yield();

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
    int GetProcessInfo(uint64_t pid, lemon_process_info_t& pInfo);

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
    int GetNextProcessInfo(uint64_t* pid, lemon_process_info_t& pInfo);

    /////////////////////////////
    /// \brief Retrieve a list of all processes
    ///
    /// Fill a vector with information about all running processes
    ///
    /// \param list Reference to a std::vector<lemon_process_info_t>
    /////////////////////////////
    void GetProcessList(std::vector<lemon_process_info_t>& list);
}