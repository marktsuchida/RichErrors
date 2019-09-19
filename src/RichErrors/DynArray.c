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

#include "DynArray.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


struct DynArray {
    char* elems;
    size_t size;
    size_t capacity;
    size_t elemSize;
    size_t fixedThresh;
    size_t linThreshBits;
    size_t capHysteresis;
};


static inline char* Begin(DynArrayPtr arr)
{
    return arr->elems;
}


static inline char* End(DynArrayPtr arr)
{
    return Begin(arr) + arr->size * arr->elemSize;
}


static inline char* Advance(DynArrayPtr arr, char* it)
{
    return it + arr->elemSize;
}


static inline size_t RoundUpToPowerOf2(size_t s)
{
    // https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2

    --s;
    s |= s >> 1;
    s |= s >> 2;
    s |= s >> 4;
    s |= s >> 8;
    s |= s >> 16;
    if (sizeof(size_t) > 4) {
        s |= s >> 32;
    }
    ++s;
    return s;
}


static inline size_t IdealCapacity(DynArrayPtr arr, size_t size)
{
    if (size == 0) {
        return 0;
    }

    if (size <= arr->fixedThresh) {
        return arr->fixedThresh;
    }

    size_t linThresh = (size_t)1 << arr->linThreshBits;
    if (size >= linThresh) {
        size_t multiplier = ((size - 1) >> arr->linThreshBits) + 1;
        return multiplier << arr->linThreshBits;
    }

    return RoundUpToPowerOf2(size);
}


// Precondition: newCap >= arr->size
static inline bool SetCapacity(DynArrayPtr arr, size_t newCap)
{
    if (newCap == 0) {
        free(arr->elems);
        arr->elems = NULL;
        arr->capacity = 0;
        return true;
    }

    char* newElems;
    if (!arr->elems) {
        newElems = malloc(newCap * arr->elemSize);
    }
    else {
        newElems = realloc(arr->elems, newCap * arr->elemSize);
    }
    if (!newElems) {
        return false;
    }

    arr->elems = newElems;
    arr->capacity = newCap;
    return true;
}


static inline bool EnsureCapacity(DynArrayPtr arr, size_t size)
{
    if (arr->capacity >= size) {
        return true;
    }
    return SetCapacity(arr, IdealCapacity(arr, size));
}


static inline void MaybeShrink(DynArrayPtr arr)
{
    size_t idealCap = IdealCapacity(arr, arr->size + arr->capHysteresis);
    if (arr->capacity > idealCap) {
        SetCapacity(arr, idealCap);
    }
}


static inline void* BSearch(DynArrayPtr arr, const void* key,
    DynArrayCompareFunc compare, bool exact)
{
    char* left = Begin(arr);
    char* right = End(arr);
    for (;;) {
        if (left == right) {
            return exact ? NULL : left;
        }

        size_t count = (right - left) / arr->elemSize;
        char* middle = left + count / 2 * arr->elemSize;
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


DynArrayPtr DynArray_Create(size_t elemSize)
{
    if (elemSize == 0) {
        return NULL;
    }

    DynArrayPtr ret = calloc(1, sizeof(struct DynArray));
    if (!ret) {
        return NULL;
    }

    ret->elemSize = elemSize;

    size_t s = RoundUpToPowerOf2(elemSize);
    size_t r = 0;
    while (s >>= 1) {
        ++r;
    }

    ret->fixedThresh = s > 64 ? 1 << r : 64 >> r;
    ret->linThreshBits = s > 4096 ? 1 << r : 4096 >> r;
    ret->capHysteresis = ret->fixedThresh;

    return ret;
}


void DynArray_Destroy(DynArrayPtr arr)
{
    arr->size = 0;
    free(arr->elems);
    arr->elems = NULL;
    arr->capacity = 0;
    free(arr);
}


void DynArray_ReserveCapacity(DynArrayPtr arr, size_t capacity)
{
    if (capacity < arr->size) {
        capacity = arr->size;
    }

    // Ignore failure because capacity reservation is hint only
    EnsureCapacity(arr, capacity);
}


void DynArray_Clear(DynArrayPtr arr)
{
    arr->size = 0;
    MaybeShrink(arr);
}


void* DynArray_Erase(DynArrayPtr arr, void* pos)
{
    char* it = (char*)pos;

    char* next = Advance(arr, it);
    memmove(pos, next, End(arr) - next);
    --arr->size;

    size_t saveOffset = it - Begin(arr);
    MaybeShrink(arr);
    it = Begin(arr) + saveOffset;

    return it;
}


void* DynArray_Insert(DynArrayPtr arr, void* pos)
{
    char* it = (char*)pos;
    size_t saveOffset = it - Begin(arr);
    bool ok = EnsureCapacity(arr, arr->size + 1);
    if (!ok) {
        return NULL;
    }
    it = Begin(arr) + saveOffset;

    char* next = Advance(arr, it);
    memmove(next, it, End(arr) - it);
    ++arr->size;

    return it;
}


size_t DynArray_Size(DynArrayPtr arr)
{
    return arr->size;
}


void* DynArray_At(DynArrayPtr arr, size_t index)
{
    return Begin(arr) + index * arr->elemSize;
}


void* DynArray_BSearchInsertionPoint(DynArrayPtr arr, const void* key,
    DynArrayCompareFunc compare)
{
    // TODO: For maximum performance, it may make sense to use linear search
    // when arr->size is below some threshold (to be determined empirically).

    return BSearch(arr, key, compare, false);
}


void* DynArray_BSearchExact(DynArrayPtr arr, const void* key,
    DynArrayCompareFunc compare)
{
    // TODO: For maximum performance, it may make sense to use linear search
    // when arr->size is below some threshold (to be determined empirically).

    return BSearch(arr, key, compare, true);
}


void* DynArray_Begin(DynArrayPtr arr)
{
    return Begin(arr);
}


void* DynArray_End(DynArrayPtr arr)
{
    return End(arr);
}


void* DynArray_Front(DynArrayPtr arr)
{
    if (arr->size > 0) {
        return Begin(arr);
    }
    return NULL;
}


void* DynArray_Back(DynArrayPtr arr)
{
    if (arr->size > 0) {
        return End(arr) - arr->elemSize;
    }
    return NULL;
}


void* DynArray_Advance(DynArrayPtr arr, void* it)
{
    return Advance(arr, it);
}
