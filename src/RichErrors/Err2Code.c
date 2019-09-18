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

#include "RichErrors/Err2Code.h"

#include "DynArray.h"
#include "Threads.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>


// Although our map is functionally per-thread, we do not use a thread-local
// map. This is because cleanup is very difficult without C++, and even in C++
// it is difficult to avoid leaking errors registered on threads that survive
// the error map instance. Since this is about error handling, we do not mind
// the overhead of a straightforward mutex.


struct MappedError {
    ThreadID thread;
    int32_t code;
    RERR_ErrorPtr error;
};


struct RERR_ErrorMap {
    int32_t minCode; // const
    int32_t maxCode; // const
    int32_t noErrorCode; // const
    int32_t oomCode; // const
    int32_t failCode; // const

    RecursiveMutex mutex;
    int32_t nextCode;
    struct MappedError* mappings; // Always sorted (MappedError_Compare)
    size_t mapCapacity; // = 0 iff mappings == NULL
    size_t mapSize; // <= mapCapacity
};


static int MappedError_Compare(const struct MappedError* lhs, const struct MappedError* rhs)
{
    if (lhs->thread < rhs->thread) {
        return -1;
    }
    if (lhs->thread > rhs->thread) {
        return 1;
    }
    if (lhs->code < rhs->code) {
        return -1;
    }
    if (lhs->code > rhs->code) {
        return 1;
    }
    return 0;
}


// Returns pointer to item in mapping, or NULL if not found. Mutex must be held.
static struct MappedError* ErrorMap_Find(RERR_ErrorMapPtr map, ThreadID thread, int32_t code)
{
    struct MappedError key;
    key.thread = thread;
    key.code = code;
    key.error = NULL;

    return DynArray_BSearchExact(map->mappings, &key, map->mapSize,
        sizeof(struct MappedError), MappedError_Compare);
}


static inline size_t ErrorMap_CapacityForSize(size_t size)
{
    return DynArray_CapacityForSize(size, 16, 6);
}


// Mutex must be held.
static RERR_ErrorPtr ErrorMap_EnsureCapacity(RERR_ErrorMapPtr map)
{
    size_t newSize = map->mapSize + 1;

    if (map->mapCapacity >= newSize) {
        return RERR_NO_ERROR;
    }

    size_t newCapacity = ErrorMap_CapacityForSize(newSize);
    bool ok = DynArray_SetCapacity(&map->mappings, newCapacity,
        sizeof(struct MappedError));
    if (!ok) {
        return RERR_Error_CreateOutOfMemory();
    }
    map->mapCapacity = newCapacity;
    return RERR_NO_ERROR;
}


// Mutex must be held.
static void ErrorMap_ShrinkCapacity(RERR_ErrorMapPtr map)
{
    size_t newCapacity = ErrorMap_CapacityForSize(map->mapSize);
    if (map->mapCapacity <= newCapacity + 32) { // Hysteresis of 32
        return;
    }

    bool ok = DynArray_SetCapacity(&map->mappings, newCapacity,
        sizeof(struct MappedError));
    if (!ok) {
        // Failed to shrink, but haven't broken anything.
        return;
    }
    map->mapCapacity = newCapacity;
}


// Inserts item, taking ownership of error on success. Mutex must be held.
static RERR_ErrorPtr ErrorMap_Insert(RERR_ErrorMapPtr map,
    ThreadID thread, int32_t code, RERR_ErrorPtr error)
{
    RERR_ErrorPtr err = ErrorMap_EnsureCapacity(map);
    if (err != RERR_NO_ERROR) {
        return err;
    }

    struct MappedError key;
    key.thread = thread;
    key.code = code;
    key.error = NULL;

    struct MappedError* ins = DynArray_BSearchInsertionPoint(map->mappings,
        &key, map->mapSize, sizeof(struct MappedError), MappedError_Compare);
    DynArray_InsertElem(ins, map->mappings + map->mapSize, &map->mapSize,
        sizeof(struct MappedError));
    ins->thread = thread;
    ins->code = code;
    ins->error = error;
    return RERR_NO_ERROR;
}


// Mutex must be held; item must be in mapping; otherwise behavior undefined.
// It is assumed that the item has already been destroyed.
static void ErrorMap_Erase(RERR_ErrorMapPtr map, struct MappedError* item)
{
    DynArray_EraseElem(map->mappings, map->mappings + map->mapSize,
        &map->mapSize, sizeof(struct MappedError));

    ErrorMap_ShrinkCapacity(map);
}


// Precondition: code is valid w.r.t. minCode and maxCode
static inline int32_t IncrementCode(int32_t code, int32_t minCode, int32_t maxCode)
{
    if (code == maxCode) {
        return minCode;
    }
    return code == INT32_MAX ? INT32_MIN : code + 1;
}


static inline bool CodeIsInRange(int32_t code, int32_t minCode, int32_t maxCode)
{
    bool continuousRange = minCode <= maxCode;
    return (continuousRange && minCode <= code && code <= maxCode) ||
        !continuousRange && (minCode <= code || code <= maxCode);
}


static inline RERR_ErrorPtr CheckConfig(const RERR_ErrorMapConfig* config)
{
    if (!config) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null error map config given");
    }

    if (CodeIsInRange(config->noErrorCode,
        config->minMappedCode, config->maxMappedCode)) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_MAP_INVALID_CONFIG,
            "Mapped code range contains no-error code");
    }

    if (CodeIsInRange(config->outOfMemoryCode,
        config->minMappedCode, config->maxMappedCode)) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_MAP_INVALID_CONFIG,
            "Mapped code range contains out-of-memory code");
    }

    if (CodeIsInRange(config->mapFailureCode,
        config->minMappedCode, config->maxMappedCode)) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_MAP_INVALID_CONFIG,
            "Mapped code range contains map-failure code");
    }

    if (config->outOfMemoryCode == config->noErrorCode) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_MAP_INVALID_CONFIG,
            "Out-of-memory code cannot equal no-error code");
    }
    if (config->mapFailureCode == config->noErrorCode) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_MAP_INVALID_CONFIG,
            "Map-failure code cannot equal no-error code");
    }
    return RERR_NO_ERROR;
}


