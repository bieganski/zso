#include <stdio.h>

void fill(int *data) {
	for (int i = 0; i < 3; i++)
		data[i] = i;
}

int main() {
	printf("Main program [rw]\n");
	return 0;
}
