extern int ans;

__asm__(
	".global _start\n"
	"_start:\n"
	"movq $42, ans\n"
	"jmp orig_start\n"
);
