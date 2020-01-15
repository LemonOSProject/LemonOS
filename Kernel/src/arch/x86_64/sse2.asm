global memcpy_sse2

memcpy_sse2:
	push rbp    ; save the prior EBP value
    mov rbp, rsp

	mov rax, rdi
	mov rbx, rsi
	mov rcx, rdx
.loop:

	movdqa xmm0, [rbx]

	movdqa [rax], xmm0

	add rax, 0x10
	add rbx, 0x10
	loop .loop

	mov rsp, rbp
	pop rbp
	ret

global memcpy_sse2_unaligned

memcpy_sse2_unaligned:
	push rbp    ; save the prior EBP value
    mov rbp, rsp

	mov rax, rdi
	mov rbx, rsi
	mov rcx, rdx
.loop:

	movdqu xmm0, [rbx]

	movdqu [rax], xmm0

	add rax, 0x10
	add rbx, 0x10
	loop .loop

	mov rsp, rbp
	pop rbp
	ret