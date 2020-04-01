
# Post-linker  
Program that allows you to append a code fragment from relocable binary to already linked ET_EXEC executable (in ELF format). Loads code and data to exec's virtual memory and resolves relocation.

Let's say we have executable (ET_EXEC type) file `exec.bin`
```C
#include <stdio.h>

void fill(int *data) {
    for (int i = 0; i < 3; i++)
        data[i] = i;
}

int main() {
    printf("Main program [rw]\n");
    return 0;
}
```

and we would like to hack it and run some code (maybe from `exec.bin`), and then jump into main to let it start it's own way.

We create relocatable ELF `rel.o`
```c
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
```

`orig_start` symbol represents oryginal ET_EXEC `_start ` symbol (a la `main`).

Case above is problematic, because `exec.bin` has already been linked. Using `postlinker` program may be helpful - it creates new executable, doing exactly what it is supposed to do.

### Build
```
make all
```

### Usage
```
./postlinker <ET_EXEC file> <ET_REL file> <target ET_EXEC file>
```



