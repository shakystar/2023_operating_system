#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_THREAD 5

int status;
thread_t thread[NUM_THREAD];
int expected[NUM_THREAD];

int *ptr;

void failed()
{
  printf(1, "Test failed!\n");
  exit();
}

void create_all(int n, void *(*entry)(void *))
{
  int i;
  for (i = 0; i < n; i++) {
    if (thread_create(&thread[i], entry, (void *)i) != 0) {
      printf(1, "Error creating thread %d\n", i);
      failed();
    }
  }
}

void join_all(int n, int test)
{
  int i, retval;
  for (i = 0; i < n; i++) {
    if (thread_join(thread[i], (void **)&retval) != 0) {
      printf(1, "Error joining thread %d in test %d\n", i, test);
      failed();
    }
    if (retval != expected[i]) {
      printf(1, "Thread %d returned %d, but expected %d\n", i, retval, expected[i]);
      failed();
    }
  }
}

void *thread_recall_3(void *arg)
{
  int val = (int)arg;
  printf(1, "Thread 3-%d start\n", val);

  int i;

  if (val == 0) {
    ptr = (int *)malloc(65536);
    sleep(100);
    free(ptr);
    ptr = 0;
  }
  else {
    while (ptr == 0)
      sleep(1);
    for (i = 0; i < 16384; i++)
      ptr[i] = val;
  }

  while (ptr != 0)
    sleep(1);

  if (val == 0) {
    if(fork() == 0) {
      while(1){}
    }
    else {
    }
  }

  sleep(100);

  exit();

  printf(1, "Test 3-%d passed\n\n", i);

  thread_exit(arg);
  return 0;
}

void *thread_recall_2(void *arg)
{
  int val = (int)arg;
  printf(1, "Thread 2-%d start\n", val);

  int i;

  if (val == 0) {
    ptr = (int *)malloc(65536);
    sleep(100);
    free(ptr);
    ptr = 0;
  }
  else {
    while (ptr == 0)
      sleep(1);
    for (i = 0; i < 16384; i++)
      ptr[i] = val;
  }

  while (ptr != 0)
    sleep(1);

  printf(1, "Test 2-%d: recall 3 test\n", val);
  create_all(NUM_THREAD, thread_recall_3);
  join_all(NUM_THREAD, 2);
  printf(1, "Test 2-%d passed\n\n", val);

  thread_exit(arg);
  return 0;
}

void *thread_recall_1(void *arg)
{
  int val = (int)arg;
  printf(1, "Thread 1-%d start\n", val);

  int i;

  if (val == 0) {
    ptr = (int *)malloc(65536);
    sleep(100);
    free(ptr);
    ptr = 0;
  }
  else {
    while (ptr == 0)
      sleep(1);
    for (i = 0; i < 16384; i++)
      ptr[i] = val;
  }

  while (ptr != 0)
    sleep(1);

  printf(1, "Test 1-%d: recall 2 test\n", val);
  create_all(NUM_THREAD, thread_recall_2);
  join_all(NUM_THREAD, 1);
  printf(1, "Test 1-%d passed\n\n", val);

  thread_exit(arg);
  return 0;
}

int
main(int argc, char *argv[])
{
  int i;
  for (i = 0; i < NUM_THREAD; i++)
    expected[i] = i;

  for (i = 0; i < 100; i++) {
    printf(1, "Test %d: recall 1 test\n", i);
    create_all(NUM_THREAD, thread_recall_1);
    join_all(NUM_THREAD, 0);
    printf(1, "Test %d passed\n\n", i);
  }

  exit();
}
