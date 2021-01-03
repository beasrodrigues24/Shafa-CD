/************************************************
 *
 *  Author(s): Pedro Tavares
 *  Created Date: 24 Dec 2020
 *  Updated Date: 03 Jan 2021
 *
 ***********************************************/

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


#include "errors.h"
#include "multithread.h"

#ifdef THREADS
int NO_MULTITHREAD = 0;
#include <unistd.h>

#ifdef POSIX_THREADS
#include <pthread.h>
#endif

#else
int NO_MULTITHREAD = 1;

#endif

#ifdef THREADS
typedef struct {

#ifdef POSIX_THREADS
    pthread_t prev_thread;
    uintptr_t (* process)(void *);
    uintptr_t (* write)(void *, _modules_error, _modules_error);
#endif //POSIX_THREADS


    void * args;

} Data;
#endif //THREADS

// This static global variable is only accessed by the main thread
// `multithread[_create | _wait]`'s functions are Thread-Unsafe because of it. For Thread-Safety it requires a reentrant version
// For further examples check: `strtok` and `strtok_r` respectively defined in C standard and Posix only
// Why? Use same multithread[_create | _wait]'s function assignature for Windows and Posix
#ifdef POSIX_THREADS
static pthread_t THREAD = 0;
#endif

struct timespec start, finish;

#ifdef THREADS
static uintptr_t wrapper(Data * data)
{

#ifdef POSIX_THREADS
    pthread_t prev_thread;
    uintptr_t error, prev_error = _SUCCESS; // Define as _SUCCESS in case there is no prev_hthread

    error = (*(data->process))(data->args);
    prev_thread = data->prev_thread;

    // In case of error in `data->process` it still joins the thread for resource cleanup
    if (prev_thread && pthread_join(prev_thread, (void **) &prev_error))
        prev_error = _THREAD_TERMINATION_FAILED;
#endif  



    error = (*(data->write))(data->args, prev_error, error);
    
    free(data);

    return prev_error ? prev_error : error;
}
#endif //THREADS

// Attention: Be careful that sizeof(void *) must be >= sizeof(_modules_error) 
// otherwise make sure the value returned by those funtions are always <= sizeof(void *)
// Why? Because we use a hack to consider a pointer as an _modules_error to pass between threads
_modules_error multithread_create(_modules_error (* process)(void *), _modules_error (* write)(void *, _modules_error, _modules_error), void * args)
{

#ifndef _NO_MULTITHREAD
    if (NO_MULTITHREAD)
#endif
    {
        _modules_error error;

        error = process(args);
        return write(args, _SUCCESS, error);
    }

    

#ifdef POSIX_THREADS

    Data * data = malloc(sizeof(Data));

    * data = (Data) {
        .prev_thread = THREAD,
        .process = (uintptr_t (*) (void *)) process,
        .write = (uintptr_t (*) (void *, _modules_error, _modules_error)) write,
        .args = args,
    };
    
    if (pthread_create(&THREAD, NULL, (void *) wrapper, data)) {
        free(data);
        return _THREAD_CREATION_FAILED;
    }

#endif



    return _SUCCESS;
}

_modules_error multithread_wait()
{
#ifndef _NO_MULTITHREAD
    if (NO_MULTITHREAD)
#endif
        return _SUCCESS;


#ifdef POSIX_THREADS
    uintptr_t error;

    if (pthread_join(THREAD, (void **) &error))
        return _THREAD_TERMINATION_FAILED;
    return error;
#endif
}




float clock_main_thread(CLOCK_ACTION action)
{

#if defined(THREADS) && (_POSIX_C_SOURCE >= 199309L) // All these have POSIX

    static struct timespec start_time;
    static bool time_fail = true;
    struct timespec finish_time;

    if (action == START_CLOCK)
        return time_fail = clock_gettime(CLOCK_MONOTONIC, &start_time);
    else {
        if (time_fail || clock_gettime(CLOCK_MONOTONIC, &finish_time) == -1)
            return -1;
        return (finish_time.tv_sec - start_time.tv_sec) * 1000 + ((double)(finish_time.tv_nsec - start_time.tv_nsec)) / 1000000.0;
    }


#else
    static clock_t start_time = -1;
    clock_t time;

    if (action == START_CLOCK) {
        start_time = clock();
        return start_time != -1 ? 0 : -1
    }
    else {
        if (start_time == -1)
            return -1;

        time = clock();
        
        if (time == -1)
            return -1;
        
        return (float) (((double) (time - start_time)) / CLOCKS_PER_SEC) * 1000;
    }

#endif
}