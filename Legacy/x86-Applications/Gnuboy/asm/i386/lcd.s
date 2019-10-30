
#include "asmnames.h"

	.set vram, lcd
	.set buf, scan+512
	.set pal1, scan+768
	.set pal2, scan+896
	.set pal4, scan+1024

	.set bg, scan
	.set buf, scan+512
	.set u, scan+1792+24
	.set v, scan+1792+28
	.set wx, scan+1792+32
	
	.data
	.balign 4


	.text
	.balign 32

debug:	.string "%08x\n"
	
	.macro _print arg=0
	pushf
	pusha
	movl \arg, %eax
	pushl %eax
	pushl $debug
	call printf
	addl $8, %esp
	popa
	popf
	.endm

	.macro _patexpand k=0
	movw (%esi,%ecx,2), %ax
	andl $(0x0101<<\k), %eax
	addb $0xff, %al
	sbbb %bl, %bl
	addb $0xff, %ah
	sbbb %bh, %bh
	andl $0x0201, %ebx
	orb %bh, %bl
	movb %bl, patpix+7-\k(%ebp,%ecx,8)
	.endm

	.macro _fastswap k=0
	movl patpix+(16*\k)(%ebp), %eax
	movl patpix+4+(16*\k)(%ebp), %ebx
	movl patpix+8+(16*\k)(%ebp), %ecx
	movl patpix+12+(16*\k)(%ebp), %edx
	bswap %eax
	bswap %ebx
	bswap %ecx
	bswap %edx
	movl %eax, patpix+1024*64+4+(16*\k)(%ebp)
	movl %ebx, patpix+1024*64+(16*\k)(%ebp)
	movl %ecx, patpix+1024*64+12+(16*\k)(%ebp)
	movl %edx, patpix+1024*64+8+(16*\k)(%ebp)
	.endm



	.globl updatepatpix
updatepatpix:
	movb anydirty, %al
	testb %al, %al
	jnz .Lupdatepatpix
	ret
	
.Lupdatepatpix:
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	
	movl $895, %edi
.Lmainloop:
	cmpl $511, %edi
	jnz .Lnoskip
	movl $383, %edi
.Lnoskip:	
	movb patdirty(%edi), %al
	testb %al, %al
	jnz .Lpatdirty
	decl %edi
	jnl .Lmainloop
	jmp .Lend
.Lpatdirty:
	movb $0, %al
	movb %al, patdirty(%edi)
	movl %edi, %eax
	movl $vram, %esi
	shll $4, %eax
	addl %eax, %esi

	movl $7, %ecx
	movl %edi, %ebp
	shll $6, %ebp
