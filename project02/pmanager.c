// TODO : pmanager
#include "types.h"
#include "stat.h"
#include "user.h"

#define MAX_BUF 128
#define MAX_ARGS 4
#define MAX_ARG_LEN 64

// from string.c
char*
strncpy(char *s, const char *t, int n)
{
  char *os;

  os = s;
  while(n-- > 0 && (*s++ = *t++) != 0)
    ;
  while(n-- > 0)
    *s++ = 0;
  return os;
}

// shell에서 input buf 받아오기
int
getbuf(char *buf, int nbuf)
{
  printf(2, "> ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

// input buf 적절하게 split하기
int
splitstr(char *buf, char *cmd, char args[MAX_ARGS][MAX_ARG_LEN])
{
  memset(cmd, 0, MAX_ARG_LEN);
  memset(args, 0, MAX_ARGS * MAX_ARG_LEN * sizeof(char));

  int i, splitlen = -1, start = 0, maxlen = strlen(buf);

  for(i = 0; i < maxlen; i++) {
    if (buf[i] == ' ' || buf[i] == '\n') {
      if (splitlen >= MAX_ARGS) return -1;
      if (i - start >= MAX_ARG_LEN) return -1;

      if (splitlen < 0) {
        strncpy(cmd, buf + start, i - start);
      } else {
        strncpy(args[splitlen], buf + start, i - start);
      }
      splitlen++;
      start = i + 1;
    }
  }

  return splitlen;
}

// 해당 명령 실행
int
execcmd(char *cmd, char args[MAX_ARGS][MAX_ARG_LEN], int arglen)
{
  int ppid = getpid();

  if (!strcmp(cmd, "list")) {
    proclist();
  }
  else if (!strcmp(cmd, "kill")) {
    if (arglen < 1) return -1;

    int pid = atoi(args[0]);

    if (pid == ppid) {
      printf(1, "(pid:%d) process killed\n\n", pid);
      return 1;
    } else if (kill(pid) < 0) {
      printf(1, "(pid:%d) process not found\n\n", pid);
    } else {
      printf(1, "(pid:%d) process killed\n\n", pid);
    }
  }
  else if (!strcmp(cmd, "execute")) {
    if (arglen < 2) return -1;

    int stacksize = atoi(args[1]);
    char* path = args[0];
    char* argv[] = {args[0], 0};

    int pid = fork();
    if (pid == 0) { // child
      exec2(path, argv, stacksize); // TODO : 출력 string 처리
      printf(1, "(path:%s) execution failed\n\n", path);
    }
  }
  else if (!strcmp(cmd, "memlim")) {
    if (arglen < 2) return -1;

    int pid = atoi(args[0]);
    int limit = atoi(args[1]);

    if (setmemorylimit(pid, limit) < 0) {
       printf(1, "(pid:%d) memory limitation failed\n\n", pid);
    } else {
       printf(1, "(pid:%d) memory limitation success\n\n", pid);
    }
  } else if (!strcmp(cmd, "exit")) {
    return 1;
  } else {
    return -1;
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  static char buf[MAX_BUF];
  char args[MAX_ARGS][MAX_ARG_LEN];
  char cmd[MAX_ARG_LEN];
  int arglen = -1, flag = 0;

  for(;;) {
    if (getbuf(buf, sizeof(buf)) < 0) continue;

    if ((arglen = splitstr(buf, cmd, args)) < 0) {
      printf(1, "the input value is not in a valid format\n\n");
      continue;
    }

    if ((flag = execcmd(cmd, args, arglen)) < 0) {
      printf(1, "the input value is not in a valid format\n\n");
    } else if (flag) {
      break;
    }
  }
  printf(1, "exit\n");
  exit();
}
