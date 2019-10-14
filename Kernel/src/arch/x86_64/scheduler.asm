extern processStack
extern processBase
extern processEntryPoint

global TaskSwitch
global ReadRIP
global IdleProc

extern IdleProcess

ReadRIP:
    mov rax, [rsp]
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