// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#include "DynArray.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct RERR_DynArray {
    char *elems;
    size_t size;
    size_t capacity;
    size_t elemSize;
    size_t fixedThresh;
    size_t linThreshBits;
    size_t capHysteresis;
};

static inline char *Begin(RERR_DynArrayPtr arr) { return arr->elems; }

static inline char *End(RERR_DynArrayPtr arr) {
    return Begin(arr) + arr->size * arr->elemSize;
}

static inline char *Advance(RERR_DynArrayPtr arr, char *it) {
    return it + arr->elemSize;
}

static inline size_t RoundUpToPowerOf2(size_t s) {
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

static inline size_t IdealCapacity(RERR_DynArrayPtr arr, size_t size) {
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
static inline bool SetCapacity(RERR_DynArrayPtr arr, size_t newCap) {
    if (newCap == 0) {
        free(arr->elems);
        arr->elems = NULL;
        arr->capacity = 0;
        return true;
    }

    char *newElems;
    if (!arr->elems) {
        newElems = malloc(newCap * arr->elemSize);
    } else {
        newElems = realloc(arr->elems, newCap * arr->elemSize);
    }
    if (!newElems) {
        return false;
    }

    arr->elems = newElems;
    arr->capacity = newCap;
    return true;
}

static inline bool EnsureCapacity(RERR_DynArrayPtr arr, size_t size) {
    if (arr->capacity >= size) {
        return true;
    }
    return SetCapacity(arr, IdealCapacity(arr, size));
}

static inline void MaybeShrink(RERR_DynArrayPtr arr) {
    size_t idealCap = IdealCapacity(arr, arr->size + arr->capHysteresis);
    if (arr->capacity > idealCap) {
        SetCapacity(arr, idealCap);
    }
}

static inline void *BSearch(RERR_DynArrayPtr arr, const void *key,
                            RERR_DynArrayCompareFunc compare, bool exact) {
    char *left = Begin(arr);
    char *right = End(arr);
    for (;;) {
        if (left == right) {
            return exact ? NULL : left;
        }

        size_t count = (right - left) / arr->elemSize;
        char *middle = left + count / 2 * arr->elemSize;
        int cmp = compare(middle, key);
        if (cmp == 0) {
            return middle;
        }
        if (cmp > 0) { // key < middle
            right = middle;
        } else { // middle < key
            if (left == middle) {
                return exact ? NULL : right;
            }
            left = middle;
        }
    }
}

static inline void *LinSearch(RERR_DynArrayPtr arr, const void *key,
                              RERR_DynArrayCompareFunc compare, bool exact) {
    char *begin = Begin(arr);
    char *end = End(arr);
    for (char *it = begin; it != end; it = Advance(arr, it)) {
        int cmp = compare(it, key);
        if (cmp == 0) {
            return it;
        }
        if (!exact && cmp < 0) { // First element > key
            return it;
        }
    }
    if (exact) {
        return NULL;
    } else {
        return end;
    }
}

RERR_DynArrayPtr RERR_DynArray_Create(size_t elemSize) {
    if (elemSize == 0) {
        return NULL;
    }

    RERR_DynArrayPtr ret = calloc(1, sizeof(struct RERR_DynArray));
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

void RERR_DynArray_Destroy(RERR_DynArrayPtr arr) {
    arr->size = 0;
    free(arr->elems);
    arr->elems = NULL;
    arr->capacity = 0;
    free(arr);
}

void RERR_DynArray_ReserveCapacity(RERR_DynArrayPtr arr, size_t capacity) {
    if (capacity < arr->size) {
        capacity = arr->size;
    }

    // Ignore failure because capacity reservation is hint only
    EnsureCapacity(arr, capacity);
}

void RERR_DynArray_Clear(RERR_DynArrayPtr arr) {
    arr->size = 0;
    MaybeShrink(arr);
}

void *RERR_DynArray_Erase(RERR_DynArrayPtr arr, void *pos) {
    char *it = (char *)pos;

    char *next = Advance(arr, it);
    memmove(pos, next, End(arr) - next);
    --arr->size;

    size_t saveOffset = it - Begin(arr);
    MaybeShrink(arr);
    it = Begin(arr) + saveOffset;

    return it;
}

void *RERR_DynArray_Insert(RERR_DynArrayPtr arr, void *pos) {
    char *it = (char *)pos;
    size_t saveOffset = it - Begin(arr);
    bool ok = EnsureCapacity(arr, arr->size + 1);
    if (!ok) {
        return NULL;
    }
    it = Begin(arr) + saveOffset;

    char *next = Advance(arr, it);
    memmove(next, it, End(arr) - it);
    ++arr->size;

    return it;
}

size_t RERR_DynArray_GetSize(RERR_DynArrayPtr arr) { return arr->size; }

bool RERR_DynArray_IsEmpty(RERR_DynArrayPtr arr) { return arr->size == 0; }

void *RERR_DynArray_At(RERR_DynArrayPtr arr, size_t index) {
    return Begin(arr) + index * arr->elemSize;
}

void *RERR_DynArray_BSearchInsertionPoint(RERR_DynArrayPtr arr,
                                          const void *key,
                                          RERR_DynArrayCompareFunc compare) {
    // TODO: For maximum performance, it may make sense to use linear search
    // when arr->size is below some threshold (to be determined empirically).

    return BSearch(arr, key, compare, false);
}

void *RERR_DynArray_BSearch(RERR_DynArrayPtr arr, const void *key,
                            RERR_DynArrayCompareFunc compare) {
    // TODO: For maximum performance, it may make sense to use linear search
    // when arr->size is below some threshold (to be determined empirically).

    return BSearch(arr, key, compare, true);
}

void *RERR_DynArray_FindFirst(RERR_DynArrayPtr arr, const void *key,
                              RERR_DynArrayCompareFunc compare) {
    return LinSearch(arr, key, compare, true);
}

void *RERR_DynArray_Begin(RERR_DynArrayPtr arr) { return Begin(arr); }

void *RERR_DynArray_End(RERR_DynArrayPtr arr) { return End(arr); }

void *RERR_DynArray_Front(RERR_DynArrayPtr arr) {
    if (arr->size > 0) {
        return Begin(arr);
    }
    return NULL;
}

void *RERR_DynArray_Back(RERR_DynArrayPtr arr) {
    if (arr->size > 0) {
        return End(arr) - arr->elemSize;
    }
    return NULL;
}

void *RERR_DynArray_Advance(RERR_DynArrayPtr arr, void *it) {
    return Advance(arr, it);
}
