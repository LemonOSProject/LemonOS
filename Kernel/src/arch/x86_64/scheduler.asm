extern processStack
extern processBase
extern processEntryPoint

global TaskSwitch
global ReadEIP
global IdleProc

extern IdleProcess

ReadEIP:
    mov eax, [esp]
    ret

TaskSwitch:
	cli
    mov rbx, [processEntryPoint]
    mov rsp, [processStack]
    mov rbp, [processBase]
	mov rax, 0xFFFFFFFF8000BEEF ; Let the scheduler know the task has been switched
    sti
    jmp rbx

IdleProc:
    call IdleProcess
    jmp IdleProc