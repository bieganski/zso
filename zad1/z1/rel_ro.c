void do_write(const char *str);

const char *init = "Hello world\n";
void f() {
	do_write(init);
}

__asm__(
	".global _start\n"
	"_start:\n"
	"push %rdx\n"
	"push %rdx\n"
	"call f\n"
	"pop %rdx\n"
	"pop %rdx\n"
	"jmp orig_start\n"
);
