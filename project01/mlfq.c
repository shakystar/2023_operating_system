// mlfq.c - multi-level feedback queue

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "mlfq.h"

#define NNODE 64 // 310개 정도까지 해도 됨

queue* node_fool; // node memory fool

// region for node

void
node_push(queue* q, struct node* n) // for node fool
{
    if(q->count == 0)
    {
        q->front = n;
        q->rear = n;
    }
    else
    {
        q->rear->next = n;
        n->prev = q->rear;
        q->rear = n;
    }
    q->count++;
}

node* // to struct proc
node_pop(queue* q)
{
    if(q->count == 0) return 0;

    node *n = q->front;    

    if(q->front == q->rear)
    {
        q->front = 0;
        q->rear = 0;
    }
    else
    {
        q->front = n->next;
        q->front->prev = 0;
    }
    q->count--;
    return n;
}

void
node_set(queue* q)
{
    // alloc size is PGSIZE 4096
    node* nodes = (node*)kalloc();
    for (node* n = nodes; n < &nodes[NNODE]; n++)
        node_push(q, n);
}

void
node_init()
{
    node_fool = (queue*)kalloc();
    if (node_fool == 0) panic("node_fool init : kalloc fail\n");
    node_fool->front = 0;
    node_fool->rear = 0;
    node_fool->count = 0;
    node_set(node_fool);
}

node*
node_alloc(struct proc* p)
{
    // cprintf("node alloc\n");
    node* n = node_pop(node_fool);
    if (n == 0) 
    {
        node_set(node_fool);
        n = node_pop(node_fool);
    }
    if (n == 0) panic("node init : kalloc fail\n");
    n->proc = p;
    n->next = 0;
    n->prev = 0;
    return n;
}

void
node_free(node* n)
{
    node_push(node_fool, n);
}

// region for queue
// default - input back and output front

queue*
queue_init()
{
    queue* q = (queue*)kalloc();
    if (q == 0) panic("queue init : kalloc fail\n");
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    return q;
}

void
queue_push_front(queue* q, struct proc* p) // to struct proc
{
    node *n = node_alloc(p);

    if(q->count == 0)
    {
        q->front = n;
        q->rear = n;
    }
    else
    {
        n->next = q->front;
        q->front->prev = n;
        q->front = n;
    }
    q->count++;
}

void
queue_push(queue* q, struct proc* p) // to struct proc
{
    node *n = node_alloc(p);
    
    if(q->count == 0)
    {
        q->front = n;
        q->rear = n;
    }
    else
    {
        q->rear->next = n;
        n->prev = q->rear;
        q->rear = n;
    }
    q->count++;
}

struct proc* // to struct proc
queue_pop(queue* q)
{
    struct proc* p; // to struct proc
    node *n = q->front;
    
    if(n == 0) return 0;

    if(q->front == q->rear)
    {
        q->front = 0;
        q->rear = 0;
    }
    else
    {
        q->front = n->next;
        q->front->prev = 0;
    }
    q->count--;

    p = n->proc;
    node_free(n);
    return p;
}

struct proc*
queue_pop_by_pid(queue* q, int pid)
{
    struct proc* p;
    node *n = q->front; // curr node

    for (;;) {
        if (n == 0) break;
        if (pid == n->proc->pid) break;
        n = n->next;
    }

    if (n == 0) return 0;

    if (n->prev) n->prev->next = n->next;
    else if (q->front == n) q->front = n->next;
    
    if (n->next) n->next->prev = n->prev;
    else if (q->rear == n) q->rear = n->prev;
    
    q->count--;

    p = n->proc; // to struct proc
    node_free(n);
    return p;
}

int
queue_is_empty(queue* q)
{
    return q->count == 0;
}


// region for mlfq - multi level feedback queue

mlfq* 
mlfq_init()
{
    node_init(); // node memory fool setting

    mlfq* q = (mlfq*)kalloc();

    for (int i = 0; i < 2; i++)
        q->queues[i] = queue_init();

    for (int i = 0; i < 4; i++)
        q->pri_queues[i] = queue_init();

    return q;
}

void
mlfq_push_front(mlfq* q, struct proc* p, int flag)
{
    if (flag == 0) queue_push_front(q->queues[0], p);

    else if (p->level < 2) queue_push_front(q->queues[p->level], p);
    else queue_push_front(q->pri_queues[p->priority], p);
}

void
mlfq_push(mlfq* q, struct proc* p)
{
    if (p->time_quantum <= 0)
    {
        if (p->level < 2) p->level++;
        else if (p->priority > 0) p->priority--;
        p->time_quantum = 2 * p->level + 4;
    }

    if (p->level < 2) queue_push(q->queues[p->level], p);
    else queue_push(q->pri_queues[p->priority], p);
}

struct proc*
mlfq_pop(mlfq* q)
{
    for (int i = 0; i < 2; i++) {
        if (!queue_is_empty(q->queues[i])) {
            return queue_pop(q->queues[i]);
        }
    }
    for (int i = 0; i < 4; i++) { // 우선순위 높은 0번째 priority 부터
        if (!queue_is_empty(q->pri_queues[i])) {
            return queue_pop(q->pri_queues[i]);
        }
    }
    return 0;
}

// for setPriority()
struct proc*
mlfq_pop_target(mlfq* q, struct proc* p)
{
    if (p->level < 2) return 0; // 꺼낼 필요가 없음.
    return queue_pop_by_pid(q->pri_queues[p->priority], p->pid);
}

void
mlfq_boost(mlfq* q)
{
    for (int i = 0; i < 2; i++)
    for (node *c = q->queues[i]->front; c != 0; c = c->next) {
        c->proc->level = 0;
        c->proc->priority = 3;
        c->proc->time_quantum = 4;
    }

    for (int i = 0; i < 4; i++)
    for (node *c = q->pri_queues[i]->front; c != 0; c = c->next) {
        c->proc->level = 0;
        c->proc->priority = 3;
        c->proc->time_quantum = 4;
    }

    // from L1 to L0
    while (!queue_is_empty(q->queues[1])) {
        queue_push(q->queues[0], queue_pop(q->queues[1]));
    }
    // from L2 to L0
    // 최근에 실행된 애들은 더 뒤에 있음. 실행 안된 애들이 제일 앞. FCFS를 지키기 위해서 Priority 3부터 부스팅해야됨.
    for (int i = 3; i >= 0; i--)
    while (!queue_is_empty(q->pri_queues[i])) {
        queue_push(q->queues[0], queue_pop(q->pri_queues[i]));
    }
}