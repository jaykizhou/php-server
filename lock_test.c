#include "mylock.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    
    int pid;
    int i;

    // 初始化一个锁
    slock *sl;
    sl = lock_init("/test/lock");

    if ((pid = fork()) == 0) { // 子进程
        p(sl, 1);
        for (i = 0; i < 10; i++) {
            printf("----------------\n");
            fflush(stdout);
            sleep(rand() % 3);
            printf("----------------\n");
            fflush(stdout);
            sleep(rand() % 2);
        }
        v(sl);

    } else { // 父进程
        p(sl, 1);
        for (i = 0; i < 10; i++) {
            printf("++++++++++++++++\n");
            fflush(stdout);
            sleep(rand() % 3);
            printf("++++++++++++++++\n");
            fflush(stdout);
            sleep(rand() % 2);
        }
        v(sl);
    }

    return 0;
}
