#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "foothread.h"

#define MAX_NODES 500
#define INPUT_PROMPT "Enter a positive integer: "

int n; 
int parent[MAX_NODES]={-1};

typedef struct {
    int sum;
    int children_count;
    int children[MAX_NODES];
    foothread_mutex_t mutex_id;
    foothread_barrier_t barrier_id;
} NodeInfo;

NodeInfo nodes[MAX_NODES];

// Function prototypes
int node_thread(void *arg);
void initialize_tree();
void read_tree(char *filename);
void create_threads();
void cleanup_resources();
void process_input(int node_id);
void compute_partial_sum(int node_id);
void print_total_sum();
int root_node = -1;

int mutex;
// Main function
int main() {
    mutex = semget(getpid(), 1, 0777|IPC_CREAT);
    semctl(mutex, 0, SETVAL, 1);
    read_tree("tree.txt");
    initialize_tree();
    create_threads();
    
    foothread_exit();
    print_total_sum();
    cleanup_resources();

    return EXIT_SUCCESS;
}


// Read tree structure from file
void read_tree(char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    fscanf(file, "%d", &n);
    int node=0;
    // printf("n = %d\n", n);
    for (int i = 0; i < n; i++) {
        fscanf(file, "%d", &node);
        fscanf(file, "%d", &parent[node]);
        if (node==parent[node]) root_node=node;
        else {
            nodes[parent[node]].children[nodes[parent[node]].children_count]=node;
            nodes[parent[node]].children_count++;
        }
    }

    fclose(file);
}

// Initialize tree nodes
void initialize_tree() {
    for (int i = 0; i < n; i++) {
        nodes[i].sum = 0;
        foothread_mutex_init(&nodes[i].mutex_id);
        foothread_barrier_init(&nodes[i].barrier_id, nodes[i].children_count+1);
    }
}
// Create threads for each node
void create_threads() {
    foothread_t threads[n];
    foothread_attr_t attr;
    foothread_attr_setjointype(&attr, FOOTHREAD_JOINABLE);
    foothread_attr_setstacksize(&attr, 0);
    for (int i = 0; i < n; i++) {
        int *node_id = malloc(sizeof(int));
        *node_id = i;
        foothread_create(&threads[i], &attr, node_thread, (void *) node_id);
    }
}


// Thread function for each node
int node_thread(void *arg) {
    int node_id = *((int *) arg);

    if (nodes[node_id].children_count == 0) {
        // Leaf node: Process input
        process_input(node_id);
        //printf("leaf Node_id = %d\n", node_id);
    } else {
        // Internal node: Compute partial sum
        compute_partial_sum(node_id);
        //printf("Node_id = %d\n", node_id);
    }
    foothread_exit();
    return 0;
}
// Process user input at leaf nodes
void process_input(int node_id) {
    int input;
    semaphore_wait(mutex);
    printf("Leaf node %d\t :: %s", node_id, INPUT_PROMPT);
    scanf("%d", &input);
    semaphore_signal(mutex);
    foothread_mutex_lock(&nodes[parent[node_id]].mutex_id);
    nodes[node_id].sum = input;
    nodes[parent[node_id]].sum += input;
    foothread_mutex_unlock(&nodes[parent[node_id]].mutex_id);
    foothread_barrier_wait(&nodes[parent[node_id]].barrier_id);
}

// Compute partial sum at internal nodes
void compute_partial_sum(int node_id) {
    int parent_id = parent[node_id];

    foothread_barrier_wait(&nodes[node_id].barrier_id);
    semaphore_wait(mutex);
    printf("Internal node  %d gets the partial sum %d from its children\n", node_id, nodes[node_id].sum);
    semaphore_signal(mutex);
    foothread_mutex_lock(&nodes[parent_id].mutex_id);
    if (node_id!=root_node) nodes[parent_id].sum += nodes[node_id].sum;
    foothread_mutex_unlock(&nodes[parent_id].mutex_id);
    if (node_id!=root_node)foothread_barrier_wait(&nodes[parent[node_id]].barrier_id);
}

// Print total sum at the root node
void print_total_sum() {
    printf("Sum at root (node %d) = %d\n", root_node, nodes[root_node].sum);
}

// Cleanup resources
void cleanup_resources() {
    for (int i = 0; i < n; i++) {
        foothread_barrier_destroy(&nodes[i].barrier_id);
        foothread_mutex_destroy(&nodes[i].mutex_id);
    }
}

