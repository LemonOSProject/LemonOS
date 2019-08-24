global memcpy_sse2
global memset_sse2

memcpy_sse2:
	;ret
	push ebp    ; save the prior EBP value
    mov ebp, esp

	mov eax, [ebp + 8]
	mov ebx, [ebp + 12]
	mov ecx, [ebp + 16]
.loop:

	movdqa xmm0, [ebx]

	movdqa [eax], xmm0

	add eax, 0x10
	add ebx, 0x10
	loop .loop

	mov esp, ebp
	pop ebp
	ret

memset_sse2:
	push ebp    ; save the prior EBP value
    mov ebp, esp

	mov eax, [ebp + 8]
	mov ebx, [ebp + 12]
	mov ecx, [ebp + 16]

	cmp ecx, 1
	jle .ret ; Avoid 0s
	
	movd xmm1, ebx
	movdqa xmm0, [bigzero]

	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1
	pslldq xmm0, 4
	paddq xmm0, xmm1

.loop:
	movdqa [eax], xmm0
	add eax, 0x10
	
	loop .loop

.ret:
	mov esp, ebp
	pop ebp

	ret

align 0x10
bigzero: dq 0
dq 0