#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define LL_ADD(item, list) \
    do { \
        if (NULL == item)  break;  \
        item->prev = NULL; \
        item->next = list; \
        if(list != NULL) list->prev = item; \
        list = item; \
    } while(0);

#define LL_REMOVE(item, list) \
    do { \
        if(NULL == item) break; \
        if(item->prev != NULL) item->prev->next = item->next; \
        if(item->next != NULL) item->next->prev = item->prev; \
        if(item == list) list = list->next; \
        item->prev = item->next = NULL; \
    } while(0);


typedef struct Task
{
    void (*task_func)(struct Task * data);
    void* data;
    
    struct Task* prev;
    struct Task* next;
}Task;


typedef struct Worker
{
    pthread_t tid;
    int terminal;
    
    struct pool_thread* pool;
    
    struct Worker* prev;
    struct Worker* next;
}Worker;


typedef struct pool_thread
{
    Worker* workers;
    Task* tasks;
    
    pthread_mutex_t resizeMutex;
    int freeSize;
    int maxSize;
    int terSize;
    
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}pool_thread;

static void* thread_func(void* argu);

static void add_worker(pool_thread* pool, int size)
{
    if (size < 1)
        return;
    for (int index = 0; index < size;++index)
    {
         Worker* worker = (Worker*)malloc(sizeof(Worker));
        if(worker == NULL)
        {
            printf("worker malloc error!\n");
            exit(-1);
        }
        memset(worker, 0, sizeof(Worker));     
        worker->pool = pool;
        
        int res = pthread_create(&worker->tid, NULL, thread_func, worker);
        if (res)
        {
            printf("pthread_create error!\n");
            worker->terminal = 1;
            continue;
        }
        
        LL_ADD(worker, pool->workers);
        ++pool->maxSize;
        ++pool->freeSize;
    }
   
}

static void free_worker(pool_thread* pool, int size)
{
    if (pool == NULL || size == 0)
        return;
    
    Worker* head = pool->workers;
    pool->terSize = size;
    for (int index = 0; index < size; ++index)
    {
        pthread_cond_signal(&pool->cond);
    }
}

// free size in (0.4, 0.8) resize to 0.6
static void resize_poll(pool_thread* pool)
{
    double freeSize = pool->freeSize;
    double maxSize = pool->maxSize;
    
    if (freeSize/maxSize <= 0.4)
    {
        int addSize = maxSize * 0.6 - freeSize;
        printf("resize worker add : %d, %d, %d\n", pool->freeSize, pool->maxSize, addSize);
        add_worker(pool, addSize);
    }    
    else if(freeSize/maxSize >= 0.8)
    {
        if (maxSize <= 5)
        {
            return;
        }
         
        int terSize = freeSize - maxSize * 0.6;
        printf("resize worker free : %d, %d, %d\n", pool->freeSize, pool->maxSize, terSize);
        free_worker(pool, terSize);
        
        pthread_mutex_unlock(&pool->resizeMutex);
    }
}

static void* thread_func(void* argu)
{
    Worker* worker = (Worker*)argu;
    pool_thread* pool = worker->pool;
    
    while(1)
    {
        pthread_mutex_lock(&pool->mutex);
        while(pool->tasks == NULL)
        {
            if (pool->terSize > 0)
            {
                --pool->terSize;
                worker->terminal = 1;
                printf("free worker...\n");
                break;
            }
        
            if(worker->terminal)
            {
                break;
            }
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }
        
        if (worker->terminal)
        {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }
        
        Task* task = pool->tasks;
        if (task)
            LL_REMOVE(task, pool->tasks);
        pthread_mutex_unlock(&pool->mutex);
        
        if(task == NULL)
            continue;
        
        pthread_mutex_lock(&pool->resizeMutex);
        --pool->freeSize;
        resize_poll(pool);
        pthread_mutex_unlock(&pool->resizeMutex);
        
        task->task_func(task);
        
        pthread_mutex_lock(&pool->resizeMutex);
        ++pool->freeSize;
        resize_poll(pool);
        pthread_mutex_unlock(&pool->resizeMutex);
        
    }
    
    
    free(worker);
    --pool->maxSize;
    --pool->freeSize;
    pthread_exit(0);
}


int pool_create(pool_thread* pool, int size)
{
    if (NULL == pool)
        return 1;

    if(size < 1)
        size = 1;
   
    memset(pool, 0, sizeof(pool_thread));
    
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->mutex, &mutex, sizeof(mutex));
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->cond, &cond, sizeof(cond));
    
    add_worker(pool, size);   
    return 1;
}

int pool_destroy(pool_thread* pool)
{
    if (NULL == pool)
        return 1;
   
    for (Worker* worker = pool->workers; worker != NULL; worker = worker->next)
        worker->terminal = 1;
        
    pthread_mutex_lock(&pool->mutex);
    pthread_mutex_lock(&pool->resizeMutex);
 
    pool->workers = NULL;
    pool->tasks = NULL;
    
    pthread_cond_broadcast(&pool->cond);
    
    pthread_mutex_unlock(&pool->mutex);   
    pthread_mutex_unlock(&pool->resizeMutex);
}

void pool_task(pool_thread* pool, Task* task)
{
    if (pool == NULL)
        return ;
        
    pthread_mutex_lock(&pool->mutex);
    
    LL_ADD(task, pool->tasks);    
    
    pthread_cond_signal(&pool->cond);
    
    pthread_mutex_unlock(&pool->mutex);
}

// test code...
void king_counter(Task *job) {

	int index = *(int*)job->data;

    sleep(2);
	printf("index : %d, selfid : %lu\n", index, pthread_self());
	
	free(job->data);
	free(job);
}

#define KING_MAX_THREAD			80
#define KING_COUNTER_SIZE		10

int main(int argc, char *argv[]) {
    int numWorkers = 5;
    
    pool_thread pool;
    pool_create(&pool, numWorkers);

    int i = 0;
    for (i = 0;i < KING_COUNTER_SIZE;i ++) {
		Task *job = (Task*)malloc(sizeof(Task));
		if (job == NULL) {
			perror("malloc");
			exit(1);
		}
		
		job->task_func = king_counter;
		job->data = malloc(sizeof(int));
		*(int*)job->data = i;

		pool_task(&pool, job);
	}
   
	getchar();  
	//pool_destroy(&pool);
	printf("\n");

	
}