global memcpy_sse2
global memset32_sse2
global memset64_sse2
global memcpy_sse2_unaligned

memcpy_sse2:
	;ret
	push rbp    ; save the prior rbp value
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

memcpy_sse2_unaligned:
	;ret
	push rbp    ; save the prior rbp value
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

memset32_sse2:
	push rbp    ; save the prior rbp value
    mov rbp, rsp

	mov rax, rcx
	mov rbx, rdx
	mov rcx, r8

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

memset64_sse2:
	push rbp    ; save the prior rbp value
    mov rbp, rsp

	mov rax, rcx
	mov rbx, rdx
	mov rcx, r8

	cmp rcx, 1
	jle .ret ; Avoid 0s
	
	movq xmm1, rbx
	movdqa xmm0, [bigzero]

	paddq xmm0, xmm1
	pslldq xmm0, 8
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