#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct spinlock tsynclock;
static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  initlock(&tsynclock, "tsync");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz, retsz;
  struct proc *curproc = myproc();

  acquire(&tsynclock);

  retsz = sz = curproc->sz;

  if (curproc->szlim && sz + n > curproc->szlim)
    goto bad;

  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      goto bad;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      goto bad;
  }

  curproc->sz = sz;

  tsync(curproc);

  release(&tsynclock);

  switchuvm(curproc);
  return retsz;

bad:
  release(&tsynclock);
  return -1;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc(); // curproc = 부모 프로세스

  // Allocate process.
  if((np = allocproc()) == 0){ // proc array에서 자식 프로세스 할당
    return -1;
  }

  acquire(&tsynclock);

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    release(&tsynclock);
    return -1;
  }
  np->sz = curproc->sz; // 프로세스 사이즈

  release(&tsynclock);

  *np->tf = *curproc->tf; // Trap frame for current syscall 이라네요...
  np->parent = curproc; // 부모
  
  np->tid = 0;
  np->spgn = curproc->spgn; // 스택 사이즈
  np->tmain = 0;
  np->retval = 0;
  // 기본적으로 kstack이 할당된 상태

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0; // np의 반환값이 0인 이유

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]); // 파일 테이블 복사
  np->cwd = idup(curproc->cwd); // 작업 디렉토리 정보 복사

  safestrcpy(np->name, curproc->name, sizeof(curproc->name)); // process 이름 복사

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

void
exit_basic(struct proc *curproc)
{
  int fd;
  struct proc *p;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();

  tclear();

  exit_basic(curproc);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent); // tclear 때문에 홀로 정리되면 됨.

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}


void
freeproc(struct proc *p, int flag)
{
  kfree(p->kstack);
  p->kstack = 0;

  if (flag) freevm(p->pgdir);

  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->killed = 0;

  p->spgn = 0;
  p->retval = 0;
  p->tid = 0;
  p->tmain = 0;
  p->next = 0;

  p->state = UNUSED;
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        freeproc(p, 1);
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}


//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// TODO : setmemorylimit
int 
setmemorylimit(int pid, int lim)
{
  struct proc *p;

  acquire(&tsynclock);

  if (lim < 0) goto bad;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->pid == pid)
      goto limit;
  goto bad;

limit:
  if (p->sz >= lim) 
    goto bad;
  p->szlim = lim;

  tsync(p);
  release(&tsynclock);
  return 0;

bad:
  release(&tsynclock);
  return -1;
}

#define DSTLEN 18

int 
intlen(int number) 
{
    int count = 0;
    if (number == 0) return 1;

    while (number != 0) {
        number /= 10;
        count++;
    }
    return count;
}

char*
intToStr(int n, char* s)
{
    int l = intlen(n);
    s[l] = '\0';
    while (l > 0) {
        s[l - 1] = '0' + (n % 10);
        n /= 10;
        l--;
    }
    return s;
}

void
padset(char* dst, char* src, char pad)
{
  strncpy(dst, src, strlen(src));
  memset(dst + strlen(src), pad, DSTLEN - strlen(src));
}

// TODO : process list print
// 성능을 포기하고 만든 낭만.. 코드는 더럽다는걸 알고 있지만...!
void
proclist(void)
{
  struct proc *p;

  char namestr[DSTLEN + 1] = {0};
  char pidstr[DSTLEN + 1] = {0};
  char spstr[DSTLEN + 1] = {0};
  char szstr[DSTLEN + 1] = {0};
  char szlimstr[DSTLEN + 1] = {0};
  
  padset(namestr, "name", ' ');
  padset(pidstr, "pid", ' ');
  padset(spstr, "stack pagenum", ' ');
  padset(szstr, "mem size", ' ');
  padset(szlimstr, "mem size limit", ' ');
  cprintf("| %s | %s | %s | %s | %s |\n", namestr, pidstr, spstr, szstr, szlimstr);

  padset(namestr, "", '-');
  padset(pidstr, "", '-');
  padset(spstr, "", '-');
  padset(szstr, "", '-');
  padset(szlimstr, "", '-');
  cprintf("| %s | %s | %s | %s | %s |\n", namestr, pidstr, spstr, szstr, szlimstr);
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNING &&
         p->state != RUNNABLE && 
         p->state != SLEEPING)
        continue;
      if(p->tid) continue;

      padset(namestr, p->name, ' ');
      padset(pidstr, intToStr(p->pid, pidstr), ' ');
      padset(spstr, intToStr(p->spgn, spstr), ' ');
      padset(szstr, intToStr(p->sz, szstr), ' ');
      padset(szlimstr, intToStr(p->szlim, szlimstr), ' ');
      cprintf("| %s | %s | %s | %s | %s |\n", namestr, pidstr, spstr, szstr, szlimstr);
  }
  cprintf("\n");
}

