#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>

#define FOOTHREAD_THREADS_MAX 50
#define FOOTHREAD_DEFAULT_STACK_SIZE 2097152 // 2MB

#define FOOTHREAD_JOINABLE 1
#define FOOTHREAD_DETACHED 0

#define FOOTHREAD_ATTR_INITIALIZER { FOOTHREAD_DETACHED, FOOTHREAD_DEFAULT_STACK_SIZE }

typedef struct {
    int join_type;
    size_t stack_size;
} foothread_attr_t;

typedef struct {
    pid_t pid;
    pid_t tid;
} foothread_t;

typedef struct {
    foothread_t tinfo;
    int joinable;
    void* stack;
    int stack_size;
    int sems;
    int valid;
} table_entity;

#define SHM_KEY 9501
#define MUT_KEY 9500
#define MUTEX_VALUE 1
#define BARRIER_VALUE 0

// Define data type for mutex
typedef struct {
    int sem_id;
    int locked;
} foothread_mutex_t;

// Define data type for barrier
typedef struct {
    int sem_id;
    int count;
    int init_count;
} foothread_barrier_t;

// Function prototypes
void foothread_mutex_init(foothread_mutex_t *mutex);
void foothread_mutex_lock(foothread_mutex_t *mutex);
void foothread_mutex_unlock(foothread_mutex_t *mutex);
void foothread_mutex_destroy(foothread_mutex_t *mutex);

void foothread_barrier_init(foothread_barrier_t *barrier, int count);
void foothread_barrier_wait(foothread_barrier_t *barrier);
void foothread_barrier_destroy(foothread_barrier_t *barrier);

// Semaphore operations
void semaphore_wait(int sem_id);
void semaphore_signal(int sem_id);

void foothread_create(foothread_t *, foothread_attr_t *, int (*)(void *), void *);
void foothread_attr_setjointype(foothread_attr_t *, int);
void foothread_attr_setstacksize(foothread_attr_t *, size_t);
void foothread_exit();