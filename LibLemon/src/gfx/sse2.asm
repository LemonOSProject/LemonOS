global memcpy_sse2

memcpy_sse2:
	ret
	;push ebp    ; save the prior EBP value
    ;mov ebp, esp

;	mov eax, [ebp + 8]
;	mov ebx, [ebp + 12]
;	mov ecx, [ebp + 16]
;.loop:

;	movdqa xmm0, [ebx]

;	movdqa [eax], xmm0

;	add eax, 0x10
;	add ebx, 0x10
;	loop .loop

;	mov esp, ebp
;	pop ebp
;	ret