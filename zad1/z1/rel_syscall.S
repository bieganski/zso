.text

msg:	.ascii "Hello, world!\n"
msg_end:

.global _start
_start:
	movq $1, %rax
	movq $1, %rdi
	lea msg(%rip), %rsi
	pushq %rdx
	movq $(msg_end - msg), %rdx
	syscall
	popq %rdx

	jmp orig_start