struct proc*
tmainget(struct proc *p)
{
  return p->tmain ? p->tmain : p;
}

int
tcreate(thread_t *thread, void *(*start_routine)(void *), void *argv)
{
  int i;
  struct proc *np;
  struct proc *curproc = myproc();

  uint sz, sp, ustack[3+MAXARG+1];

  // alloc proccess, same pid
  if((np = allocproc()) == 0)
   return -1;
  np->tid = np->pid;
  np->pid = curproc->pid;

  // clone : copy state
  np->pgdir = curproc->pgdir;
  np->spgn = 1;
  *np->tf = *curproc->tf;
  np->parent = curproc->parent;

  np->tmain = tmainget(curproc);

  // set personal stack
  acquire(&tsynclock);

  sz = curproc->sz;

  if (curproc->szlim && sz + 2*PGSIZE > curproc->szlim)
    goto bad;

  sz = PGROUNDUP(sz);
  if((sz = allocuvm(np->pgdir, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  clearpteu(np->pgdir, (char*)(sz - 2*PGSIZE));

  np->sz = sz;
  np->szlim = curproc->szlim;

  tsync(np);

  release(&tsynclock);

  sp = sz;
  ustack[0] = 0xffffffff;
  ustack[1] = (uint)argv; // void*의 주소값으 arg로 보내기 때문에 복사가 아닌, 참조로 들어간다.

  sp -= 2 * 4;
  if(copyout(np->pgdir, sp, ustack, 2 * 4) < 0)
    goto bad;

  // copy file, cwd
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  // setting np info
  np->tf->eip = (uint)start_routine; // start_routine
  np->tf->esp = sp;
  np->tf->eax = 0; // returns 0 in the child.

  safestrcpy(np->name, curproc->name, sizeof(np->name));

  *thread = np->tid;

  switchuvm(curproc);

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return 0;

 bad:
  if (holding(&tsynclock))
    release(&tsynclock);
  kfree(np->kstack);
  np->kstack = 0;
  np->state = UNUSED;
  return -1;
}

void
texit(void *retval)
{
  struct proc *curproc = myproc();

  if (retval) curproc->retval = retval;

  exit_basic(curproc);

  // tmain might be sleeping in wait().
  wakeup1(curproc->tmain);

  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

int
twait(thread_t thread, void **retval)
{
  struct proc *p;
  struct proc *curproc = myproc();
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->tid == thread){
      break;
    }
  }
  if (p >= &ptable.proc[NPROC])
    return -1;

  acquire(&ptable.lock);
  for(;;){
    if(p->state == ZOMBIE){
      *retval = p->retval;
      freeproc(p, 0);
      release(&ptable.lock);
      return 0;
    }

    // No point waiting
    if(p->tmain != curproc || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.
    sleep(curproc, &ptable.lock);
  }
}

// 현재 프로세스를 제외하고, 관련된 스레드를 정리합니다.
int
tclear(void)
{
  struct proc *p;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->pid == curproc->pid && p != curproc){
      p->state = CLEANING;
      release(&ptable.lock);
      exit_basic(p);
      freeproc(p, 0);
    }

  release(&ptable.lock);

  curproc->tid = 0;
  curproc->tmain = 0;

  return 0;
}

// must tsynclock
void
tsync(struct proc *curproc)
{
  struct proc *p;
  // struct proc *tmain = tmainget(curproc);
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->pid == curproc->pid) {
      p->sz = curproc->sz;
      p->szlim = curproc->szlim;
    }
}