.Lexpandline:	
	_patexpand 0
	_patexpand 1
	_patexpand 2
	_patexpand 3
	_patexpand 4
	_patexpand 5
	_patexpand 6
	_patexpand 7
	decl %ecx
	jnl .Lexpandline

	_fastswap 0
	_fastswap 1
	_fastswap 2
	_fastswap 3

	movl patpix(%ebp), %eax
	movl patpix+4(%ebp), %ebx
	movl patpix+8(%ebp), %ecx
	movl patpix+12(%ebp), %edx
	movl %eax, patpix+2048*64+56(%ebp)
	movl %ebx, patpix+2048*64+60(%ebp)
	movl %ecx, patpix+2048*64+48(%ebp)
	movl %edx, patpix+2048*64+52(%ebp)
	movl patpix+16(%ebp), %eax
	movl patpix+20(%ebp), %ebx
	movl patpix+24(%ebp), %ecx
	movl patpix+28(%ebp), %edx
	movl %eax, patpix+2048*64+40(%ebp)
	movl %ebx, patpix+2048*64+44(%ebp)
	movl %ecx, patpix+2048*64+32(%ebp)
	movl %edx, patpix+2048*64+36(%ebp)
	movl patpix+32(%ebp), %eax
	movl patpix+36(%ebp), %ebx
	movl patpix+40(%ebp), %ecx
	movl patpix+44(%ebp), %edx
	movl %eax, patpix+2048*64+24(%ebp)
	movl %ebx, patpix+2048*64+28(%ebp)
	movl %ecx, patpix+2048*64+16(%ebp)
	movl %edx, patpix+2048*64+20(%ebp)
	movl patpix+48(%ebp), %eax
	movl patpix+52(%ebp), %ebx
	movl patpix+56(%ebp), %ecx
	movl patpix+60(%ebp), %edx
	movl %eax, patpix+2048*64+8(%ebp)
	movl %ebx, patpix+2048*64+12(%ebp)
	movl %ecx, patpix+2048*64(%ebp)
	movl %edx, patpix+2048*64+4(%ebp)

	movl patpix+1024*64(%ebp), %eax
	movl patpix+1024*64+4(%ebp), %ebx
	movl patpix+1024*64+8(%ebp), %ecx
	movl patpix+1024*64+12(%ebp), %edx
	movl %eax, patpix+3072*64+56(%ebp)
	movl %ebx, patpix+3072*64+60(%ebp)
	movl %ecx, patpix+3072*64+48(%ebp)
	movl %edx, patpix+3072*64+52(%ebp)
	movl patpix+1024*64+16(%ebp), %eax
	movl patpix+1024*64+20(%ebp), %ebx
	movl patpix+1024*64+24(%ebp), %ecx
	movl patpix+1024*64+28(%ebp), %edx
	movl %eax, patpix+3072*64+40(%ebp)
	movl %ebx, patpix+3072*64+44(%ebp)
	movl %ecx, patpix+3072*64+32(%ebp)
	movl %edx, patpix+3072*64+36(%ebp)
	movl patpix+1024*64+32(%ebp), %eax
	movl patpix+1024*64+36(%ebp), %ebx
	movl patpix+1024*64+40(%ebp), %ecx
	movl patpix+1024*64+44(%ebp), %edx
	movl %eax, patpix+3072*64+24(%ebp)
	movl %ebx, patpix+3072*64+28(%ebp)
	movl %ecx, patpix+3072*64+16(%ebp)
	movl %edx, patpix+3072*64+20(%ebp)
	movl patpix+1024*64+48(%ebp), %eax
	movl patpix+1024*64+52(%ebp), %ebx
	movl patpix+1024*64+56(%ebp), %ecx
	movl patpix+1024*64+60(%ebp), %edx
	movl %eax, patpix+3072*64+8(%ebp)
	movl %ebx, patpix+3072*64+12(%ebp)
	movl %ecx, patpix+3072*64(%ebp)
	movl %edx, patpix+3072*64+4(%ebp)

	decl %edi
	jnl .Lmainloop
.Lend:
	movb $0, %al
	movb %al, anydirty
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret



	.globl bg_scan_color
bg_scan_color:
	movb wx, %ch
	cmpb $0, %ch
	jb .Lbsc_done_nopop
	pushl %ebx
	pushl %ebp
	pushl %esi
	pushl %edi
	movl v, %eax
	movl $bg, %esi
	movl $buf, %edi
	leal patpix(,%eax,8), %ebp
	movl (%esi), %eax
	movl u, %ebx
	shll $6, %eax
	movb $-8, %cl
	addl %ebx, %eax
	addb %bl, %cl
	movb 4(%esi), %bl
	addl $8, %esi
	addb %cl, %ch
.Lbsc_preloop:	
	movb (%ebp,%eax), %dl
	incl %eax
	orb %bl, %dl
	movb %dl, (%edi)
	incl %edi
	incb %cl
	jnz .Lbsc_preloop
	cmpb $0, %ch
	jb .Lbsc_done
	subb $8, %ch
.Lbsc_loop:	
	movl (%esi), %eax
	movl 4(%esi), %edx
	shll $6, %eax
	movb %dl, %dh
	addl $8, %esi
	movl %edx, %ebx
	rorl $16, %edx
	orl %edx, %ebx
	movl (%ebp,%eax), %edx
	orl %ebx, %edx
	movl %edx, (%edi)
	movl 4(%ebp,%eax), %edx
	orl %ebx, %edx
	movl %edx, 4(%edi)
	addl $8, %edi
	subb $8, %ch
	jae .Lbsc_loop
	addb $8, %ch
	jz .Lbsc_done
	movl (%esi), %eax
	shll $6, %eax
	movb 4(%esi), %bl
.Lbsc_postloop:	
	movb (%ebp,%eax), %dl
	incl %eax
	orb %bl, %dl
	movb %dl, (%edi)
	incl %edi
	decb %ch
	jnz .Lbsc_postloop
.Lbsc_done:
	popl %edi
	popl %esi
	popl %ebp
	popl %ebx
.Lbsc_done_nopop:
	ret








	













