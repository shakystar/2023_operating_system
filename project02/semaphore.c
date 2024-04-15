#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "spinlock.h"
#include "proc.h"

struct semaphore{
    int init;
    int count;
    struct proc *next;
};
    
struct spinlock semalock; // 어차피 malloc에서밖에 호출안함. 전역으로 합시다.

int sys_init_semaphore(void) {
  int count;
  struct semaphore* semaphore;

  if(argptr(0, (char**)&semaphore, sizeof(semaphore)) < 0 || argint(1, &count) < 0)
    return -1;

  init_semaphore(semaphore, count);
  return 0;
}

int sys_p_semaphore(void) {
  struct semaphore* semaphore;

  if(argptr(0, (char**)&semaphore, sizeof(semaphore)) < 0)
    return -1;

  P(semaphore);
  return 0;
}

int sys_v_semaphore(void) {
  struct semaphore* semaphore;

  if(argptr(0, (char**)&semaphore, sizeof(semaphore)) < 0)
    return -1;

  V(semaphore);
  return 0;
}

void semainit() {
  initlock(&semalock, "semaphore");
}

void init_semaphore(struct semaphore* semaphore, int count) {
    acquire(&semalock);
    if (semaphore->init) return;
    semaphore->init = 1;
    semaphore->count = count;
    semaphore->next = 0;
    release(&semalock);
}

void P(struct semaphore* semaphore) {
    acquire(&semalock);
    semaphore->count--;
    if (semaphore->count < 0) {
        struct proc *p = semaphore->next;
        while(p && p->next) p = p->next;
        if (p) p->next = myproc();
        else semaphore->next = myproc();
        sleep(myproc(), &semalock);
    }
    release(&semalock);
}

void V(struct semaphore* semaphore) {
    acquire(&semalock);
    semaphore->count++;
    if (semaphore->next) {
        struct proc *p = semaphore->next;
        semaphore->next = semaphore->next->next;
        p->next = 0;
        wakeup(p);
    }
    release(&semalock);
}