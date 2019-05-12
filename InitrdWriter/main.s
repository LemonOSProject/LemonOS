	.file	"main.cpp"
	.section	.rodata
	.type	_ZStL19piecewise_construct, @object
	.size	_ZStL19piecewise_construct, 1
_ZStL19piecewise_construct:
	.zero	1
	.section	.text._ZStorSt13_Ios_OpenmodeS_,"axG",@progbits,_ZStorSt13_Ios_OpenmodeS_,comdat
	.weak	_ZStorSt13_Ios_OpenmodeS_
	.type	_ZStorSt13_Ios_OpenmodeS_, @function
_ZStorSt13_Ios_OpenmodeS_:
.LFB985:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	movl	-4(%rbp), %eax
	orl	-8(%rbp), %eax
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE985:
	.size	_ZStorSt13_Ios_OpenmodeS_, .-_ZStorSt13_Ios_OpenmodeS_
	.local	_ZStL8__ioinit
	.comm	_ZStL8__ioinit,1,1
	.globl	version
	.data
	.align 16
	.type	version, @object
	.size	version, 16
version:
	.byte	108
	.byte	101
	.byte	109
	.byte	111
	.byte	110
	.byte	105
	.byte	110
	.byte	105
	.byte	116
	.byte	102
	.byte	115
	.byte	118
	.byte	49
	.byte	0
	.byte	0
	.byte	0
	.section	.text._ZN20lemoninitfs_header_tC2Ev,"axG",@progbits,_ZN20lemoninitfs_header_tC5Ev,comdat
	.align 2
	.weak	_ZN20lemoninitfs_header_tC2Ev
	.type	_ZN20lemoninitfs_header_tC2Ev, @function
_ZN20lemoninitfs_header_tC2Ev:
.LFB1503:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	movl	$0, (%rax)
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1503:
	.size	_ZN20lemoninitfs_header_tC2Ev, .-_ZN20lemoninitfs_header_tC2Ev
	.weak	_ZN20lemoninitfs_header_tC1Ev
	.set	_ZN20lemoninitfs_header_tC1Ev,_ZN20lemoninitfs_header_tC2Ev
	.section	.text._ZN18lemoninitfs_node_tC2Ev,"axG",@progbits,_ZN18lemoninitfs_node_tC5Ev,comdat
	.align 2
	.weak	_ZN18lemoninitfs_node_tC2Ev
	.type	_ZN18lemoninitfs_node_tC2Ev, @function
_ZN18lemoninitfs_node_tC2Ev:
.LFB1506:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	movw	$0, (%rax)
	movq	-8(%rbp), %rax
	movl	$0, 34(%rax)
	movq	-8(%rbp), %rax
	movl	$0, 38(%rax)
	movq	-8(%rbp), %rax
	movb	$0, 42(%rax)
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1506:
	.size	_ZN18lemoninitfs_node_tC2Ev, .-_ZN18lemoninitfs_node_tC2Ev
	.weak	_ZN18lemoninitfs_node_tC1Ev
	.set	_ZN18lemoninitfs_node_tC1Ev,_ZN18lemoninitfs_node_tC2Ev
	.section	.rodata
.LC0:
	.string	"Usage: "
	.align 8
.LC1:
	.string	" <output file> file1 file1_name [<file2> <file2name>] ..."
.LC2:
	.string	"Could not open file "
.LC3:
	.string	" for writing"
.LC4:
	.string	" for reading"
.LC5:
	.string	"Writing file at "
.LC6:
	.string	" with name "
.LC7:
	.string	" and size "
.LC8:
	.string	"B"
	.text
	.globl	main
	.type	main, @function
main:
.LFB1501:
	.cfi_startproc
	.cfi_personality 0x3,__gxx_personality_v0
	.cfi_lsda 0x3,.LLSDA1501
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbx
	subq	$1232, %rsp
	.cfi_offset 14, -24
	.cfi_offset 13, -32
	.cfi_offset 12, -40
	.cfi_offset 3, -48
	movl	%edi, -1252(%rbp)
	movq	%rsi, -1264(%rbp)
	movq	%fs:40, %rax
	movq	%rax, -40(%rbp)
	xorl	%eax, %eax
	cmpl	$3, -1252(%rbp)
	jg	.L6
	movq	-1264(%rbp), %rax
	movq	(%rax), %rbx
	movl	$.LC0, %esi
	movl	$_ZSt4cout, %edi
