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

#include "../RichErrors/Threads.h"

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
    int32_t noErrorCode; // const
    int32_t minCode; // const
    int32_t maxCode; // const

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

    return bsearch(&key, map->mappings, map->mapSize,
        sizeof(struct MappedError), MappedError_Compare);
}


// Returns stepped capacity >= given size
static inline size_t ErrorMap_CapacityForSize(size_t size)
{
    // Zero - const - exponential - linear

    const unsigned minPow = 6;
    const unsigned maxPow = 16;

    if (size == 0) {
        return 0;
    }

    const size_t minThresh = (size_t)1 << (minPow - 1);
    if (size <= minThresh) {
        return minThresh;
    }

    const size_t linThresh = (size_t)1 << (maxPow - 1);
    if (size >= linThresh) {
        return ((size - 1) / linThresh + 1) * linThresh;
    }

    size_t s = size - 1;
    size_t ret = 1;
    while (s >>= 1) {
        ret <<= 1;
    }
    return ret;
}


// Mutex must be held.
static RERR_ErrorPtr ErrorMap_EnsureCapacity(RERR_ErrorMapPtr map)
{
    size_t newSize = map->mapSize + 1;

    if (map->mapCapacity >= newSize) {
        return RERR_NO_ERROR;
    }

    size_t newCapacity = ErrorMap_CapacityForSize(newSize);
    struct MappedError* newMappings;
    if (map->mapCapacity == 0) {
        newMappings = malloc(sizeof(struct MappedError) * newCapacity);
    }
    else {
        newMappings = realloc(map->mappings, sizeof(struct MappedError) * newCapacity);
    }
    if (!newMappings) {
        return RERR_Error_CreateOutOfMemory();
    }
    map->mappings = newMappings;
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

    struct MappedError* newMappings;
    if (map->mapSize == 0) {
        free(map->mappings);
        newMappings = NULL;
    }
    else {
        newMappings = realloc(map->mappings,
            sizeof(struct MappedError) * newCapacity);
        if (!newMappings) {
            // Unexpected error, but we haven't broken anything.
            return;
        }
    }
    map->mappings = newMappings;
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

    struct MappedError* begin = map->mappings;
    struct MappedError* end = begin + map->mapSize;

    // Binary search for insertion point (like std::lower_bound)
    struct MappedError* left = begin;
    struct MappedError* right = end;
    while (left < right) {
        struct MappedError* middle = left + (right - left) / 2;
        if (MappedError_Compare(&key, middle) < 0) {
            right = middle;
        }
        else {
            left = middle;
        }
    }
    // Now left == right == insertion point

    memmove(left + 1, left, sizeof(struct MappedError) * (end - left));
    ++map->mapSize;
    left->thread = thread;
    left->code = code;
    left->error = error;

    return RERR_NO_ERROR;
}


// Mutex must be held; item must be in mapping; otherwise behavior undefined.
// It is assumed that the item has already been destroyed.
static void ErrorMap_Erase(RERR_ErrorMapPtr map, struct MappedError* item)
{
    size_t tailCount = map->mapSize - (item - map->mappings);
    memmove(item, item + 1, sizeof(struct MappedError) * tailCount);
    --map->mapSize;
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


RERR_ErrorPtr RERR_ErrorMap_Create(RERR_ErrorMapPtr* map,
    int32_t minMappedCode, int32_t maxMappedCode, int32_t noErrorCode)
{
    if (!map) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null address for error map pointer given");
    }
    *map = NULL;

    bool continuousRange = minMappedCode <= maxMappedCode;
    if ((continuousRange &&
        minMappedCode <= noErrorCode && noErrorCode <= maxMappedCode) ||
        !continuousRange &&
        (minMappedCode <= noErrorCode || noErrorCode <= maxMappedCode)) {

        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_MAP_INVALID_RANGE,
            "Mapped code range contains no-error code");
    }

    *map = calloc(1, sizeof(struct RERR_ErrorMap));
    if (!*map) {
        return RERR_Error_CreateOutOfMemory();
    }
    (*map)->noErrorCode = noErrorCode;
    (*map)->minCode = minMappedCode;
    (*map)->maxCode = maxMappedCode;
    InitMutex(&(*map)->mutex);
    (*map)->nextCode = minMappedCode;
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
        free(i->error);
    }

    free(map->mappings);
    map->mappings = NULL;

    free(map);
}


RERR_ErrorPtr RERR_ErrorMap_RegisterThreadLocal(RERR_ErrorMapPtr map,
    RERR_ErrorPtr error, int32_t* mappedCode)
{
    if (!map) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null error map pointer given");
    }
    if (!mappedCode) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null address for code given");
    }
    if (error == RERR_NO_ERROR) {
        *mappedCode = map->noErrorCode;
        return RERR_NO_ERROR;
    }

    ThreadID thread = GetThisThreadId();

    RERR_ErrorPtr ret = RERR_NO_ERROR;
    LockMutex(&map->mutex);
    {
        int32_t firstCandidate = map->nextCode;
        map->nextCode = IncrementCode(map->nextCode, map->minCode, map->maxCode);

        int32_t code = firstCandidate;
        for (;;) {
            struct MappedError* found = ErrorMap_Find(map, thread, code);
            if (!found) {
                ret = ErrorMap_Insert(map, thread, code, error);
                if (ret == RERR_NO_ERROR) {
                    *mappedCode = code;
                }
                break;
            }

            code = IncrementCode(code, map->minCode, map->maxCode);
            if (code == firstCandidate) {
                ret = RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
                    RERR_ECODE_MAP_FULL,
                    "Cannot assign an error code (available range exhausted)");
                break;
            }
        }
    }
    UnlockMutex(&map->mutex);
    return ret;
}


RERR_ErrorPtr RERR_ErrorMap_RetrieveThreadLocal(RERR_ErrorMapPtr map,
    int32_t mappedCode, RERR_ErrorPtr* error)
{
    if (!map) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null error map pointer given");
    }
    if (!error) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT,
            "Null address for error pointer given");
    }
    if (mappedCode == map->noErrorCode) {
        *error = RERR_NO_ERROR;
        return RERR_NO_ERROR;
    }

    RERR_ErrorPtr ret = RERR_NO_ERROR;
    LockMutex(&map->mutex);
    {
        struct MappedError* found = ErrorMap_Find(map, GetThisThreadId(), mappedCode);
        if (found) {
            *error = found->error;
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
