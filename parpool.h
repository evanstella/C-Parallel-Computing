#ifndef PARPOOL_H
#define PARPOOL_H

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>


typedef struct parpool
{
    int             pool_size;
    worker*         workers;
    parpool_queue*  queue;

} parpool;

typedef struct parpool_job
{
    void*           ( fcn )( void* );
    void*           fcn_args;
    future*         future;
    parpool_job*    next_job;

} parpool_job;

typedef struct parpool_queue
{
    unsigned int    length;
    parpool_job*    next_job;
    parpool_job*    last_job;
    
} parpool_queue;

typedef struct worker
{
    int             id;
    pthread_t*      thread;
    parpool_queue*  job_queue;

} worker;

typedef struct future
{
    int             status;
    void*           ( fcn )( void* );
    void*           inputs;
    void*           output;

} future;




parpool* parpool_init ( int num_workers )
{
    parpool* pool = ( parpool* ) malloc( sizeof(parpool) );
    if ( pool == NULL )

    pool->pool_size = num_workers;


         
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
    if ( pool->queue != NULL ) internal_queue_clear( pool->queue );

    for ( int i = 0; i < pool->pool_size; i++ )
        internal_worker_delete( &pool->workers[i] );
    free( pool->workers );
    
}


static void internal_worker_delete ( worker* worker )
{
    free( worker->thread );
}


static void internal_job_delete ( parpool_job* job )
{
    free( job->future );
} 


static void internal_worker_routine ( void ) 
{
    printf("test");
}


#endif



