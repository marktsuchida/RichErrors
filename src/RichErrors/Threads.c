// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#define _POSIX_C_SOURCE 200809L

#include "Threads.h"

void InitRecursiveMutex(Mutex *mutex) {
#if USE_WIN32THREADS
    InitializeCriticalSection(mutex);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(mutex, &attr);
#endif
}
