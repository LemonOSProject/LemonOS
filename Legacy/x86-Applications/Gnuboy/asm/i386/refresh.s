

#include "asmnames.h"

	.text

	.macro _enter	
	pushl %ebx
	pushl %ebp
	pushl %esi
	pushl %edi
	movl 20(%esp), %edi
	movl 24(%esp), %esi
	movl 28(%esp), %ebp
	movl 32(%esp), %ecx
	xorl %eax, %eax
	xorl %ebx, %ebx
	.endm

	.macro _leave
	popl %edi
	popl %esi
	popl %ebp
	popl %ebx
	ret
	.endm


	.globl refresh_1
refresh_1:
	_enter
	subl $4, %esi
	subl $4, %edi
	shrl $2, %ecx
.Lrefresh_1:
	movb 2(%esi,%ecx,4), %al
	movb 3(%esi,%ecx,4), %bl
	movb (%ebp, %eax), %dl
	movb (%esi,%ecx,4), %al
	movb (%ebp, %ebx), %dh
	movb 1(%esi,%ecx,4), %bl
	rorl $16, %edx
	movb (%ebp, %eax), %dl
	movb (%ebp, %ebx), %dh
	movl %edx, (%edi,%ecx,4)
	decl %ecx
	jnz .Lrefresh_1
	_leave

	.globl refresh_2
refresh_2:
	_enter
	subl $2, %esi
	subl $4, %edi
	shrl $1, %ecx
.Lrefresh_2:
	movb 1(%esi,%ecx,2), %al
	movb (%esi,%ecx,2), %bl
	movw (%ebp,%eax,2), %dx
	rorl $16, %edx
	movw (%ebp,%ebx,2), %dx
	movl %edx, (%edi,%ecx,4)
	decl %ecx
	jnz .Lrefresh_2
	_leave

	.globl refresh_3
refresh_3:
	_enter
	subl $2, %esi
	leal (%ecx,%ecx,2), %edx
	shrl $1, %ecx
	addl %edx, %edi
.Lrefresh_3:
	movb (%esi,%ecx,2), %al
	subl $6, %edi
	movb 1(%esi,%ecx,2), %bl
	movl (%ebp,%eax,4), %edx
	movb %dl, (%edi)
	movb 2(%ebp,%eax,4), %dl
	movb %dh, 1(%edi)
	movb %dl, 2(%edi)
	movl (%ebp,%ebx,4), %edx
	movb %dl, 3(%edi)
	movb 2(%ebp,%ebx,4), %dl
	movb %dh, 4(%edi)
	movb %dl, 5(%edi)
	decl %ecx
	jnz .Lrefresh_3
	_leave

	.globl refresh_4
refresh_4:
	_enter
	subl $2, %esi
	subl $8, %edi
	shrl $1, %ecx
.Lrefresh_4:
	movb (%esi,%ecx,2), %al
	movb 1(%esi,%ecx,2), %bl
	movl (%ebp,%eax,4), %edx
	movl %edx, (%edi,%ecx,8)
	movl (%ebp,%ebx,4), %edx
	movl %edx, 4(%edi,%ecx,8)
	decl %ecx
	jnz .Lrefresh_4
	_leave



	.globl refresh_1_2x
refresh_1_2x:
	_enter
	subl $2, %esi
	subl $4, %edi
	shrl $1, %ecx
.Lrefresh_1_2x:
	movb 1(%esi,%ecx,2), %al
	movb (%esi,%ecx,2), %bl
	movb (%ebp,%eax), %al
	movb %al, %dl
	movb %al, %dh
	movb (%ebp,%ebx), %bl
	rorl $16, %edx
	movb %bl, %dl
	movb %bl, %dh
	movl %edx, (%edi,%ecx,4)
	decl %ecx
	jnz .Lrefresh_1_2x
	_leave




	.globl refresh_2_2x
refresh_2_2x:
	_enter
	subl $2, %esi
	subl $8, %edi
	shrl $1, %ecx
