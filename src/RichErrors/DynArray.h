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

// Common internal functions for dynamic arrays.
//
// These are functions _upon which_ a dynamic array implementation can be
// created; not a generic dynamic array implementation on their own.

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
constexpr
#endif
static inline size_t DynArray_CapacityForSize(size_t size, size_t fixedThresh, size_t linThreshBits)
{
    if (size == 0) {
        return 0;
    }

    if (size <= fixedThresh) {
        return fixedThresh;
    }

    size_t linThresh = (size_t)1 << linThreshBits;
    if (size >= linThresh) {
        size_t multiplier = ((size - 1) >> linThreshBits) + 1;
        return multiplier << linThreshBits;
    }

    size_t ret = 1 << 1;
    --size;
    while (size >>= 1) {
        ret <<= 1;
    }
    return ret;
}


// Precondition: pBegin != NULL
// Precondition: *pBegin must be NULL or previously allocated by this function
static inline bool DynArray_SetCapacity(void** pBegin, size_t newCap, size_t elemSize)
{
    if (newCap == 0) {
        free(*pBegin);
        *pBegin = 0;
        return true;
    }

    char* b = (char*)(*pBegin);
    if (!b) {
        *pBegin = malloc(newCap * elemSize);
        return *pBegin != NULL;
    }

    void* newBegin = realloc(b, newCap * elemSize);
    if (!newBegin) {
        return false;
    }

    *pBegin = newBegin;
    return true;
}


// Precondition: pBegin != NULL
// Precondition: *pBegin must be NULL or previously allocated
static inline void DynArray_Dealloc(void** pBegin)
{
    DynArray_SetCapacity(pBegin, 0, 0);
}


// Precondition: it <= end
static inline void DynArray_EraseElem(void* it, void* end, size_t* arrSize, size_t elemSize)
{
    --*arrSize;

    if (it == end) {
        return;
    }

    char* e = (char*)end;
    char* i = (char*)it;
    char* n = i + elemSize;

    memmove(i, n, e - n);
}


// Precondition: it <= end
static inline void DynArray_InsertElem(void* it, void* end, size_t* arrSize, size_t elemSize)
{
    ++*arrSize;

    if (it == end) {
        return;
    }

    char* e = (char*)end;
    char* i = (char*)it;
    char* n = i + elemSize;

    memmove(n, i, e - i);
}


/// Compare function pointer.
/**
 * A pointer to a function which compares an array element and a key (which
 * need not be of the same type).
 */
typedef int (*DynArray_CompareFunc)(const void* lhsElem, const void* rhsKey);


static inline void* DynArray_BSearch(void* begin, const void* key,
    size_t arrSize, size_t elemSize, DynArray_CompareFunc compare, bool exact)
{
    char* b = (char*)begin;
    char* e = b + arrSize * elemSize;
    char* left = b;
    char* right = e;
    for (;;) {
        if (left == right) {
            return exact ? NULL : left;
        }

        size_t count = (right - left) / elemSize;
        char* middle = left + count / 2 * elemSize;
        int cmp = compare(middle, key);
        if (cmp == 0) {
            return middle;
        }
        if (cmp > 0) { // key < middle
            right = middle;
        }
        else { // middle < key
            if (left == middle) {
                return exact ? NULL : right;
            }
            left = middle;
        }
    }
}


static inline void* DynArray_BSearchInsertionPoint(void* begin, const void* key,
    size_t arrSize, size_t elemSize, DynArray_CompareFunc compare)
{
    return DynArray_BSearch(begin, key, arrSize, elemSize, compare, false);
}


static inline void* DynArray_BSearchExact(void* begin, const void* key,
    size_t arrSize, size_t elemSize, DynArray_CompareFunc compare)
{
    return DynArray_BSearch(begin, key, arrSize, elemSize, compare, true);
}


#ifdef __cplusplus
} // extern "C"
#endif
