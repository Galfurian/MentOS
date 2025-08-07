// /bin/yieldtest
#include <stdio.h>
#include <unistd.h>

int yield(void); // forward declaration if using inline syscall

int main(void)
{
    for (int i = 0;; ++i) {
        printf("[yieldtest] Yielding %d\n", i);
        yield(); // Your system call
    }
    return 0;
}
