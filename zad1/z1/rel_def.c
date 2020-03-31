void fill(int *);

int tst[3];
void f() {
	fill(tst);
    printf("%d,%d,%d", tst[0], tst[1], tst[2]); // TODO wywalić
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
