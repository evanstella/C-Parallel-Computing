#ifndef PARPOOL_H
#define PARPOOL_H

#include <unistd.h>
#include <stdio.h>
#include <pthread.h>


/******************************************************************************
 *                           Type Declarations
 *****************************************************************************/


/******************************************************************************
 * 
 * A future to poll asynchronous jobs. Is returned when a job is added to the
 * parpool queue.
 * 
 * Attributes:
 *      status:     0 = completed, 1 = running, 2 = queued
 *      fcn:        pointer to the function that is being executed for the job
 *      inputs:     the inputs that went into the function being run
 *      outputs:    the output of the function; set once the job is complete
 *
 *****************************************************************************/
typedef struct future
{
    int             status;
    void*           ( *fcn )( void* );
    void*           inputs;
    void*           output;

} future;

/******************************************************************************
 * 
 * A job to be executed on the parpool
 * 
 * Attributes:
 *      fcn:        pointer to the function that is being executed for the job
 *      fcn_args:   the inputs to go into the function being run
 *      future:     the future that is tracking this job
 *      next_job:   the next job in the queue
 * 
 *****************************************************************************/
typedef struct parpool_job parpool_job;
struct parpool_job
{
    void*           ( *fcn )( void* );
    void*           fcn_args;
    future*         future;
    parpool_job*    next_job;

};

/******************************************************************************
 * 
 * A queue of jobs to be executed on the parpool
 * 
 * Attributes:
 *      length:     the length of the queue
 *      next_job:   the job at the front of the queue;
 *      last_job:   the last job in the queue;
 *
 *****************************************************************************/
typedef struct parpool_queue
{
    int             length;
    parpool_job*    next_job;
    parpool_job*    last_job;
    
} parpool_queue;

/******************************************************************************
 * 
 * A worker on the parpool to execute jobs
 * 
 * Attributes:
 *      id:         an identifier
 *      thread:     the underlying pthread executing the job
 *      job_queue:  a pointer to the pool's job queue
 *
 *****************************************************************************/
typedef struct worker
{
    int             id;
    pthread_t*      thread;
    parpool_queue*  job_queue;

} worker;

/******************************************************************************
 * 
 * The parallel pool of workers
 * 
 * Attributes:
 *      pool_size:  the number of workers
 *      workers:    pointers to all the workers
 *      queue:      the pool's job queue 
 *
 *****************************************************************************/
typedef struct parpool
{
    int             pool_size;
    worker**        workers;
    parpool_queue*  queue;

} parpool;


/******************************************************************************
 *                               Parpool API
 *****************************************************************************/


/******************************************************************************
 *
 * Initialize a pool of parallel workers.
 * 
 * Parameters:
 *      pool_size:  the number of workers
 *      workers:    pointers to all the workers
 *      queue:      the pool's job queue 
 *
 * Returns:
 *      A reference to the initalized parpool object
 *
 *****************************************************************************/
parpool* parpool_init ( int num_workers );




/******************************************************************************
 *                            Internal Functions
 *****************************************************************************/
static parpool_queue*   internal_queue_init      ( );
static worker*          internal_worker_init     ( int id, parpool_queue* q );

static void             internal_parpool_error   ( parpool* pool, char* msg );

static void             internal_queue_clear     ( parpool_queue* queue );
static void             internal_parpool_cleanup ( parpool* pool );

static void             internal_worker_delete   ( worker* worker );
static void             internal_job_delete      ( parpool_job* job );
static void             internal_queue_delete    ( parpool_queue* queue );

static void*            internal_worker_routine  ( void* argv );




/******************************************************************************
 *                             Implementations
 *****************************************************************************/
parpool* parpool_init ( int num_workers )
{
    if ( num_workers < 1 )
    {
        char* msg = (char*) "Number of parallel workers must be greater than 0";
        internal_parpool_error( NULL, msg );
    }

    parpool* pool = ( parpool* ) malloc( sizeof(parpool) );
    if ( pool == NULL )
        internal_parpool_error( pool, (char*) "Error initializing parpool." );

    pool->pool_size = num_workers;
    pool->queue = internal_queue_init();

    pool->workers = ( worker** ) malloc( num_workers * sizeof(worker*) );
    for ( int i = 0; i < num_workers; i++ ) 
    {
        pool->workers[i] = internal_worker_init( i, pool->queue );
        if ( pool->workers[i] == NULL )
            internal_parpool_error( pool, (char*) "Error creating thread." );
    }
    
    return pool;
}


static parpool_queue* internal_queue_init ( )
{
    parpool_queue* queue = ( parpool_queue* ) malloc( sizeof(parpool_queue) );
    queue->length   = 0;
    queue->next_job = NULL;
    queue->last_job = NULL;

    return queue;
}

static worker* internal_worker_init ( int id, parpool_queue* queue )
{
    worker* wrker = ( worker* ) malloc( sizeof(worker) );
    if ( wrker == NULL ) return NULL;

    wrker->id = id;
    wrker->job_queue = queue;
    wrker->thread = ( pthread_t* ) malloc( sizeof(pthread_t) );
    if ( wrker->thread == NULL ) return NULL;

    int rc = pthread_create( wrker->thread, NULL, &internal_worker_routine, NULL );
    if ( rc != 0 ) return NULL;

    return wrker;
}


#define PARPOOL_ERROR   -1

static void internal_parpool_error ( parpool* pool, char* msg )
{
    internal_parpool_cleanup ( pool );
    perror( msg );
    exit( PARPOOL_ERROR );
}


static void internal_queue_clear ( parpool_queue* queue )
{
    if ( queue == NULL ) return;
    parpool_job* job_ptr = queue->next_job;
    while ( job_ptr != NULL )
    {
        parpool_job* tmp = job_ptr->next_job;
        internal_job_delete( job_ptr );
        free( job_ptr );
        job_ptr = tmp;
    }
    queue->next_job = NULL;
    queue->last_job = NULL;
}


static void internal_parpool_cleanup ( parpool* pool )
{
    if ( pool == NULL ) return;
    if ( pool->queue != NULL ) 
        internal_queue_delete( pool->queue );

    for ( int i = 0; i < pool->pool_size; i++ )
        internal_worker_delete( pool->workers[i] );
    free( pool->workers );
    
}


static void internal_worker_delete ( worker* worker )
{
    free( worker->thread );
    free( worker );
}


static void internal_job_delete ( parpool_job* job )
{
    free( job->future );
    free( job );
} 

static void internal_queue_delete ( parpool_queue* queue )
{
    internal_queue_clear( queue );
    free( queue );
}


static void* internal_worker_routine ( void* args ) 
{
    if (!args)
        printf("test\n");

    return NULL;
}


#endif



