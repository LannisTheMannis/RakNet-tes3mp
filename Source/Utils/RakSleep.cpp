/*
 *  Copyright (c) 2014, Oculus VR, Inc.
 *  Copyright (c) 2016-2018, TES3MP Team
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#if   defined(_WIN32)
#include "WindowsIncludes.h" // Sleep
#else

#include <pthread.h>
#include <ctime>
#include <sys/time.h>

pthread_mutex_t fakeMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t fakeCond = PTHREAD_COND_INITIALIZER;
#endif

#include "RakSleep.h"

void RakSleep(unsigned int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    //Single thread sleep code thanks to Furquan Shaikh, http://somethingswhichidintknow.blogspot.com/2009/09/sleep-in-pthread.html
    //Modified slightly from the original
    struct timespec timeToWait{};
    struct timeval now{};

    gettimeofday(&now, nullptr);

    long seconds = ms / 1000;
    long nanoseconds = (ms - seconds * 1000) * 1000000;
    timeToWait.tv_sec = now.tv_sec + seconds;
    timeToWait.tv_nsec = now.tv_usec * 1000 + nanoseconds;

    if (timeToWait.tv_nsec >= 1000000000)
    {
        timeToWait.tv_nsec -= 1000000000;
        timeToWait.tv_sec++;
    }

    pthread_mutex_lock(&fakeMutex);
    int rt = pthread_cond_timedwait(&fakeCond, &fakeMutex, &timeToWait);
    pthread_mutex_unlock(&fakeMutex);
    (void)(rt);
#endif
}
