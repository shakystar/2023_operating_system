#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 100000

#define NUM_THREAD 4
#define MAX_LEVEL 3

#define PASSWORD 2020003309

int parent;

int fork_children(int num)
{
  int i, p;
  for (i = 0; i < num; i++)
    if ((p = fork()) == 0)
    {
      sleep(10);
      return getpid();
    }
  return parent;
}

void exit_children()
{
  if (getpid() != parent)
    exit();
  while (wait() != -1);
}

void calculate()
{
  int i, pid = getpid();
  int count[MAX_LEVEL] = {0};

  uint tick0 = uptime();

  for (i = 0; i < NUM_LOOP; i++)
  {
    int x = getLevel();
    if (x < 0 || x > 2)
    {
      printf(1, "Wrong level: %d\n", x);
      exit();
    }
    count[x]++;
  }

  schedulerLock(PASSWORD);
  printf(1, "Process %d\n", pid);
  for (i = 0; i < MAX_LEVEL; i++)
    printf(1, "L%d: %d\n", i, count[i]);
  printf(1, "걸린 시간 : %d\n", uptime() - tick0);
  schedulerUnlock(PASSWORD);
}

int fork_test()
{
  parent = getpid();
  printf(1, "[Test 0] Fork Test started\n");  
  fork_children(100);
  if (getpid() != parent)
  {
    sleep(100);
  }
  exit_children();
  printf(1, "[Test 1] finished\n");
  printf(1, "done\n");
  return 0;
}

int scheduler_test()
{
  parent = getpid();
  int i, pid;
  int count[MAX_LEVEL] = {0};

  printf(1, "[Test 1] Scheduler Test started\n");
  fork_children(4);
  if (getpid() != parent)
  {
    uint tick0 = uptime();

    pid = getpid();
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getLevel();
      if (x < 0 || x > 2)
      {
        printf(1, "Wrong level: %d\n", x);
        exit();
      }
      count[x]++;
    }

    schedulerLock(PASSWORD);
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL; i++)
      printf(1, "L%d: %d\n", i, count[i]);
    printf(1, "걸린 시간 : %d\n", uptime() - tick0);
    schedulerUnlock(PASSWORD);
  }
  exit_children();
  printf(1, "[Test 1] finished\n");
  printf(1, "done\n");
  return 0;
}

int lock_test()
{
  parent = getpid();

  printf(1, "[Test 2] Lock Test 1 started\n");

  fork_children(1);
  if (getpid() != parent)
  {
    calculate();
    schedulerLock(123123);
    printf(1, "schedulerLock Not Kill!! Wrong....\n");
  }
  exit_children();

  fork_children(1);
  if (getpid() != parent)
  {
    schedulerLock(PASSWORD);
    calculate();
    schedulerUnlock(123123);
    printf(1, "schedulerUnlock Not Kill!! Wrong....\n");
  }
  exit_children();

  printf(1, "[Test 2] finished\n");
  printf(1, "done\n");
  return 0;
}

int lock_test2()
{
  parent = getpid();
  printf(1, "[Test 2] Lock Test 2 started\n");

  fork_children(5);
  if (getpid() == parent)
  {
    schedulerLock(PASSWORD);
    printf(1, "Parent %d Lock!!\n", getpid());

    fork_children(1);
    if(getpid() != parent)
    {
      sleep(40);
      printf(1, "MLFQ 동작\n");
    }
    else
    {
      printf(1, "Parent %d Sleep!!\n", getpid());
      sleep(200);
      printf(1, "Parent %d Unlock!!\n", getpid());
      schedulerUnlock(PASSWORD);
    }
  }
  else
  {
    sleep(25);
    schedulerLock(PASSWORD);
    printf(1, "Child %d Lock!!\n", getpid());
    sleep(50);
    printf(1, "Child %d Unlock!!\n", getpid());
    schedulerUnlock(PASSWORD);
  }
  exit_children();
  printf(1, "[Test 2] finished\n");
  printf(1, "done\n");
  return 0;
}

int lock_test3()
{
  int i, pid;
  int count[2] = {0};
  parent = getpid();

  printf(1, "[Test 2] Lock Test 3 started\n");
  fork_children(4);
  if (getpid() != parent)
  {
    pid = getpid();
    for (i=0;i<50;i++)
    {
      schedulerLock(PASSWORD);
      count[0]++;
      printf(1, "Lock PID : %d\n",pid);
      sleep(50);
      printf(1, "Unlock PID : %d\n",pid);
      count[1]++;
      schedulerUnlock(PASSWORD);
    }
    schedulerLock(PASSWORD);
    printf(1, "Process %d\n", pid);
    printf(1, "Lock Count: %d\n", count[0]);
    printf(1, "UnLk Count: %d\n", count[1]);
    schedulerUnlock(PASSWORD);
  }
  exit_children();
  printf(1, "[Test 2] finished\n");
  printf(1, "done\n");
  return 0;
}

int priority_test()
{
  int i;
  int count[MAX_LEVEL] = {0};

  parent = getpid();
  printf(1, "[Test 3] Priority Test 2 started\n");
  fork_children(4);

  if (getpid() != parent)
  {
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getLevel();
      count[x]++;

      if (i%(getpid()*getpid()) == 0)
      {
        setPriority(getpid(), 0);
      }
      if (i%(getpid()*10) == 0)
      {
        yield();
      }
    }
    printf(1, "Success %d!!\n", getpid());
  }


  exit_children();
  printf(1, "[Test 3] finished\n");
  printf(1, "done\n");
  return 0;
}

int main(int argc, char *argv[])
{
  // fork_test();
  // scheduler_test();
  // lock_test();
  // lock_test2();
  // lock_test3();
  // priority_test();

  // asm volatile("int $128");
  // asm volatile("int $129");

  exit();
}
