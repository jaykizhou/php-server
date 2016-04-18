#include "mylock.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    
    int pid;
    int i;

    // 初始化一个锁
    slock *sl;
    sl = lock_init("/test-lock-1");
    if (sl == NULL) {
        printf("lock_init error\n");
        return 0;
    }

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
        lock_close(sl);

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
        lock_close(sl);
        wait();
    }

    return 0;
}
