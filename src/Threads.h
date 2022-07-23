// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

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
#include <pthread.h>
#endif

//
// Types and values
//

#if USE_WIN32THREADS

typedef CRITICAL_SECTION Mutex;

typedef INIT_ONCE CallOnceFlag;

#define CALL_ONCE_FLAG_INITIALIZER INIT_ONCE_STATIC_INIT

typedef DWORD ThreadID;

#else // pthreads

typedef pthread_mutex_t Mutex;

typedef pthread_once_t CallOnceFlag;

#define CALL_ONCE_FLAG_INITIALIZER PTHREAD_ONCE_INIT

// Since we're not displaying, pthread_t is ok.
typedef pthread_t ThreadID;

#endif

//
// Mutex initialization
//

void InitRecursiveMutex(Mutex *mutex);

//
// Call-once support
//

#if USE_WIN32THREADS
// Not static since we are taking the pointer (won't be inlined anyway).
inline BOOL __stdcall RERR_Internal_CallOnceCallback(PINIT_ONCE i, PVOID param,
                                                     PVOID *c) {
    (void)i;
    (void)c;
    typedef void (*voidvoidfunc)(void);
    voidvoidfunc func = (voidvoidfunc)param;
    func();
    return TRUE;
}
#endif

static inline void CallOnce(CallOnceFlag *flag, void (*func)(void)) {
#if USE_WIN32THREADS
    InitOnceExecuteOnce(flag, RERR_Internal_CallOnceCallback, (void *)func,
                        NULL);
#else
    pthread_once(flag, func);
#endif
}

//
// Mutex lock and unlock
//

static inline void LockMutex(Mutex *mutex) {
#if USE_WIN32THREADS
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

static inline void UnlockMutex(Mutex *mutex) {
#if USE_WIN32THREADS
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

//
// Thread id
//

static inline ThreadID GetThisThreadId(void) {
#if USE_WIN32THREADS
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}
