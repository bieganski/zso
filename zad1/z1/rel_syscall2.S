.text

msg:	.ascii "Hello, world!\n"
msg_end:

.global _start
_start:
	xor %eax, %eax
	movl $msg, %eax
	movq %rax, %rsi

	pushq %rdx
	movq $(msg_end - msg), %rdx

	movq $1, %rax
	movq $1, %rdi

	syscall
	popq %rdx

	jmp orig_start
