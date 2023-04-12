# Contents
[le_log](#le_log) \
[le_boot_timer](#le_boot_timer) \
[le_handle_close](#le_handle_close) \
[le_handle_dup](#le_handle_dup) \
[le_futex_wait](#le_futex_wait) \
[le_futex_wake](#le_futex_wake) \
[le_set_user_tcb](#le_set_user_tcb) \
[le_load_module](#le_load_module) \
[le_unload_module](#le_unload_module) \
[le_query_modules](#le_query_modules)

# Types

### le_str_t
```c
typedef const char* le_str_t
```
String type for syscall parameters. In userspace it is defined as `const char*`

# Lemon Syscalls

## le_log
```c
long le_log(le_str_t msg)
```
Send string to kernel log

### Errors
EFAULT  Invalid *msg* pointer

## le_boot_timer
```c
long le_boot_timer()
```
### Return
Microseconds since boot.

This function is always successful.

## le_handle_close
```c
 long le_handle_close(le_handle_t handle);
```

Closes the specified handle

### Errors
**EBADF**   Invalid *handle* was given

## le_process_getpid
```c
long le_process_getpid(le_handle_t process)
```

## le_create_process
```c
long le_create_process(le_handle_t* handle, uint64_t flags, le_str_t name)
```

## Flags
**LE_PROCESS_FORK** The created process is a fork of the caller \
**LE_PROCESS_PID** \
**LE_PROCESS_DETACH** Do not kill the process when the handle is closed \

## Errors
**EINVAL** LE_PROCESS_PID was specified without LE_PROCESS_DETACH \
**EFAULT** `handle` or `name` is not accessible memory \
**ENAMETOOLONG** `name` was greater than *NAME_MAX* \

## le_handle_dup
```c
long le_handle_dup(le_handle_t handle, int flags)
```

## le_futex_wait
```c
long le_futex_wait(int* futex, int expected, const struct timespec* time)
```

## le_futex_wake
```c
long le_futex_wake(int* futex)
```

## le_set_user_tcb
```c
long le_set_user_tcb(uintptr_t value)
```

Sets the user Thread Control Block (TCB).

On x86_64 this sets the FS register to *value*.

This function is always successful.

## le_create_thread
```c
long le_create_thread(le_handle_t* handle, uint64_t flags, void* entry, void* stack)
```

## le_thread_gettid
```c
long le_thread_gettid(le_handle_t thread)
```

### Errors
This call is always successful

## le_load_module
```c
long le_load_module(le_str_t path)
```
### Errors
**EFAULT**  Invalid *path* ptr

**ENOENT**  Module does not exist at *path*

## le_unload_module
```c
long le_unload_module(le_str_t name)
```
### Errors
**EFAULT**  Invalid *name* ptr

**ENOENT**  Module *name* is not loaded

# UNIX Syscalls

## sys_read
```c
long sys_read(le_handle_t handle, uint8_t* buf, size_t count, ssize_t* bytesRead)
```

## sys_write
```c
long sys_write(le_handle_t handle, const uint8_t* buf, size_t count, ssize_t* bytesWritten)
```

## sys_openat
```c
long sys_openat(le_handle_t directory, le_str_t _filename, int flags, int mode, le_handle_t out)
```

## sys_fstatat

## sys_lseek

## sys_ioctl

## sys_pread

## sys_pwrite

## sys_execve
```c
long sys_execve(le_str_t filename, le_str_t const* argv, le_str_t const* envp);
```

Executes the ELF executable at *filename*. The calling process is replaced with the new program. 

*argv* is a pointer to strings containing the program arguments.
*envp* is a pointer to string containing the environment variables.

### Errors
**EFAULT** *filename* or a pointer in *argv* or *envp* does not point to accessible memory.

**ENOENT** *filename* does not exist.

**ENOEXEC** *filename* is not a valid ELF executable.
