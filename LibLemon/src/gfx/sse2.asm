global memcpy_sse2:function
global memset32_sse2:function
global memset64_sse2:function
global memcpy_sse2_unaligned:function

section .text

memcpy_sse2:
	xor rax, rax
	mov rcx, rdx
	
	test rcx, rcx
	jz .ret
.loop:
	movdqa xmm0, [rsi + rax]

	movdqa [rdi + rax], xmm0

	add rax, 0x10
	loop .loop
.ret:
	ret

memcpy_sse2_unaligned:
	xor rax, rax
	mov rcx, rdx
.loop:
	movdqu xmm0, [rsi + rax]

	movdqu [rdi + rax], xmm0

	add rax, 0x10
	loop .loop
	
	ret

memset32_sse2:
	push rbp    ; save the prior rbp value
    mov rbp, rsp

	mov rcx, rdx

	cmp rcx, 1
	jle .ret ; Avoid 0s
	
	movq xmm1, rsi
	pxor xmm0, xmm0

	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1

.loop:
	movdqa [rdi], xmm0
	add rdi, 0x10
	
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
	pxor xmm0, xmm0

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