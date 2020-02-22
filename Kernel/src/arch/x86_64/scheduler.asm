[BITS 64]

extern processStack
extern processBase
extern processEntryPoint
extern processPML4
extern processFxStatePtr

global TaskSwitch
global ReadRIP
global IdleProc

extern IdleProcess

section .text

ReadRIP:
    mov rax, [rsp]
    ret

TaskSwitch:
	cli
    mov rbx, [processEntryPoint]
    mov rsp, [processStack]
    mov rbp, [processBase]
    mov rax, [processPML4]
    mov cr3, rax
    
	mov rax, 0xFFFFFFFF8000BEEF ; Let the scheduler know the task has been switched
    sti
    jmp rbx

IdleProc:
    call IdleProcess
    jmp IdleProc