.Lrefresh_2_2x:
	movb (%esi,%ecx,2), %al
	movb 1(%esi,%ecx,2), %bl
	movw (%ebp,%eax,2), %dx
	rorl $16, %edx
	movw (%ebp,%eax,2), %dx
	movl %edx, (%edi,%ecx,8)
	movw (%ebp,%ebx,2), %dx
	rorl $16, %edx
	movw (%ebp,%ebx,2), %dx
	movl %edx, 4(%edi,%ecx,8)
	decl %ecx
	jnz .Lrefresh_2_2x
	_leave



	.globl refresh_4_2x
refresh_4_2x:
	_enter
	subl $2, %esi
	subl $16, %edi
.Lrefresh_4_2x:
	movb (%esi,%ecx), %al
	movb 1(%esi,%ecx), %bl
	movl (%ebp,%eax,4), %edx
	movl %edx, (%edi,%ecx,8)
	movl %edx, 4(%edi,%ecx,8)
	movl (%ebp,%ebx,4), %edx
	movl %edx, 8(%edi,%ecx,8)
	movl %edx, 12(%edi,%ecx,8)
	subl $2, %ecx
	jnz .Lrefresh_4_2x
	_leave




	.globl refrsh_1_3x
refresh_1_3x:	
	_enter
	leal (%ecx,%ecx,2), %edx
	shrl $1, %ecx
	addl %edx, %edi
	subl $2, %esi
.Lrefresh_1_3x:
	movb (%esi,%ecx,2), %al
	subl $6, %edi
	movb 1(%esi,%ecx,2), %bl
	movb (%ebp,%eax,2), %dl
	movb %dl, (%edi)
	movb %dl, 1(%edi)
	movb %dl, 2(%edi)
	movb (%ebp,%ebx,2), %dl
	movb %dl, 3(%edi)
	movb %dl, 4(%edi)
	movb %dl, 5(%edi)
	decl %ecx
	jnz .Lrefresh_1_3x
	_leave


	.globl refresh_2_3x
refresh_2_3x:
	_enter
	shll $1, %ecx
	addl %ecx, %edi
	addl %ecx, %edi
	addl %ecx, %edi
	shrl $2, %ecx
	subl $2, %esi
.Lrefresh_2_3x:
	movb (%esi,%ecx,2), %al
	subl $12, %edi
	movb 1(%esi,%ecx,2), %bl
	movw (%ebp,%eax,2), %dx
	movw %dx, (%edi)
	movw %dx, 2(%edi)
	movw %dx, 4(%edi)
	movw (%ebp,%ebx,2), %dx
	movw %dx, 6(%edi)
	movw %dx, 8(%edi)
	movw %dx, 10(%edi)
	decl %ecx
	jnz .Lrefresh_2_3x
	_leave



	.globl refresh_4_3x
refresh_4_3x:
	_enter
	shll $2, %ecx
	addl %ecx, %edi
	addl %ecx, %edi
	addl %ecx, %edi
	shrl $3, %ecx
	subl $2, %esi
.Lrefresh_4_3x:
	movb (%esi,%ecx,2), %al
	subl $24, %edi
	movb 1(%esi,%ecx,2), %bl
	movl (%ebp,%eax,4), %edx
	movl %edx, (%edi)
	movl %edx, 4(%edi)
	movl %edx, 8(%edi)
	movl (%ebp,%ebx,4), %edx
	movl %edx, 12(%edi)
	movl %edx, 16(%edi)
	movl %edx, 20(%edi)
	decl %ecx
	jnz .Lrefresh_4_3x
	_leave


	.globl refresh_4_4x
refresh_4_4x:
	_enter
	shll $4, %ecx
	addl %ecx, %edi
	shrl $5, %ecx
	subl $2, %esi
.Lrefresh_4_4x:
	movb (%esi,%ecx,2), %al
	subl $32, %edi
	movb 1(%esi,%ecx,2), %bl
	movl (%ebp,%eax,4), %edx
	movl %edx, (%edi)
	movl %edx, 4(%edi)
	movl %edx, 8(%edi)
	movl %edx, 12(%edi)
	movl (%ebp,%ebx,4), %edx
	movl %edx, 16(%edi)
	movl %edx, 20(%edi)
	movl %edx, 24(%edi)
	movl %edx, 28(%edi)
	decl %ecx
	jnz .Lrefresh_4_4x
	_leave






