#include "foothread.h"

int mutex_created = -1;
void foothread_create(foothread_t *thread, foothread_attr_t *attr, int (*start_routine)(void *), void *arg) {
    void *stack;
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_THREAD | CLONE_SIGHAND | SIGCHLD;
    int ret;
    int mutex = semget(MUT_KEY+getpid(), 1, 0777|IPC_CREAT);
    if (mutex_created == -1){
        semctl(mutex, 0, SETVAL, 1);
        mutex_created = 1;
    }

    struct sembuf mtx_op;
    mtx_op.sem_flg = 0;
    mtx_op.sem_num = 0;
    int shm_table = shmget(SHM_KEY+getpid(), FOOTHREAD_THREADS_MAX*sizeof(table_entity), 0777|IPC_CREAT);

    if (attr == NULL) {
        attr = &(foothread_attr_t)FOOTHREAD_ATTR_INITIALIZER;
    }
    if (attr->join_type == FOOTHREAD_DETACHED){
        flags = flags | CLONE_PARENT;
    }

    if (attr->stack_size == 0) {
        attr->stack_size = FOOTHREAD_DEFAULT_STACK_SIZE;
    }

    // Allocate memory for the stack
    
    mtx_op.sem_op = -1;
    semop(mutex, &mtx_op, 1);   // wait

    // entry into the table
    
    table_entity* table = shmat(shm_table, 0, 0);
    int i=0;
    for(i=0; i<FOOTHREAD_THREADS_MAX; i++){
        if (table[i].valid != 1){
            // empty entry can be filled
            // Create a new thread using clone()
            stack = malloc(attr->stack_size);
            if (stack == NULL) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            ret = clone(start_routine, stack + attr->stack_size, flags, arg);
            if (ret == -1) {
                perror("clone");
                free(stack);
                exit(EXIT_FAILURE);
            }

            // Fill in thread details
            if (thread != NULL) {
                thread->pid = getpid();
                thread->tid = ret;
            }
            table[i].stack = stack;
            table[i].tinfo = *thread;
            table[i].valid = 1;
            table[i].joinable = attr->join_type;
            // for(int j=0; j<20; j++){
            //     table[i].sems[j] = -1;
            // }
            table[i].sems = semget(thread->tid, 1, 0777|IPC_CREAT);
            semctl(table[i].sems, 0, SETVAL, 0);
            table[i].stack_size = attr->stack_size;
            break;
        }
    }
    if (i == FOOTHREAD_THREADS_MAX){
        // no entry available
        printf("No entry for new thread\n");
    }

    mtx_op.sem_op = 1;
    semop(mutex, &mtx_op, 1);   // signal
}

void foothread_attr_setjointype(foothread_attr_t *attr, int join_type) {
    attr->join_type = join_type;
}

void foothread_attr_setstacksize(foothread_attr_t *attr, size_t stack_size) {
    attr->stack_size = stack_size;
}

void foothread_exit(){
    int mutex = semget(MUT_KEY+getpid(), 1, 0777|IPC_CREAT);
    int shm_table = shmget(SHM_KEY+getpid(), FOOTHREAD_THREADS_MAX*sizeof(table_entity), 0777|IPC_CREAT);

    semaphore_wait(mutex);
    table_entity* table = shmat(shm_table, 0, 0);
    if ( gettid()==getpid()){
        // leader thread has to wait
        for(int i=0; i<FOOTHREAD_THREADS_MAX; i++){
            if ( table[i].valid== 1 && table[i].joinable==1 && table[i].tinfo.pid==getpid()){
                // wait for this thread
                // printf("%d thread waiting\n", table[i].tinfo.tid);
                semaphore_signal(mutex);
                semaphore_wait(table[i].sems);
                semaphore_wait(mutex);
                // printf("%d thread finished\n", table[i].tinfo.tid);
                table[i].valid = 0;
                free(table[i].stack);
            }
        }
    }
    else {
        // followers
        for(int i=0; i<FOOTHREAD_THREADS_MAX; i++){
            if ( table[i].valid == 1 && table[i].tinfo.tid==gettid() ){
                // signal
                if (table[i].joinable==0){
                    //detatched
                    table[i].valid = 0;
                }
                // printf("%d thread ended\n", table[i].tinfo.tid);
                semaphore_signal(table[i].sems);
                break;
            }
        }
    }
    
    semaphore_signal(mutex);
}


// Mutex functions
void foothread_mutex_init(foothread_mutex_t *mutex) {
    mutex->sem_id = semget(4000+gettid(), 1, IPC_CREAT | 0666);
    semctl(mutex->sem_id, 0, SETVAL, MUTEX_VALUE);
    mutex->locked = 0;
}

void foothread_mutex_lock(foothread_mutex_t *mutex) {
    semaphore_wait(mutex->sem_id);
    mutex->locked = 1;
}

void foothread_mutex_unlock(foothread_mutex_t *mutex) {
    if (!mutex->locked) {
        fprintf(stderr, "Error: Attempt to unlock an unlocked mutex\n");
        exit(EXIT_FAILURE);
    }
    mutex->locked = 0;
    semaphore_signal(mutex->sem_id);
}

void foothread_mutex_destroy(foothread_mutex_t *mutex) {
    semctl(mutex->sem_id, 0, IPC_RMID);
}
// Barrier functions
void foothread_barrier_init(foothread_barrier_t *barrier, int count) {
    barrier->sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (barrier->sem_id == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    barrier->count = count;
    barrier->init_count = count;
    semctl(barrier->sem_id, 0, SETVAL, 0); 
}

void foothread_barrier_wait(foothread_barrier_t *barrier) {
    barrier->count--;
    if (barrier->count == 0) {
        // If all threads have arrived, release all waiting threads
        for (int i = 0; i < barrier->init_count - 1; i++) {
            semaphore_signal(barrier->sem_id);
        }
        barrier->count = barrier->init_count; 
    } else {
        semaphore_wait(barrier->sem_id); // Wait until all threads have arrived
    }
}

void foothread_barrier_destroy(foothread_barrier_t *barrier) {
    semctl(barrier->sem_id, 0, IPC_RMID); 
}


void semaphore_wait(int sem){
    struct sembuf sem_op;
    sem_op.sem_flg = 0;
    sem_op.sem_num = 0;
    sem_op.sem_op = -1;
    semop(sem, &sem_op, 1);
}

void semaphore_signal(int sem){
    struct sembuf sem_op;
    sem_op.sem_flg = 0;
    sem_op.sem_num = 0;
    sem_op.sem_op = 1;
    semop(sem, &sem_op, 1);
}