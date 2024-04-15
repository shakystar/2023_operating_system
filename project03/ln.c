#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int flag = 0;

  if(argc != 4){
    printf(2, "Usage: ln [-s|-h] [old] [new]\n");
    exit();
  }

  if (strcmp(argv[1], "-s") == 0) {
    flag = symlink(argv[2], argv[3]);
  } else if (strcmp(argv[1], "-h") == 0) {
    flag = link(argv[2], argv[3]);
  } else {
    printf(2, "Usage: ln [-s|-h] [old] [new]\n");
    exit();
  }

  if(flag < 0)
    printf(2, "link %s %s: failed\n", argv[2], argv[3]);
  exit();
}
