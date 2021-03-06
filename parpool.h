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

#ifndef PARPOOL_H
#define PARPOOL_H

#include <pthread.h>


#define PARPOOL_ERROR   -1

/******************************************************************************
 *                             Type Declarations
 *****************************************************************************/

/******************************************************************************
 * 
 * The states a futures can be in
 * 
 * States:
 *      COMPLETED:  the job has completed and output can be read
 *      RUNNING:    the job is currently running
 *      QUEUED:     the job is on the queue waiting to be run
 *
 *****************************************************************************/
enum future_status
{
    JOB_COMPLETED,
    JOB_RUNNING,
    JOB_QUEUED
};

/******************************************************************************
 * 
 * A future to poll asynchronous jobs. Is created when a job is added to the
 * parpool queue.
 * 
 * Fields:
 *      status:     0 = completed, 1 = running, 2 = queued
 *      fcn:        pointer to the function that is being executed for the job
 *      inputs:     the inputs that went into the function being run
 *      outputs:    the output of the function; set once the job is complete
 *
 *****************************************************************************/
typedef struct future
{
    enum future_status      status;
    void *                  ( *fcn )( void * );
    void *                  inputs;
    void *                  output;

} future;

/******************************************************************************
 * 
 * A job to be executed on the parpool
 * 
 * Fields:
 *      fcn:        pointer to the function that is being executed for the job
 *      fcn_args:   the inputs to go into the function being run
 *      future:     the future that is tracking this job
 *      next_job:   the next job in the queue
 * 
 *****************************************************************************/
struct parpool_job
{
    void *                  ( *fcn )( void * );
    void *                  fcn_args;
    future *                future;
    struct parpool_job*     next_job;

};

/******************************************************************************
 * 
 * A queue of jobs to be executed on the parpool
 * 
 * Fields:
 *      length:     the length of the queue
 *      next_job:   the job at the front of the queue;
 *      last_job:   the last job in the queue;
 *      mutex_lock: a mutual exclusion lock on the queue so queue access can be
 *                  synchronized
 *
 *****************************************************************************/
struct parpool_queue
{
    int                     length;
    struct parpool_job *    next_job;
    struct parpool_job *    last_job;
    pthread_spinlock_t      lock;
    
};

/******************************************************************************
 * 
 * The states a worker can be in
 * 
 * States:
 *      WORKING:    the worker is currently running a job
 *      IDLEING:    the worker is idleing
 *      SHUTDOWN:   a shutdown request has been sent to the worker
 *
 *****************************************************************************/
enum worker_status
{
    WORKER_WORKING,
    WORKER_IDLEING,
    WORKER_SHUTDOWN

};

/******************************************************************************
 * 
 * A worker on the parpool to execute jobs
 * 
 * Fields:
 *      id:         an identifier
 *      thread:     the underlying pthread executing the job
 *      job_queue:  a pointer to the pool's job queue
 *
 *****************************************************************************/
struct parpool_worker
{
    int                     id;
    enum worker_status      status;
    pthread_t               thread;
    struct parpool_queue *  job_queue;

};

/******************************************************************************
 * 
 * The parallel pool of workers
 * 
 * Fields:
 *      pool_size:  the number of workers
 *      workers:    pointer to all the workers
 *      queue:      the pool's job queue 
 *
 *****************************************************************************/
typedef struct parpool
{
    int                     pool_size;
    struct parpool_worker * workers;
    struct parpool_queue *  queue;

} parpool;


/******************************************************************************
 *                               Parpool API
 *****************************************************************************/


/******************************************************************************
 *
 * Initialize a pool of parallel workers.
 * 
 * Parameters:
 *      num_workers:    the number of threads to create for the pool
 *
 * Returns:
 *      A reference to the initialized parpool object
 *
 *****************************************************************************/
parpool * parpool_init ( int num_workers );

/******************************************************************************
 *
 * Delete a parpool and clean up associated resources
 * 
 * Parameters:
 *      pool:       the parpool to delete
 *
 *****************************************************************************/
void parpool_delete ( parpool *pool );

/******************************************************************************
 *
 * Add a job to be evaluated by the parpool.
 * 
 * Parameters:
 *      pool:       the parpool to execute on
 *      fcn:        the function to execute
 *      arg:        the argument to pass to fcn 
 *
 *****************************************************************************/
void parpool_eval ( parpool *pool, future *f, void *(fcn)(void *), void *arg );

/******************************************************************************
 *
 * Wait for a single future to be done. Will halt the main process until the 
 * inputted future has completed.
 * 
 * Parameters:
 *      future:     the future to wait for
 *
 *****************************************************************************/
void parpool_wait ( future *future );

/******************************************************************************
 *
 * Wait futures to be done. Will halt the main process until the inputted 
 * number of futures has completed.
 * 
 * Parameters:
 *      futures:        the futures to wait for
 *      num_futures:    the number of futures to wait for. Should be equal to
 *                      the length of futures.
 *
 *****************************************************************************/
void parpool_wait_all ( future *futures, int num_futures );



#endif