.LEHB0:
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movq	%rbx, %rsi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$.LC1, %esi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$1, %r13d
	jmp	.L22
.L6:
	leaq	-1088(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt14basic_ofstreamIcSt11char_traitsIcEEC1Ev
.LEHE0:
	movq	-1264(%rbp), %rax
	addq	$8, %rax
	movq	(%rax), %rcx
	leaq	-1088(%rbp), %rax
	movl	$4, %edx
	movq	%rcx, %rsi
	movq	%rax, %rdi
.LEHB1:
	call	_ZNSt14basic_ofstreamIcSt11char_traitsIcEE4openEPKcSt13_Ios_Openmode
	leaq	-1088(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt14basic_ofstreamIcSt11char_traitsIcEE7is_openEv
	xorl	$1, %eax
	testb	%al, %al
	je	.L8
	movq	-1264(%rbp), %rax
	addq	$8, %rax
	movq	(%rax), %rbx
	movl	$.LC2, %esi
	movl	$_ZSt4cout, %edi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movq	%rbx, %rsi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$.LC3, %esi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$2, %r13d
	movl	$0, %ebx
	jmp	.L9
.L8:
	leaq	-1120(%rbp), %rax
	movq	%rax, %rdi
	call	_ZN20lemoninitfs_header_tC1Ev
	leaq	-1120(%rbp), %rax
	addq	$4, %rax
	movl	$version, %esi
	movq	%rax, %rdi
	call	strcpy
	movl	-1252(%rbp), %eax
	movl	%eax, %edx
	shrl	$31, %edx
	addl	%edx, %eax
	sarl	%eax
	subl	$1, %eax
	movl	%eax, -1120(%rbp)
	movl	$20, %edx
	leaq	-1120(%rbp), %rcx
	leaq	-1088(%rbp), %rax
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	_ZNSo5writeEPKcl
	movl	$11008, %edi
	call	_Znam
	movq	%rax, %r14
	movq	%r14, %rax
	movl	$255, %ebx
	movq	%rax, %r12
.L11:
	cmpq	$-1, %rbx
	je	.L10
	movq	%r12, %rdi
	call	_ZN18lemoninitfs_node_tC1Ev
	addq	$43, %r12
	subq	$1, %rbx
	jmp	.L11
.L10:
	movq	%r14, -1208(%rbp)
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt13basic_fstreamIcSt11char_traitsIcEEC1Ev
.LEHE1:
	movl	$43, %eax
	sall	$8, %eax
	movl	$20, %edx
	addl	%edx, %eax
	movl	%eax, -1236(%rbp)
	movl	$0, -1232(%rbp)
.L13:
	movl	-1252(%rbp), %eax
	subl	$2, %eax
	cmpl	-1232(%rbp), %eax
	jle	.L12
	movl	-1232(%rbp), %eax
	movl	%eax, %edx
	shrl	$31, %edx
	addl	%edx, %eax
	sarl	%eax
	movslq	%eax, %rdx
	movq	%rdx, %rax
	salq	$2, %rax
	addq	%rdx, %rax
	salq	$2, %rax
	addq	%rdx, %rax
	addq	%rax, %rax
	addq	%rax, %rdx
	movq	-1208(%rbp), %rax
	addq	%rdx, %rax
	movq	%rax, -1200(%rbp)
	movl	$16, %esi
	movl	$8, %edi
	call	_ZStorSt13_Ios_OpenmodeS_
	movl	%eax, %esi
	movl	-1232(%rbp), %eax
	cltq
	addq	$2, %rax
	leaq	0(,%rax,8), %rdx
	movq	-1264(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rcx
	leaq	-576(%rbp), %rax
	movl	%esi, %edx
	movq	%rcx, %rsi
	movq	%rax, %rdi
.LEHB2:
	call	_ZNSt13basic_fstreamIcSt11char_traitsIcEE4openEPKcSt13_Ios_Openmode
	leaq	-576(%rbp), %rax
	movl	$0, %edx
	movl	$0, %esi
	movq	%rax, %rdi
	call	_ZNSi5seekgElSt12_Ios_Seekdir
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSi5tellgEv
	movq	%rax, -1184(%rbp)
	movq	%rdx, -1176(%rbp)
	leaq	-1184(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNKSt4fposI11__mbstate_tEcvlEv
	movl	%eax, -1224(%rbp)
	leaq	-576(%rbp), %rax
	movl	$2, %edx
	movl	$0, %esi
	movq	%rax, %rdi
	call	_ZNSi5seekgElSt12_Ios_Seekdir
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSi5tellgEv
	movq	%rax, -1168(%rbp)
	movq	%rdx, -1160(%rbp)
	leaq	-1168(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNKSt4fposI11__mbstate_tEcvlEv
	movl	%eax, -1220(%rbp)
	movl	-1224(%rbp), %eax
	subl	%eax, -1220(%rbp)
	leaq	-576(%rbp), %rax
	movl	$0, %edx
	movl	$0, %esi
	movq	%rax, %rdi
	call	_ZNSi5seekgElSt12_Ios_Seekdir
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt13basic_fstreamIcSt11char_traitsIcEE5closeEv
	movl	-1232(%rbp), %eax
	cltq
	addq	$3, %rax
	leaq	0(,%rax,8), %rdx
	movq	-1264(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rax
	movq	-1200(%rbp), %rdx
	addq	$2, %rdx
	movq	%rax, %rsi
	movq	%rdx, %rdi
	call	strcpy
	movq	-1200(%rbp), %rax
	movw	$-16657, (%rax)
	movq	-1200(%rbp), %rax
	movb	$1, 42(%rax)
	movq	-1200(%rbp), %rax
	movl	-1220(%rbp), %edx
	movl	%edx, 38(%rax)
	movl	-1220(%rbp), %eax
	addl	%eax, -1236(%rbp)
	addl	$2, -1232(%rbp)
	jmp	.L13
.L12:
	movl	$20, %edx
	leaq	-1136(%rbp), %rax
	movq	%rdx, %rsi
	movq	%rax, %rdi
	call	_ZNSt4fposI11__mbstate_tEC1El
	movq	-1136(%rbp), %rcx
	movq	-1128(%rbp), %rdx
	leaq	-1088(%rbp), %rax
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	_ZNSo5seekpESt4fposI11__mbstate_tE
	movl	$11008, %eax
	movq	%rax, %rdx
	movq	-1208(%rbp), %rcx
	leaq	-1088(%rbp), %rax
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	_ZNSo5writeEPKcl
	movl	$0, -1228(%rbp)
.L17:
	movl	-1252(%rbp), %eax
	subl	$2, %eax
	cmpl	-1228(%rbp), %eax
	jle	.L14
	movl	$16, %esi
	movl	$8, %edi
	call	_ZStorSt13_Ios_OpenmodeS_
	movl	%eax, %esi
	movl	-1228(%rbp), %eax
	cltq
	addq	$2, %rax
	leaq	0(,%rax,8), %rdx
	movq	-1264(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rcx
	leaq	-576(%rbp), %rax
	movl	%esi, %edx
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	_ZNSt13basic_fstreamIcSt11char_traitsIcEE4openEPKcSt13_Ios_Openmode
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt13basic_fstreamIcSt11char_traitsIcEE7is_openEv
	xorl	$1, %eax
	testb	%al, %al
	je	.L15
	movl	-1228(%rbp), %eax
	cltq
	addq	$2, %rax
	leaq	0(,%rax,8), %rdx
	movq	-1264(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rbx
	movl	$.LC2, %esi
	movl	$_ZSt4cout, %edi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movq	%rbx, %rsi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$.LC4, %esi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_, %esi
	movq	%rax, %rdi
	call	_ZNSolsEPFRSoS_E
	movl	$1, %r13d
	movl	$0, %ebx
	jmp	.L16
.L15:
	leaq	-576(%rbp), %rax
	movl	$0, %edx
	movl	$0, %esi
	movq	%rax, %rdi
	call	_ZNSi5seekgElSt12_Ios_Seekdir
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSi5tellgEv
	movq	%rax, -1152(%rbp)
	movq	%rdx, -1144(%rbp)
	leaq	-1152(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNKSt4fposI11__mbstate_tEcvlEv
	movl	%eax, -1216(%rbp)
	leaq	-576(%rbp), %rax
	movl	$2, %edx
	movl	$0, %esi
	movq	%rax, %rdi
	call	_ZNSi5seekgElSt12_Ios_Seekdir
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSi5tellgEv
	movq	%rax, -1136(%rbp)
	movq	%rdx, -1128(%rbp)
	leaq	-1136(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNKSt4fposI11__mbstate_tEcvlEv
	movl	%eax, -1212(%rbp)
	movl	-1216(%rbp), %eax
	subl	%eax, -1212(%rbp)
	leaq	-576(%rbp), %rax
	movl	$0, %edx
	movl	$0, %esi
	movq	%rax, %rdi
	call	_ZNSi5seekgElSt12_Ios_Seekdir
	movl	-1228(%rbp), %eax
	cltq
	addq	$2, %rax
	leaq	0(,%rax,8), %rdx
	movq	-1264(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %rbx
	movl	-1228(%rbp), %eax
	cltq
	addq	$3, %rax
	leaq	0(,%rax,8), %rdx
	movq	-1264(%rbp), %rax
	addq	%rdx, %rax
	movq	(%rax), %r12
	movl	$.LC5, %esi
	movl	$_ZSt4cout, %edi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movq	%r12, %rsi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$.LC6, %esi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movq	%rbx, %rsi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$.LC7, %esi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movq	%rax, %rdx
	movl	-1212(%rbp), %eax
	movl	%eax, %esi
	movq	%rdx, %rdi
	call	_ZNSolsEj
	movl	$.LC8, %esi
	movq	%rax, %rdi
	call	_ZStlsISt11char_traitsIcEERSt13basic_ostreamIcT_ES5_PKc
	movl	$_ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_, %esi
	movq	%rax, %rdi
	call	_ZNSolsEPFRSoS_E
	movl	-1212(%rbp), %eax
	movq	%rax, %rdi
	call	_Znam
	movq	%rax, -1192(%rbp)
	movl	-1212(%rbp), %edx
	movq	-1192(%rbp), %rcx
	leaq	-576(%rbp), %rax
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	_ZNSi4readEPcl
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt13basic_fstreamIcSt11char_traitsIcEE5closeEv
	movl	-1212(%rbp), %edx
	movq	-1192(%rbp), %rcx
	leaq	-1088(%rbp), %rax
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	_ZNSo5writeEPKcl
.LEHE2:
	movq	-1192(%rbp), %rax
	movq	%rax, %rdi
	call	_ZdlPv
	addl	$2, -1228(%rbp)
	jmp	.L17
.L14:
	movl	$1, %ebx
.L16:
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt13basic_fstreamIcSt11char_traitsIcEED1Ev
	cmpl	$1, %ebx
	je	.L30
	movl	$0, %ebx
	jmp	.L9
.L30:
	nop
	movl	$1, %ebx
.L9:
	leaq	-1088(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt14basic_ofstreamIcSt11char_traitsIcEED1Ev
	cmpl	$1, %ebx
	jne	.L22
	nop
	movl	$0, %r13d
.L22:
	movl	%r13d, %eax
	movq	-40(%rbp), %rcx
	xorq	%fs:40, %rcx
	je	.L25
	jmp	.L29
.L27:
	movq	%rax, %rbx
	leaq	-576(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt13basic_fstreamIcSt11char_traitsIcEED1Ev
	jmp	.L24
.L26:
	movq	%rax, %rbx
.L24:
	leaq	-1088(%rbp), %rax
	movq	%rax, %rdi
	call	_ZNSt14basic_ofstreamIcSt11char_traitsIcEED1Ev
	movq	%rbx, %rax
	movq	%rax, %rdi
.LEHB3:
	call	_Unwind_Resume
.LEHE3:
.L29:
	call	__stack_chk_fail
.L25:
	addq	$1232, %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1501:
	.globl	__gxx_personality_v0
	.section	.gcc_except_table,"a",@progbits
.LLSDA1501:
	.byte	0xff
	.byte	0xff
	.byte	0x1
	.uleb128 .LLSDACSE1501-.LLSDACSB1501
.LLSDACSB1501:
	.uleb128 .LEHB0-.LFB1501
	.uleb128 .LEHE0-.LEHB0
	.uleb128 0
	.uleb128 0
	.uleb128 .LEHB1-.LFB1501
	.uleb128 .LEHE1-.LEHB1
	.uleb128 .L26-.LFB1501
	.uleb128 0
	.uleb128 .LEHB2-.LFB1501
	.uleb128 .LEHE2-.LEHB2
	.uleb128 .L27-.LFB1501
	.uleb128 0
	.uleb128 .LEHB3-.LFB1501
	.uleb128 .LEHE3-.LEHB3
	.uleb128 0
	.uleb128 0
.LLSDACSE1501:
	.text
	.size	main, .-main
	.section	.text._ZNKSt4fposI11__mbstate_tEcvlEv,"axG",@progbits,_ZNKSt4fposI11__mbstate_tEcvlEv,comdat
	.align 2
	.weak	_ZNKSt4fposI11__mbstate_tEcvlEv
	.type	_ZNKSt4fposI11__mbstate_tEcvlEv, @function
_ZNKSt4fposI11__mbstate_tEcvlEv:
.LFB1604:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	movq	(%rax), %rax
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1604:
	.size	_ZNKSt4fposI11__mbstate_tEcvlEv, .-_ZNKSt4fposI11__mbstate_tEcvlEv
	.section	.text._ZNSt4fposI11__mbstate_tEC2El,"axG",@progbits,_ZNSt4fposI11__mbstate_tEC5El,comdat
	.align 2
	.weak	_ZNSt4fposI11__mbstate_tEC2El
	.type	_ZNSt4fposI11__mbstate_tEC2El, @function
_ZNSt4fposI11__mbstate_tEC2El:
.LFB1607:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-8(%rbp), %rax
	movq	-16(%rbp), %rdx
	movq	%rdx, (%rax)
	movq	-8(%rbp), %rax
	movl	$0, 8(%rax)
	movq	-8(%rbp), %rax
	movl	$0, 12(%rax)
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1607:
	.size	_ZNSt4fposI11__mbstate_tEC2El, .-_ZNSt4fposI11__mbstate_tEC2El
	.weak	_ZNSt4fposI11__mbstate_tEC1El
	.set	_ZNSt4fposI11__mbstate_tEC1El,_ZNSt4fposI11__mbstate_tEC2El
	.text
	.type	_Z41__static_initialization_and_destruction_0ii, @function
_Z41__static_initialization_and_destruction_0ii:
.LFB1778:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	cmpl	$1, -4(%rbp)
	jne	.L36
	cmpl	$65535, -8(%rbp)
	jne	.L36
	movl	$_ZStL8__ioinit, %edi
	call	_ZNSt8ios_base4InitC1Ev
	movl	$__dso_handle, %edx
	movl	$_ZStL8__ioinit, %esi
	movl	$_ZNSt8ios_base4InitD1Ev, %edi
	call	__cxa_atexit
.L36:
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1778:
	.size	_Z41__static_initialization_and_destruction_0ii, .-_Z41__static_initialization_and_destruction_0ii
	.type	_GLOBAL__sub_I_version, @function
_GLOBAL__sub_I_version:
.LFB1779:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	$65535, %esi
	movl	$1, %edi
	call	_Z41__static_initialization_and_destruction_0ii
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1779:
	.size	_GLOBAL__sub_I_version, .-_GLOBAL__sub_I_version
	.section	.init_array,"aw"
	.align 8
	.quad	_GLOBAL__sub_I_version
	.hidden	__dso_handle
	.ident	"GCC: (Ubuntu 5.4.0-6ubuntu1~16.04.10) 5.4.0 20160609"
	.section	.note.GNU-stack,"",@progbits
