#ifndef _MYLOCK_H__
#define _MYLOCK_H__

#define LOCK_NAME_LEN 80
typedef struct _slock {
    sem_t *semp;
    char name[LOCK_NAME_LEN];
} slock;

slock * lock_init(const char *name);
int p(slock *sl, int wait);
int v(slock *sl);
void lock_close(slock *sl);

#endif
