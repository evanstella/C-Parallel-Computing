# An Asynchronous Computing Implementation for C

This is a simple asynchronous parallel thread pool implementation for ANSI C that is meant to mimic the user-friendly interface of MATLAB's Parallel Computing Toolbox. The code is implemented using POSIX Threads but no external dependances. Projects using this code should be compiled with `-lpthread`.

## Interface

`typedef parpool`
- A struct to encapsulate the parallel thread pool.

`typedef future`
- Encapsulates a function that is going to be run on the parpool. Once the function has been run, the output of the function is stored in the `output` field of the future. 

`parpool* parpool_init ( int num_threads )`
- Create a parallel thread pool with the inputted number of threads.

`void parpool_eval ( parpool* pool, future* future, void* (*fcn)(void), void* arg )`
- Add a job for the parpool to execute. 
- The next available worker will execute the function passed in as `fcn` with the argument `arg`.
- The output of the function is stored in the future when it has completed execution

`void parpool_wait ( future* future )`
- Halt execution of the calling thread until the future has completed.
- Once the future has completed, the output of the function that was run is in `future.output`

`void parpool_wait_all ( future* futures, int num_futures )`
- Halt execution of the calling thread until `num_futures` in `futures` has completed.
- `num_futures` should be equal to the number of futures stored in `futures`

`parpool_delete ( parpool* pool )`
- Deallocate the parpool and kill all of its workers.


## Example Usage

In the following extremely simple example, we will use the parallel pool to multiply an array of integers by 2:

```C

/* Mulitply the number at num_in by 2 and return the pointer*/
void * fcn ( void *num_in )
{
    int *n = (int *) num_in;
    *n = (*n) * 2;

    return num_in;
}

int main ( void )
{
    // start a parpool with 4 threads
    parpool *pool = parpool_init(4);
    
    // allocate the futures
    future futures[10];
    
    // use the parpool to multiply integers 0-9 by 2;
    int numbers[10];
    for ( int i = 0; i < 10; i++ )
    {
        numbers[i] = i;
        parpool_eval( pool, &futures[i], fcn, (void *)(&numbers[i]) );
    }

    // block execution until all the jobs have completed
    parpool_wait_all( futures, 10 );

    // print the results
    for ( int i = 0; i < 10; i++ )
    {
        int *result = (int *) futures[i].output;
        printf( "%d ", *result );
    }

    // delete the parpool
    parpool_delete( pool );
}

```

Output:
``` 
0 2 4 6 8 10 12 14 16 18
```
