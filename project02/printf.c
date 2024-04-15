#include "types.h"
#include "stat.h"
#include "user.h"

#define BUFFER_SIZE 1024

static void
flush(char* buffer, int* buffer_index, 
      int fd)
{
  if (*buffer_index > 0) {
    write(fd, buffer, *buffer_index);
    *buffer_index = 0;
  }
}

static void
putc(char* buffer, int* buffer_index, 
     int fd, char c)
{
  if (*buffer_index >= BUFFER_SIZE)
    flush(buffer, buffer_index, fd);

  buffer[(*buffer_index)++] = c;
}

static void
printint(char* buffer, int* buffer_index, 
         int fd, int xx, int base, int sgn)
{
  static char digits[] = "0123456789ABCDEF";
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    putc(buffer, buffer_index, fd, buf[i]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
void
printf(int fd, const char *fmt, ...)
{
  char buffer[BUFFER_SIZE];
  int buffer_index = 0;

  char *s;
  int c, i, state;
  uint *ap;

  state = 0;
  ap = (uint*)(void*)&fmt + 1;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        putc(buffer, &buffer_index, fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(buffer, &buffer_index, fd, *ap, 10, 1);
        ap++;
      } else if(c == 'x' || c == 'p'){
        printint(buffer, &buffer_index, fd, *ap, 16, 0);
        ap++;
      } else if(c == 's'){
        s = (char*)*ap;
        ap++;
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          putc(buffer, &buffer_index, fd, *s);
          s++;
        }
      } else if(c == 'c'){
        putc(buffer, &buffer_index, fd, *ap);
        ap++;
      } else if(c == '%'){
        putc(buffer, &buffer_index, fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        putc(buffer, &buffer_index, fd, '%');
        putc(buffer, &buffer_index, fd, c);
      }
      state = 0;
    }
  }

  flush(buffer, &buffer_index, fd);
}
