#include <stdio.h>
#include <unistd.h>>
// #include <uapi.asm.unistd_64.h>
/*
ssize_t read (int fd_, void *ptr_, size_t len_) {
    register int sys_nr __asm__("eax") = __NR_read;
    register ssize_t res __asm__("rax");
    register int fd __asm__("edi") = fd_;
    register void *ptr __asm__("rsi") = ptr_;
    register size_t len __asm__("rdx") = len_;
    __asm__ volatile (
        "syscall"
        : "=g"(res)
        : "g"(sys_nr), "g"(fd), "g"(ptr), "g"(len)
        : "cc", "rcx", "r11", "memory"
    );
    return res;
}
*/

int fun() {
    register int eax __asm__("eax");
    register int ebx __asm__("ebx");
    register int ecx __asm__("ecx");
    register int edx __asm__("edx");
    int ebx_ = ebx;
    int ecx_ = ecx;
    int edx_ = edx;
    char str[13] = {0};
    __asm__ volatile (
        "cpuid"
        : "=g"(ebx), "=g"(ecx), "=g"(edx)
        : "g"(eax)

    );
    memcpy (str, &ebx_, sizeof ebx_);
    memcpy (str + 4, &ecx_, sizeof ebx_);
    memcpy (str + 8, &edx_, sizeof ebx_);
    printf("%s\n", str);
    return 0;
}


int main() {
    int a = 1, b = 2, c = 3;
    

// a = b + c;
    fun();
__asm__ volatile (
    "addl %1, %0"
    : "=g"(a)
    : "gm"(b), "0"(c)
    : "cc"
);

    return 0;
}
