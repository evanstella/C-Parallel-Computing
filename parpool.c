/*
MIT License

Copyright (c) 2021 Evan Stella

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "parpool.h"



/******************************************************************************
 *                            Internal Functions
 *****************************************************************************/
static struct parpool_queue * internal_queue_init ( );

static void internal_worker_init 
( 
    int id, 
    struct parpool_queue *q, 
    struct parpool_worker *wrkr
);

static struct parpool_job * internal_job_init 
( 
    future * fut, 
    void *(fcn)(void *), 
    void *arg 
);

static void internal_parpool_error 
( 
    parpool *pool, 
    const char *msg 
);

static void internal_queue_clear ( struct parpool_queue *q );
static void internal_parpool_cleanup ( parpool *pool );

static void internal_worker_delete ( struct parpool_worker *wrkr );
static void internal_job_delete ( struct parpool_job *pool );
static void internal_queue_delete ( struct parpool_queue *q );
static void internal_worker_routine ( void *argv );

static struct parpool_job * internal_job_request ( struct parpool_queue *q );





/******************************************************************************
 *                             Implementations
 *****************************************************************************/


/******************************************************************************
 * Initialize parpool queue and workers
 *****************************************************************************/
parpool * parpool_init ( int num_workers )
{
    if ( num_workers < 1 )
    {
        internal_parpool_error ( 
            NULL, "Number of parallel workers must be greater than 0" 
        );
    }

    parpool *pool = ( parpool * ) malloc( sizeof(parpool) );
    if ( pool == NULL )
        internal_parpool_error( pool, "Error initializing parpool." );

    pool->pool_size = num_workers;
    pool->queue = internal_queue_init();
    if ( pool->queue == NULL )
        internal_parpool_error( pool, "Error initializing queue." );

    pool->workers = ( struct parpool_worker * )
        malloc( num_workers*sizeof(struct parpool_worker) );
    if ( pool->workers == NULL )
        internal_parpool_error( pool, "Error allocating worker." );
    for ( int i = 0; i < num_workers; i++ ) 
        internal_worker_init( i, pool->queue, &pool->workers[i] );
    
    return pool;
}

/******************************************************************************
 * free the memory malloced by the pool
 *****************************************************************************/
void parpool_delete ( parpool *pool )
{
    if ( pool == NULL ) return;
    internal_parpool_cleanup( pool );
    free( pool );
}

/******************************************************************************
 * add a job to the queue
 *****************************************************************************/
void parpool_eval ( parpool *pool, future *f, void *(fcn)(void *), void *argv )
{
    f-> status = JOB_QUEUED;
    f->fcn = fcn;
    f->inputs = argv;
    f->output = NULL;

    struct parpool_job *job = internal_job_init( f, fcn, argv );

    struct parpool_queue *queue = pool->queue;
    pthread_spin_lock( &queue->lock );
    if ( queue->next_job == NULL ) 
    {
        queue->next_job = job;
        queue->last_job = job;
    }
    else 
    {
        queue->last_job->next_job = job;
        queue->last_job = job;
    }
    queue->length++;
    pthread_spin_unlock( &queue->lock );
}

/******************************************************************************
 * wait for a future to complete
 *****************************************************************************/
void parpool_wait ( future *future )
{
    while (1) if ( future->status == JOB_COMPLETED ) return;
}

/******************************************************************************
 * wait for all futures to complete
 *****************************************************************************/
void parpool_wait_all ( future *futures, int num_futures )
{
    int num_completed = 0;
    while (1) {
        for ( int i = 0; i < num_futures; i++ ) 
        {
            if ( futures[i].status == JOB_COMPLETED )
                num_completed++;
        }
        if ( num_completed >= num_futures )
            return;
        num_completed = 0;
    }
}

/******************************************************************************
 * initialize a job object 
 *****************************************************************************/
static struct parpool_job* internal_job_init ( future* f, void* (*fcn)(void*), void* argv )
{
    struct parpool_job* job = ( struct parpool_job* ) malloc( sizeof(struct parpool_job) );
    job->fcn = fcn;
    job->fcn_args = argv;
    job->future = f;
    job->next_job = NULL;

    return job;
}

/******************************************************************************
 * get the next job in the queue and pop the queue
 *****************************************************************************/
