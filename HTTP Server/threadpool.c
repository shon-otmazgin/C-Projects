#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool.
 */
threadpool* create_threadpool(int num_threads_in_pool){
	if(num_threads_in_pool > MAXT_IN_POOL || num_threads_in_pool <= 0){
		fprintf(stderr , "error wrong argument size , func : create_threadpool\n");
		return NULL;
	}
	threadpool *pool = (threadpool*)malloc(sizeof(threadpool));
	if( pool == NULL ){
		perror("malloc");
		return NULL;
	}
	pool->num_threads = num_threads_in_pool;
	pool->qsize = 0;
	pool->qhead = pool->qtail = NULL;
	pool->shutdown = pool->dont_accept = 0;
	if(pthread_mutex_init(&pool->qlock, NULL) != 0){
		fprintf(stderr , "error while trying to init mutex , func : create_threadpool\n");
		free(pool);
		return NULL;
	}
	if(pthread_cond_init(&pool->q_not_empty, NULL) != 0){
		fprintf(stderr , "error while trying to init cond , func : create_threadpool\n");
		free(pool);
		return NULL;
	}
	if(pthread_cond_init(&pool->q_empty, NULL) != 0){
		fprintf(stderr , "error while trying to init cond , func : create_threadpool\n");
		free(pool);
		return NULL;
	}
	pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*num_threads_in_pool);
	if(pool->threads == NULL){
		free(pool);
		perror("malloc");
		return NULL;
	}
	// now createing the threads.
	int i , rc;
	for (i = 0; i < num_threads_in_pool; i++){
		rc = pthread_create((pool->threads)+i, NULL, do_work, pool);
		if (rc) {
			fprintf(stderr , "error while trying to create thread , func : create_threadpool\n");
			pool->num_threads = i;
			destroy_threadpool(pool);
			return NULL;
		}
	}

	return pool;
}


/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 * this function should:
 * 1. create and init work_t element
 * 2. lock the mutex
 * 3. add the work_t element to the queue
 * 4. unlock mutex
 *
 */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
	if(from_me == NULL || dispatch_to_here == NULL)
		return;
	if(pthread_mutex_lock(&from_me->qlock) != 0){
		fprintf(stderr , "error while trying to lock the queue , func: dispatch\n");
		return;
	}

	if(from_me->dont_accept){
		if ( pthread_mutex_unlock(&from_me->qlock) != 0)
			fprintf(stderr , "error while trying to unlock the queue , func: dispatch\n");
		return;
	}
	// create work.
	work_t *newJob = (work_t*)malloc(sizeof(work_t));
	if(newJob == NULL){
		perror("malloc");
		return;
	}
	// init the work.
	newJob->routine = dispatch_to_here;
	newJob->arg = arg;
	newJob->next = NULL;
	// enqueue the work.
	if(!from_me->qsize)
		from_me->qhead = from_me->qtail = newJob;
	else{
		from_me->qtail->next = newJob;
		from_me->qtail = newJob;
	}
	from_me->qsize++;
	//signal for thread and unlock the queue.
	pthread_cond_signal(&from_me->q_empty);
	if(pthread_mutex_unlock(&from_me->qlock) != 0){
		fprintf(stderr , "error while trying to unlock the queue , func: dispatch\n");
		return;
	}
}

/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 *
 */
void* do_work(void* p){
	if(p == NULL)
		return NULL;
	work_t *job;
	threadpool *pool = (threadpool*)p;
	while(1){
		if(pthread_mutex_lock(&pool->qlock) != 0){
			fprintf(stderr , "error while trying to lock the queue , func: do_work\n");
			return NULL;
		}
		while(!pool->qsize && !pool->shutdown)
			pthread_cond_wait(&pool->q_empty , &pool->qlock);

		if(pool->shutdown){
			if( pthread_mutex_unlock(&pool->qlock) != 0)
				fprintf(stderr , "error while trying to unlock the queue , func: do_work\n");
			return NULL;
		}
		job = pool->qhead;
		pool->qhead = pool->qhead->next;
		pool->qsize--;
		if(!pool->qsize){
			pool->qtail = pool->qhead;
			pthread_cond_signal(&pool->q_not_empty);
		}
		if(pthread_mutex_unlock(&pool->qlock) != 0){
			fprintf(stderr , "error while trying to unlock the queue , func: do_work\n");
			return NULL;
		}
		job->routine(job->arg);
		free(job);
	}
}


/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme){
	if(destroyme != NULL){
		pthread_mutex_lock(&destroyme->qlock);
		destroyme->dont_accept = 1;
		while(destroyme->qsize > 0)
			pthread_cond_wait(&destroyme->q_not_empty , &destroyme->qlock);
		destroyme->shutdown = 1;
		pthread_cond_broadcast(&destroyme->q_empty); // should be broadcast;
		pthread_mutex_unlock(&destroyme->qlock);
		void *retval;
		int i;
		for (i = 0; i < destroyme->num_threads; i++)
			pthread_join(destroyme->threads[i], &retval);
		pthread_mutex_destroy(&destroyme->qlock);
		pthread_cond_destroy(&destroyme->q_empty);
		pthread_cond_destroy(&destroyme->q_not_empty);
		if(destroyme->threads != NULL)
			free(destroyme->threads);
		free(destroyme);
	}
}
