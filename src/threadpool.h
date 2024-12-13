#ifndef THREADPOOL_H  
#define THREADPOOL_H  

#include <pthread.h>  
#include <netinet/in.h>  

typedef struct {  
    int client_socket;  
    struct sockaddr_in client_addr;  
} client_data_t;  

typedef struct {  
    pthread_t *threads;  
    client_data_t *client_queue;  
    int thread_count;  
    int queue_size;  
    int head;  
    int tail;  
    int count;  
    pthread_mutex_t lock;  
    pthread_cond_t not_full;  
    pthread_cond_t not_empty;  
    int shutdown;  
} threadpool_t;  

threadpool_t* threadpool_create(int thread_count, int queue_size);  
void threadpool_destroy(threadpool_t *pool);  
int threadpool_add_task(threadpool_t *pool, int client_socket, struct sockaddr_in client_addr);  

#endif