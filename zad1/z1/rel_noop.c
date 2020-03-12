void do_write(const char *str);

void f() {
	do_write("Hello, world!\n");
}

__asm__(
	".global _no_start\n"
	"_no_start:\n"
	"push %rdx\n"
	"push %rdx\n"
	"call f\n"
	"pop %rdx\n"
	"pop %rdx\n"
	"jmp orig_start\n"
);