static struct parpool_job* internal_job_request ( struct parpool_queue* queue )
{
    if ( queue->length == 0 ) return NULL;

    pthread_spin_lock( &queue->lock );
    struct parpool_job* job = queue->next_job;

    if ( job != NULL ) 
    {
        queue->next_job = job->next_job;
        queue->length--;
    }

    pthread_spin_unlock( &queue->lock );
    return job;
}

/******************************************************************************
 * initialize the a job queue
 *****************************************************************************/
static struct parpool_queue* internal_queue_init ( )
{
    struct parpool_queue *queue;

    queue = ( struct parpool_queue * ) malloc( sizeof(struct parpool_queue) );
    if ( queue == NULL ) 
    {
        perror( "Error: unable to allocate queue" );
        return NULL;
    }

    queue->length   = 0;
    queue->next_job = NULL;
    queue->last_job = NULL;
    
    if ( pthread_spin_init( &queue->lock, PTHREAD_PROCESS_PRIVATE ) != 0 ) 
    {
        printf("Error: initializing queue spinlock");
        return NULL;
    }

    return queue;
}

/******************************************************************************
 * initialize the a worker to begin waiting for jobs on the queue
 *****************************************************************************/
static void internal_worker_init ( int id, struct parpool_queue *q, struct parpool_worker *w )
{
    if ( w == NULL ) 
        perror( "Error: unable to allocate worker." );

    w->id = id;
    w->status = WORKER_WORKING;
    w->job_queue = q;

    if ( pthread_create( 
            &w->thread, NULL, (void*) &internal_worker_routine, (void*) w ) != 0 ) 
        perror( "Error: unable to create worker thread." );
}

/******************************************************************************
 * initialize the a worker to begin waiting for jobs on the queue
 *****************************************************************************/
static void internal_parpool_error ( parpool *pool, const char *msg )
{
    parpool_delete ( pool );
    perror( msg );
    exit( PARPOOL_ERROR );
}

/******************************************************************************
 * clear the job queue and free malloced memory
 *****************************************************************************/
static void internal_queue_clear ( struct parpool_queue *queue )
{
    if ( queue == NULL ) return;
    struct parpool_job* job_ptr = queue->next_job;
    while ( job_ptr != NULL )
    {
        struct parpool_job* tmp = job_ptr->next_job;
        internal_job_delete( job_ptr );
        job_ptr = tmp;
    }
    queue->next_job = NULL;
    queue->last_job = NULL;
}

/******************************************************************************
 * cleanup all memory malloced for parpool
 *****************************************************************************/
static void internal_parpool_cleanup ( parpool *pool )
{
    if ( pool == NULL ) return;
    for ( int i = 0; i < pool->pool_size; i++ )
        internal_worker_delete( &pool->workers[i] );
    free( pool->workers );

    if ( pool->queue != NULL ) 
        internal_queue_delete( pool->queue );


    
}

/******************************************************************************
 * delete and free worker
 *****************************************************************************/
static void internal_worker_delete ( struct parpool_worker *wrker )
{
    wrker->status = WORKER_SHUTDOWN;

    if ( pthread_join( wrker->thread, NULL ) != 0 )
        printf("Error: unable to shutdown worker.");
}

/******************************************************************************
 * delete and free job
 *****************************************************************************/
static void internal_job_delete ( struct parpool_job *job )
{
    free( job );
} 

/******************************************************************************
 * delete and free queue
 *****************************************************************************/
static void internal_queue_delete ( struct parpool_queue *queue )
{
    internal_queue_clear( queue );
    pthread_spin_destroy( &queue->lock );
    free( queue );
}

/******************************************************************************
 * The main routine run by each worker
 *****************************************************************************/
static void internal_worker_routine ( void *args ) 
{
    struct parpool_worker *this_worker = ( struct parpool_worker * ) args;

    while ( this_worker->status != WORKER_SHUTDOWN ) 
    {
        struct parpool_job *job = internal_job_request( this_worker->job_queue );
        if ( job == NULL )
            continue;

        // evaluate the actual function
        future *future = job->future;
        future->status = JOB_RUNNING;
        void *out = job->fcn( job->fcn_args );
        job->future->output = out;
        future->status = JOB_COMPLETED;

        internal_job_delete( job );
    }

}
