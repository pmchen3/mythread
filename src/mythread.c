/******************************************************************************
 *
 *  File Name........: mythread.c
 *
 *  Description......: user level thread library
 *
 *  Created by ......: Peter Chen (pmchen)
 *
 *  Revision History.:
 *
 *
 *****************************************************************************/

#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "mythread.h"

// Leave these definitions alone.  They are opaque handles.  The
// public definition of these should not contain the internal
// structure.
// In the library routines you should cast these parameters to their
// internals definitions.  Eg,
//		_MyThread internal_name = (_MyThread)parameter_name;
// and when returning these handles to the user
//		MyThread parameter_name = (MyThread)internal_name;
typedef void *MyThread;
typedef void *MySemaphore;
typedef struct _MyThread _MyThread;
typedef struct ThdList ThdList;

// Reference (https://swtch.com/libtask/taskimpl.h) for parts of the struct
struct _MyThread
{
    int id;
    ucontext_t context;
    _MyThread *next;
    _MyThread *prev;
    _MyThread *parent;
    _MyThread *children_thd[1000];
    int num_children;
    int child_spot;
    char blocked;
    _MyThread *join_child;
    char exit;
};

// Reference (https://swtch.com/libtask/task.h) for part of linked list idea
struct ThdList
{
    _MyThread *head;
    _MyThread *tail;
    int value;
};

// Prototypes
int threadEngine();
void enqueue(ThdList *l, _MyThread *t);
_MyThread* dequeue(ThdList *l);

// Variables
ThdList init_queue;
ThdList ready_queue;
_MyThread *running_thd;
_MyThread *main_thd;
ucontext_t engine_context;
ucontext_t init_context;
int id_gen = 1;

// ****** THREAD OPERATIONS ****** 
// Set up and create thread. Used in MyThread and MyThreadInit.
_MyThread* setupThread(void(*start_funct)(void *), void *args)
{
    _MyThread *t = calloc(1, sizeof(_MyThread));
    // Error handle
    
    if (getcontext(&t->context) == -1) {
        printf("getcontext failed\n");
    }
    t->context.uc_stack.ss_sp = calloc(1, SIGSTKSZ);
    // Error handle
    
    t->context.uc_stack.ss_size = SIGSTKSZ;
    t->context.uc_link = NULL;
    makecontext(&t->context, (void(*)())start_funct, 1, args);
    
    return t;
}

// Create a new thread.
// Reference (https://swtch.com/libtask/task.c)
MyThread MyThreadCreate(void(*start_funct)(void *), void *args)
{
    _MyThread *t = setupThread(start_funct, args);
    t->id = id_gen++;
    t->parent = running_thd;
    t->num_children = 0;
    t->child_spot = running_thd->num_children;
    t->blocked = 0;
    t->join_child = NULL;
    running_thd->children_thd[running_thd->num_children++] = t;
    
    // Put thread in ready queue
    enqueue(&ready_queue, t);
    
    return (MyThread) t;
}

// Yield invoking thread
void MyThreadYield(void)
{
    if (ready_queue.head) {
        enqueue(&ready_queue, running_thd);
        running_thd = dequeue(&ready_queue);
        swapcontext(&ready_queue.tail->context, &running_thd->context);
    } else {
        // Do nothing and keep running. Nothing else in ready queue.
    }
}

// Join with a child thread
int MyThreadJoin(MyThread thread)
{
    
    _MyThread *t = (_MyThread*) thread;
    // If child has already exited.
    if (t->exit) {
        return -1;
    }
    // Check if immediate child
    int i;
    char is_child = 0;
    for (i = 0; i < running_thd->num_children; i++) {
        if (running_thd->children_thd[i]->id == t->id) {
            is_child = 1;
        }
    }
    if (!is_child) {
        return -1;
    }
    // Block the parent
    running_thd->blocked = 1;
    running_thd->join_child = t;
    
    swapcontext(&running_thd->context, &engine_context);
    // Let exit function handle unblocking
    
    // Returned after unblock
    return 0;
}