RERR_ErrorPtr RERR_ErrorMap_Create(RERR_ErrorMapPtr* map,
    const RERR_ErrorMapConfig* config)
{
    if (!map) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null address for error map pointer given");
    }
    *map = NULL;

    RERR_ErrorPtr err = CheckConfig(config);
    if (err != RERR_NO_ERROR) {
        return err;
    }

    *map = calloc(1, sizeof(struct RERR_ErrorMap));
    if (!*map) {
        return RERR_Error_CreateOutOfMemory();
    }

    (*map)->minCode = config->minMappedCode;
    (*map)->maxCode = config->maxMappedCode;
    (*map)->noErrorCode = config->noErrorCode;
    (*map)->oomCode = config->outOfMemoryCode;
    (*map)->failCode = config->mapFailureCode;

    InitMutex(&(*map)->mutex);
    (*map)->nextCode = (*map)->minCode;

    return RERR_NO_ERROR;
}


void RERR_ErrorMap_Destroy(RERR_ErrorMapPtr map)
{
    if (!map) {
        return;
    }

    struct MappedError* begin = map->mappings;
    struct MappedError* end = begin + map->mapSize;
    for (struct MappedError* i = begin; i < end; ++i) {
        RERR_Error_Destroy(i->error);
    }

    DynArray_Dealloc(&map->mappings);

    free(map);
}


int32_t RERR_ErrorMap_RegisterThreadLocal(RERR_ErrorMapPtr map,
    RERR_ErrorPtr error)
{
    if (!map) {
        // TODO We could call a user-supplied fatal error handler before
        // crashing.
        abort();
    }

    if (error == RERR_NO_ERROR) {
        return map->noErrorCode;
    }
    if (RERR_Error_IsOutOfMemory(error)) {
        return map->oomCode;
    }

    ThreadID thread = GetThisThreadId();

    int32_t ret = map->noErrorCode;
    LockMutex(&map->mutex);
    {
        int32_t firstCandidate = map->nextCode;
        map->nextCode = IncrementCode(map->nextCode, map->minCode, map->maxCode);

        int32_t code = firstCandidate;
        for (;;) {
            struct MappedError* found = ErrorMap_Find(map, thread, code);
            if (!found) { // Code is available
                RERR_ErrorPtr err = ErrorMap_Insert(map, thread, code, error);
                if (err == RERR_NO_ERROR) {
                    ret = code;
                    break;
                }
                ret = RERR_Error_IsOutOfMemory(err) ? map->oomCode : map->failCode;
                RERR_Error_Destroy(error);
                break;
            }

            code = IncrementCode(code, map->minCode, map->maxCode);
            if (code == firstCandidate) { // Range exhausted
                ret = map->failCode;
                RERR_Error_Destroy(error);
                break;
            }
        }
    }
    UnlockMutex(&map->mutex);
    return ret;
}


bool RERR_ErrorMap_IsRegisteredThreadLocal(RERR_ErrorMapPtr map, int32_t code)
{
    if (!map) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null error map pointer given");
    }
    if (code == map->noErrorCode ||
        code == map->oomCode ||
        code == map->failCode) {
        // Special codes are implicitly "registered"
        return true;
    }
    LockMutex(&map->mutex);
    struct MappedError* found = ErrorMap_Find(map, GetThisThreadId(), code);
    UnlockMutex(&map->mutex);
    return found != NULL;
}


RERR_ErrorPtr RERR_ErrorMap_RetrieveThreadLocal(RERR_ErrorMapPtr map,
    int32_t mappedCode)
{
    if (!map) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null error map pointer given");
    }
    if (mappedCode == map->noErrorCode) {
        return RERR_NO_ERROR;
    }
    if (mappedCode == map->oomCode) {
        return RERR_Error_CreateOutOfMemory();
    }
    if (mappedCode == map->failCode) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_MAP_FAILURE, "Failed to assign an error code");
    }

    RERR_ErrorPtr ret = RERR_NO_ERROR;
    LockMutex(&map->mutex);
    {
        struct MappedError* found = ErrorMap_Find(map, GetThisThreadId(), mappedCode);
        if (found) {
            ret = found->error;
            ErrorMap_Erase(map, found);
        }
        else {
            ret = RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
                RERR_ECODE_MAP_INVALID_CODE,
                "Unregistered error code (probably a bug in error handling)");
        }
    }
    UnlockMutex(&map->mutex);
    return ret;
}


void RERR_ErrorMap_ClearThreadLocal(RERR_ErrorMapPtr map)
{
    if (!map) {
        return;
    }

    ThreadID thisThread = GetThisThreadId();

    LockMutex(&map->mutex);
    {
        struct MappedError* begin = map->mappings;
        struct MappedError* end = begin + map->mapSize;
        struct MappedError* j = begin;
        for (struct MappedError* i = begin; i < end; ++i) {
            if (i->thread == thisThread) {
                RERR_Error_Destroy(i->error);
            }
            else if (j < i) {
                memcpy(j, i, sizeof(struct MappedError));
            }
        }
        map->mapSize = j - begin;

        ErrorMap_ShrinkCapacity(map);
    }
    UnlockMutex(&map->mutex);
}
