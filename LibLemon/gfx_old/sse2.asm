global memcpy_sse2
global memset_sse2
global memcpy_sse2_unaligned

memcpy_sse2:
	;ret
	push rbp    ; save the prior EBP value
    mov rbp, rsp

	mov rax, [rbp + 8]
	mov rbx, [rbp + 12]
	mov rcx, [rbp + 16]
.loop:

	movdqa xmm0, [rbx]

	movdqa [rax], xmm0

	add rax, 0x10
	add rbx, 0x10
	loop .loop

	mov rsp, rbp
	pop rbp
	ret

memcpy_sse2_unaligned:
	;ret
	push rbp    ; save the prior EBP value
    mov rbp, rsp

	mov rax, [rbp + 8]
	mov rbx, [rbp + 12]
	mov rcx, [rbp + 16]
.loop:

	movdqu xmm0, [ebx]

	movdqu [eax], xmm0

	add rax, 0x10
	add rbx, 0x10
	loop .loop

	mov rsp, rbp
	pop rbp
	ret

memset_sse2:
	push rbp    ; save the prior EBP value
    mov rbp, rsp

	mov rax, [rbp + 8]
	mov rbx, [rbp + 12]
	mov rcx, [rbp + 16]

	cmp rcx, 1
	jle .ret ; Avoid 0s
	
	movq xmm1, rbx
	movdqa xmm0, [bigzero]

	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1

.loop:
	movdqa [rax], xmm0
	add rax, 0x10
	
	loop .loop

.ret:
	mov rsp, rbp
	pop rbp

	ret

align 0x10
bigzero: dq 0
dq 0
