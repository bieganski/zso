__asm__(
	".global _start\n"
	"_start:\n"
	"push %rdx\n"
	"push %rdx\n"
	"call some_func\n"
	"pop %rdx\n"
	"pop %rdx\n"
	"jmp orig_start\n"
);
