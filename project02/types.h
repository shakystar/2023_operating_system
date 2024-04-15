typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;

typedef struct {
    int init;
    int count;
    void* next;
} Semaphore;