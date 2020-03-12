#include <stdio.h>

void do_write(const char *str) {
	printf("%s", str);
}

int main() {
	do_write("Main program [noop]\n");
	return 0;
}
