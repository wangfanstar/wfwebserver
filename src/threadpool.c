#include <stdlib.h>  
#include <string.h>  
#include "threadpool.h"  
#include "log.h"  
#include "http_handler.h"  

static void *worker_thread(void *arg) {  
    threadpool_t *pool = (threadpool_t *)arg;  
    client_data_t task;  

    while (1) {  
        pthread_mutex_lock(&pool->lock);  

        // 等待任务  
        while (pool->count == 0 && !pool->shutdown) {  
            pthread_cond_wait(&pool->not_empty, &pool->lock);  
        }  

        if (pool->shutdown) {  
            pthread_mutex_unlock(&pool->lock);  
            pthread_exit(NULL);  
        }  

        // 获取任务  
        task = pool->client_queue[pool->head];  
        pool->head = (pool->head + 1) % pool->queue_size;  
        pool->count--;  

        pthread_cond_signal(&pool->not_full);  
        pthread_mutex_unlock(&pool->lock);  

        // 处理客户端请求  
        handle_client(task.client_socket, &task.client_addr);  
    }  

    return NULL;  
}  

threadpool_t* threadpool_create(int thread_count, int queue_size) {  
    threadpool_t *pool = (threadpool_t *)malloc(sizeof(threadpool_t));  
    if (pool == NULL) {  
        write_log("Failed to allocate memory for thread pool");  
        return NULL;  
    }  

    pool->thread_count = thread_count;  
    pool->queue_size = queue_size;  
    pool->head = pool->tail = pool->count = 0;  
    pool->shutdown = 0;  

    // 分配线程数组  
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);  
    if (pool->threads == NULL) {  
        write_log("Failed to allocate memory for threads");  
        free(pool);  
        return NULL;  
    }  

    // 分配任务队列  
    pool->client_queue = (client_data_t *)malloc(sizeof(client_data_t) * queue_size);  
    if (pool->client_queue == NULL) {  
        write_log("Failed to allocate memory for task queue");  
        free(pool->threads);  
        free(pool);  
        return NULL;  
    }  

    // 初始化互斥锁和条件变量  
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {  
        write_log("Failed to initialize mutex");  
        free(pool->client_queue);  
        free(pool->threads);  
        free(pool);  
        return NULL;  
    }  
    if (pthread_cond_init(&pool->not_full, NULL) != 0) {  
        write_log("Failed to initialize condition variable (not_full)");  
        pthread_mutex_destroy(&pool->lock);  
        free(pool->client_queue);  
        free(pool->threads);  
        free(pool);  
        return NULL;  
    }  
    if (pthread_cond_init(&pool->not_empty, NULL) != 0) {  
        write_log("Failed to initialize condition variable (not_empty)");  
        pthread_cond_destroy(&pool->not_full);  
        pthread_mutex_destroy(&pool->lock);  
        free(pool->client_queue);  
        free(pool->threads);  
        free(pool);  
        return NULL;  
    }  

    // 创建工作线程  
    for (int i = 0; i < thread_count; i++) {  
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {  
            write_log("Failed to create thread %d", i);  
            threadpool_destroy(pool);  
            return NULL;  
        }  
        write_log("Created worker thread %lu", (unsigned long)pool->threads[i]);  
    }  

    return pool;  
}  

void threadpool_destroy(threadpool_t *pool) {  
    if (pool == NULL) return;  

    pthread_mutex_lock(&pool->lock);  
    pool->shutdown = 1;  
    pthread_cond_broadcast(&pool->not_empty);  
    pthread_mutex_unlock(&pool->lock);  

    // 等待所有线程结束  
    for (int i = 0; i < pool->thread_count; i++) {  
        pthread_join(pool->threads[i], NULL);  
    }  

    pthread_mutex_destroy(&pool->lock);  
    pthread_cond_destroy(&pool->not_full);  
    pthread_cond_destroy(&pool->not_empty);  
    free(pool->client_queue);  
    free(pool->threads);  
    free(pool);  
}  

int threadpool_add_task(threadpool_t *pool, int client_socket, struct sockaddr_in client_addr) {  
    pthread_mutex_lock(&pool->lock);  

    // 等待队列有空位  
    while (pool->count == pool->queue_size && !pool->shutdown) {  
        pthread_cond_wait(&pool->not_full, &pool->lock);  
    }  

    if (pool->shutdown) {  
        pthread_mutex_unlock(&pool->lock);  
        return -1;  
    }  

    // 添加任务到队列  
    pool->client_queue[pool->tail].client_socket = client_socket;  
    pool->client_queue[pool->tail].client_addr = client_addr;  
    pool->tail = (pool->tail + 1) % pool->queue_size;  
    pool->count++;  

    pthread_cond_signal(&pool->not_empty);  
    pthread_mutex_unlock(&pool->lock);  

    return 0;  
}