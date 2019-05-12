section .text

global _start

extern pmain
extern win_class
extern surface
extern whandle

global os_new_window
global os_copy_surface

_start:
    call pmain
end:
    jmp end
