#include "mylock.h"
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <semaphore.h>

/*
 * 初始化并创建一个命名信号量
 */
slock * lock_init(const char *name) {
    slock *sl;

    if ((sl = malloc(sizeof(slock))) == NULL) {
        return NULL;
    }
    strcpy(sl->name, name);

    if ((sl->semp = sem_open(sl->name, O_CREAT, 0644, 1)) == SEM_FAILED) {
        free(sp);
        return NULL;
    }

    return sl;
}

/*
 * 等待信号量
 * wait为0，表示非阻塞等待
 * wait为1，表示阻塞等待
 */
int p(slock *sl, int wait) {
    if (wait) {
        return sem_wait(sl->semp);
    } else {
        return sem_trywait(sl->semp);
    }
}

/*
 * 释放占用的信号量
 */
int v(slock *sl) {
    return sem_post(sl->semp);
}

/*
 * 关闭命名信号量
 * 和释放锁占用的堆空间
 */
void lock_close(slock *sl) {
    sem_close(sl->semp);
    sem_unlink(sl->name);
    free(sl);
}
