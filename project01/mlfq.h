typedef struct node {
    struct proc* proc; // to struct proc
    struct node* prev;
    struct node* next;
} node;

typedef struct queue {
    node* front;
    node* rear;
    int count;
} queue;

typedef struct mlfq {
    queue* queues[2]; // L0, L1
    queue* pri_queues[4]; // L2 priority 구현
} mlfq;


mlfq* mlfq_init();

void mlfq_push_front(mlfq* q, struct proc* p, int flag);

void mlfq_push(mlfq* q, struct proc* p);

void mlfq_boost(mlfq* q);

struct proc* mlfq_pop(mlfq* q);

struct proc* mlfq_pop_target(mlfq* q, struct proc* p);