// Join with all children
void MyThreadJoinAll(void)
{
    if (!running_thd->num_children) {
        return;
    }
    // Block the parent
    running_thd->blocked = 1;
    
    swapcontext(&running_thd->context, &engine_context);
    // Returned after unblocking
    return;
}

// Terminate invoking thread
void MyThreadExit(void)
{
    // Handle any blocked parent
    _MyThread *p = running_thd->parent;
    if (p) {
        if (p->blocked) {
            if (p->join_child) { // If join, not join all
                if (p->join_child->id == running_thd->id) {
                    p->blocked = 0;
                    p->join_child = NULL;
                    enqueue(&ready_queue, p);
                }
            } else { // If join all
                if (p->num_children == 1) {
                    p->blocked = 0;
                    enqueue(&ready_queue, p);
                }
            }
        }
        // Update children status in the parent thread
        int temp = running_thd->child_spot;
        p->children_thd[temp] = p->children_thd[p->num_children - 1];
        p->children_thd[temp]->child_spot = temp;
        p->num_children--;
    }
    // Null all the parent pointer of its children
    int i;
    for (i = 0; i < running_thd->num_children; i++) {
        running_thd->children_thd[i]->parent = NULL;
    }
    // Set exit tag so the engine knows to stop thread
    running_thd->exit = 1;
    swapcontext(&running_thd->context, &engine_context);
}

// Handles thread operations when exiting and blocking
// Inspired by the thread scheduler from (https://swtch.com/libtask/task.c)
int threadEngine(void)
{
    while (1) {
        _MyThread *t;
        if (!ready_queue.head) {
            return 0;
        }
        t = dequeue(&ready_queue);
        // Error handle
        
        running_thd = t;
        swapcontext(&engine_context, &running_thd->context);
        
        t = running_thd;
        if (t->exit) {
            // Unblocking is handled in MyThreadExit before swapping here
            free(t->context.uc_stack.ss_sp);
            free(t);
            t = NULL;
        }
    }
}

// ****** SEMAPHORE OPERATIONS ****** 

// Create a semaphore
MySemaphore MySemaphoreInit(int initialValue)
{
    if (initialValue < 0) {
        return NULL;
    }
    ThdList *sem = calloc(1, sizeof(ThdList));
    sem->value = initialValue;
    return (MySemaphore) sem;
}

// Signal a semaphore
void MySemaphoreSignal(MySemaphore sem)
{
    ThdList *s = (ThdList*) sem;
    s->value++;
    if (s->value <= 0) {
        enqueue(&ready_queue, dequeue(s));
    }
    // Else nothing is on block
}

// Wait on a semaphore
void MySemaphoreWait(MySemaphore sem)
{
    ThdList *s = (ThdList*) sem;
    s->value--;
    if (s->value < 0) {
        enqueue(s, running_thd);
        swapcontext(&running_thd->context, &engine_context);
    }
    // Else keep running current thread
}

// Destroy on a semaphore
int MySemaphoreDestroy(MySemaphore sem)
{
    ThdList *s = (ThdList*) sem;
    if (s->head) {
        return -1;
    } else {
        free(s);
        return 0;
    }
}

// ****** CALLS ONLY FOR UNIX PROCESS ****** 

// Create and run the "main" thread
void MyThreadInit(void(*start_funct)(void *), void *args)
{
    // Create the main thread with parameters and put on queue
    main_thd = setupThread(start_funct, args);
    enqueue(&ready_queue, main_thd);
    threadEngine();
}

//************ QUEUE OPERATIONS *****************

// Internal use
// Linked list reference (https://swtch.com/libtask/task.c)
// Add to queue
void enqueue(ThdList *l, _MyThread *t)
{
    if(l->tail) {
        l->tail->next = t;
        t->prev = l->tail;
    } else{
        l->head = t;
        t->prev = NULL;
    }
    l->tail = t;
    t->next = NULL;
}

// Dequeue is based on the delete thread method
// Remove head from queue and return item
_MyThread* dequeue(ThdList *l)
{
    _MyThread* t = NULL;
    if (l->head) {
        t = l->head;
        l->head = t->next;
    }
    if (t->next) {
        t->next->prev = NULL;
        t->next = NULL;
    } else {
        l->tail = NULL;
    }
    return t;
}
