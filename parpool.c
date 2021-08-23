#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "parpool.h"



/******************************************************************************
 *                            Internal Functions
 *****************************************************************************/
static parpool_queue*   internal_queue_init      ( );
static void             internal_worker_init     ( int id, parpool_queue* q, worker* w );
static parpool_job*     internal_job_init        ( future* f, void* (*fcn)(void*), void* argv );

static void             internal_parpool_error   ( parpool* pool, const char* msg );

static void             internal_queue_clear     ( parpool_queue* queue );
static void             internal_parpool_cleanup ( parpool* pool );

static void             internal_worker_delete   ( worker* worker );
static void             internal_job_delete      ( parpool_job* job );
static void             internal_queue_delete    ( parpool_queue* queue );

static parpool_job*     internal_job_request     ( parpool_queue* queue );
static void*            internal_worker_routine  ( void* argv );




/******************************************************************************
 *                             Implementations
 *****************************************************************************/


/******************************************************************************
 * Initialize parpool queue and workers
 *****************************************************************************/
parpool* parpool_init ( int num_workers )
{
    if ( num_workers < 1 )
    {
        internal_parpool_error( 
            NULL, 
            (char*) "Number of parallel workers must be greater than 0" 
        );
    }

    parpool* pool = ( parpool* ) malloc( sizeof(parpool) );
    if ( pool == NULL )
        internal_parpool_error( pool, "Error initializing parpool." );

    pool->pool_size = num_workers;
    pool->queue = internal_queue_init();
    if ( pool->queue == NULL )
        internal_parpool_error( pool, "Error initializing queue." );

    pool->workers = ( worker* ) malloc( num_workers * sizeof(worker) );
    if ( pool->workers == NULL )
        internal_parpool_error( pool, "Error allocating worker." );
    for ( int i = 0; i < num_workers; i++ ) 
        internal_worker_init( i, pool->queue, &pool->workers[i] );
    
    return pool;
}

/******************************************************************************
 * free the memory malloced by the pool
 *****************************************************************************/
void parpool_delete ( parpool* pool )
{
    if ( pool == NULL ) return;
    internal_parpool_cleanup( pool );
    free( pool );
}

/******************************************************************************
 * add a job to the queue
 *****************************************************************************/
void parpool_eval ( parpool* pool, future* f, void* (*fcn)(void*), void* argv )
{
    f-> status = QUEUED;
    f->fcn = fcn;
    f->inputs = argv;
    f->output = NULL;

    parpool_job* job = internal_job_init( f, fcn, argv );

    parpool_queue* queue = pool->queue;
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
}

/******************************************************************************
 * wait for a future to complete
 *****************************************************************************/
void parpool_wait ( future* future )
{
    while(1) if ( future->status == COMPLETED ) return;
}

/******************************************************************************
 * wait for all futures to complete
 *****************************************************************************/
void parpool_wait_all ( future* futures, int num_futures )
{
    int num_completed = 0;
    while(1) 
    {
        for ( int i = 0; i < num_futures; i++ )
        {
            if ( futures[i].status == COMPLETED )
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
static parpool_job* internal_job_init ( future* f, void* (*fcn)(void*), void* argv )
{
    parpool_job* job = ( parpool_job* ) malloc( sizeof(parpool_job) );
    job->fcn = fcn;
    job->fcn_args = argv;
    job->future = f;
    job->next_job = NULL;

    return job;
}

/******************************************************************************
 * get the next job in the queue and pop the queue
 *****************************************************************************/
static parpool_job* internal_job_request ( parpool_queue* queue )
{
    pthread_mutex_lock( &queue->mutex_lock );
    parpool_job* job = queue->next_job;
    if ( job != NULL )
    {
        queue->next_job = job->next_job;
        queue->length--;
    }
    pthread_mutex_unlock( &queue->mutex_lock );
    return job;
}

/******************************************************************************
 * initialize the a job queue
 *****************************************************************************/
static parpool_queue* internal_queue_init ( )
{
    parpool_queue* queue = ( parpool_queue* ) malloc( sizeof(parpool_queue) );
    if ( queue == NULL )
    {
        perror( "Error: unable to allocate queue" );
        return NULL;
    }
    queue->length   = 0;
    queue->next_job = NULL;
    queue->last_job = NULL;
    
    if ( pthread_mutex_init( &queue->mutex_lock, NULL ) != 0 )
    {
        printf("Error: initializing queue mutex lock");
        return NULL;
    }

    return queue;
}

/******************************************************************************
 * initialize the a worker to begin waiting for jobs on the queue
 *****************************************************************************/
static void internal_worker_init ( int id, parpool_queue* queue, worker* w )
{
    if ( w == NULL ) 
        perror( "Error: unable to allocate worker." );

    w->id = id;
    w->status = WORKING;
    w->job_queue = queue;

    if ( pthread_create( &w->thread, NULL, &internal_worker_routine, (void*) w ) != 0 ) 
        perror( "Error: unable to create worker thread." );

}

/******************************************************************************
 * initialize the a worker to begin waiting for jobs on the queue
 *****************************************************************************/
static void internal_parpool_error ( parpool* pool, const char* msg )
{
    parpool_delete ( pool );
    perror( msg );
    exit( PARPOOL_ERROR );
}

/******************************************************************************
 * clear the job queue and free malloced memory
 *****************************************************************************/
static void internal_queue_clear ( parpool_queue* queue )
{
    if ( queue == NULL ) return;
    parpool_job* job_ptr = queue->next_job;
    while ( job_ptr != NULL )
    {
        parpool_job* tmp = job_ptr->next_job;
        internal_job_delete( job_ptr );
        job_ptr = tmp;
    }
    queue->next_job = NULL;
    queue->last_job = NULL;
}

/******************************************************************************
 * cleanup all memory malloced for parpool
 *****************************************************************************/
static void internal_parpool_cleanup ( parpool* pool )
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
static void internal_worker_delete ( worker* wrker )
{
    wrker->status = SHUTDOWN;
    if ( pthread_join( wrker->thread, NULL ) != 0 )
        printf("Error: unable to shutdown worker.");
}

/******************************************************************************
 * delete and free job
 *****************************************************************************/
static void internal_job_delete ( parpool_job* job )
{
    free( job );
} 

/******************************************************************************
 * delete and free queue
 *****************************************************************************/
static void internal_queue_delete ( parpool_queue* queue )
{
    internal_queue_clear( queue );
    pthread_mutex_destroy( &queue->mutex_lock );
    free( queue );
}

/******************************************************************************
 * The main routine run by each worker
 *****************************************************************************/
static void* internal_worker_routine ( void* args ) 
{
    worker* this_worker = ( worker* ) args;

    while ( this_worker->status != SHUTDOWN ) 
    {
        parpool_job* job = internal_job_request( this_worker->job_queue );
        if ( job == NULL )
            continue;

        printf("thread %d:", this_worker->id);
        // evaluate the actual function
        future* future = job->future;
        future->status = RUNNING;
        void* out = job->fcn( job->fcn_args );
        job->future->output = out;
        future->status = COMPLETED;

        internal_job_delete( job );
    }

    pthread_exit( NULL );
}
