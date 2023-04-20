#pragma once

enum {
    _sys_read = 0,
    _sys_write = 1,
    _sys_openat = 2,
    _sys_fstatat = 3,
    _sys_lseek = 4,
    _sys_mmap = 5,
    _sys_mprotect = 6,
    _sys_munmap = 7,
    _sys_ioctl = 8,
    _sys_pread = 9,
    _sys_pwrite = 10,
    _sys_execve = 11,
    _sys_sigprocmask = 12,
    _sys_sigaction = 13,
    _sys_kill = 14,

    _le_log = 0x10000,
    _le_boot_timer = 0x10001,
    _le_handle_close = 0x10002,
    _le_handle_dup = 0x10003,
    _le_futex_wait = 0x10004,
    _le_futex_wake = 0x10005,
    _le_set_user_tcb = 0x10006,
    _le_create_process = 0x10007,
    _le_create_thread = 0x10008,
    _le_nanosleep = 0x10009
};
