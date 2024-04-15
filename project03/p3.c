#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define CHUNK_SIZE 512  // Number of bytes read or written at a time
#define SIXTY_FOUR_KB (64 * 1024)
#define FOUR_MB (4 * 1024 * 1024)
#define SIXTEEN_MB (16 * 1024 * 1024)
#define CHECKPOINT_INTERVAL (1024 * 1024)  // Checkpoint every 1MB

void create_and_fill_file(char* filename, int size) {
  printf(1, "Creating file %s of size %d bytes\n", filename, size);

  int fd = open(filename, O_CREATE | O_RDWR);
  if (fd < 0) {
    printf(1, "Couldn't create file %s\n", filename);
    exit();
  }

  char data[CHUNK_SIZE];
  memset(data, 'x', CHUNK_SIZE);
  
  int i;
  for(i = 0; i < size; i += CHUNK_SIZE) {
    if (size > CHECKPOINT_INTERVAL && i % CHECKPOINT_INTERVAL == 0) {
      printf(1, "Written %d bytes to %s\n", i, filename);
    }
    if (i + CHUNK_SIZE > size) {
      write(fd, data, size - i);  // Write the remaining bytes
    } else {
      write(fd, data, CHUNK_SIZE);
    }
    if (i%2 ==0) memset(data,'x',CHUNK_SIZE);
    else memset(data,'a',CHUNK_SIZE);
  }
  printf(1, "Finished writing to %s\n", filename);
  close(fd);
}

void read_file(char* filename, int size) {
  printf(1, "Reading file %s of size %d bytes\n", filename, size);

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    printf(1, "Couldn't open file %s\n", filename);
    exit();
  }

  char data[CHUNK_SIZE];

  int i;
  for(i = 0; i < size; i += CHUNK_SIZE) {
    if (size > CHECKPOINT_INTERVAL && i % CHECKPOINT_INTERVAL == 0) {
      printf(1, "Read %d bytes from %s\n", i, filename);
    }
    if (i + CHUNK_SIZE > size) {
      read(fd, data, size - i);  // Read the remaining bytes
    } else {
      read(fd, data, CHUNK_SIZE);
    }
  }
  printf(1, "Finished reading from %s\n", filename);
  close(fd);
}

int main(int argc, char *argv[]) {
  create_and_fill_file("smallfile", 512*30);
  if(argc < 2) exit();
  else sync();
  exit();
}

