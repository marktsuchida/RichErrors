// RichErrors: A C library for rich (nested) error information.
// Author: Mark A. Tsuchida
//
// Copyright 2019 The Board of Regents of the University of Wisconsin System
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once

// Minimal thin wrapper around Win32 threads or pthreads. Only functionality
// used by RichErrors is wrapped. Not for inclusion in public headers!

#ifdef __cplusplus
#error This header is not for use with C++ code.
#endif

#ifdef _WIN32
#define USE_WIN32THREADS 1
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <pthreads.h>
#endif


//
// Types and values
//

#if USE_WIN32THREADS

typedef CRITICAL_SECTION RecursiveMutex;
#define DECLARE_STATIC_MUTEX(m) m

typedef INIT_ONCE MutexInitializer;
#define DECLARE_MUTEX_INITIALIZER(i) i = INIT_ONCE_STATIC_INIT

typedef DWORD ThreadID;

#else // pthreads

typedef pthread_mutex_t RecursiveMutex;
#ifdef PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#define DECLARE_STATIC_MUTEX(m) m = PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#else
#define DECLARE_STATIC_MUTEX(m) m = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#endif

typedef int MutexInitializer;
#define DECLARE_MUTEX_INITIALIZER(i) i

// Since we're not displaying, pthread_t is ok.
typedef pthread_t ThreadID;

#endif


//
// Mutex nonstatic initialization
//

static inline void InitMutex(RecursiveMutex* mutex)
{
#if USE_WIN32THREADS
    InitializeCriticalSection(mutex);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr,
#ifdef PTHREAD_MUTEX_RECURSIVE
        PTHREAD_MUTEX_RECURSIVE
#else
        PTHREAD_MUTEX_RECURSIVE_NP
#endif
    );
    pthread_mutex_init(mutex, &attr);
#endif
}


//
// Mutex static initialization
//

#if USE_WIN32THREADS
// Nonstatic since we take the pointer (won't be inlined)
inline BOOL RERR_Internal_InitializeMutexCallback(PINIT_ONCE i, PVOID param, PVOID* c)
{
    InitializeCriticalSection((RecursiveMutex*)param);
    return TRUE;
}
#endif

static inline void EnsureInitMutex(RecursiveMutex* mutex, MutexInitializer* init)
{
#if USE_WIN32THREADS
    InitOnceExecuteOnce(init, RERR_Internal_InitializeMutexCallback, mutex, NULL);
#else
    // pthread mutex uses static initializer
#endif
}


//
// Mutex lock and unlock
//

static inline void LockMutex(RecursiveMutex* mutex)
{
#if USE_WIN32THREADS
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

static inline void UnlockMutex(RecursiveMutex* mutex)
{
#if USE_WIN32THREADS
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}


//
// Thread id
//

static inline ThreadID GetThisThreadId()
{
#if USE_WIN32THREADS
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}
