void fill(int *);

int tst[] = { 13, 31, 42 };
void f() {
	fill(tst);
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
