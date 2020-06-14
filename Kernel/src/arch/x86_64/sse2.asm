global memcpy_sse2

memcpy_sse2:
	push rbp    ; save the prior EBP value
    mov rbp, rsp

	mov rcx, rdx
.loop:

	movdqa xmm0, [rsi]

	movdqa [rdi], xmm0

	add rsi, 0x10
	add rdi, 0x10
	loop .loop

	mov rsp, rbp
	pop rbp
	ret

global memcpy_sse2_unaligned

memcpy_sse2_unaligned:
	push rbp    ; save the prior EBP value
    mov rbp, rsp

	mov rcx, rdx
.loop:

	movdqu xmm0, [rsi]

	movdqu [rdi], xmm0

	add rsi, 0x10
	add rdi, 0x10
	loop .loop

	mov rsp, rbp
	pop rbp
	ret