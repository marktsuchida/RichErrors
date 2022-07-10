// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RERR_DynArray *RERR_DynArrayPtr;

typedef int (*RERR_DynArrayCompareFunc)(const void *lhsElem,
                                        const void *rhsKey);

RERR_DynArrayPtr RERR_DynArray_Create(size_t elemSize);
void RERR_DynArray_Destroy(RERR_DynArrayPtr arr);
void RERR_DynArray_ReserveCapacity(RERR_DynArrayPtr arr, size_t capacity);
void RERR_DynArray_Clear(RERR_DynArrayPtr arr);
void *RERR_DynArray_Erase(RERR_DynArrayPtr arr, void *pos);
void *RERR_DynArray_Insert(RERR_DynArrayPtr arr, void *pos);
size_t RERR_DynArray_GetSize(RERR_DynArrayPtr arr);
bool RERR_DynArray_IsEmpty(RERR_DynArrayPtr arr);
void *RERR_DynArray_At(RERR_DynArrayPtr arr, size_t index);
void *RERR_DynArray_BSearchInsertionPoint(RERR_DynArrayPtr arr,
                                          const void *key,
                                          RERR_DynArrayCompareFunc compare);
void *RERR_DynArray_BSearch(RERR_DynArrayPtr arr, const void *key,
                            RERR_DynArrayCompareFunc compare);
void *RERR_DynArray_FindFirst(RERR_DynArrayPtr arr, const void *key,
                              RERR_DynArrayCompareFunc compare);

// Iterators. Note that pointer arithmetic cannot be used due to being void*.
// (GCC allows void* arithmetic but result will be incorrect.)
void *RERR_DynArray_Begin(RERR_DynArrayPtr arr);
void *RERR_DynArray_End(RERR_DynArrayPtr arr);
void *RERR_DynArray_Front(RERR_DynArrayPtr arr);
void *RERR_DynArray_Back(RERR_DynArrayPtr arr);
void *RERR_DynArray_Advance(RERR_DynArrayPtr arr, void *it);

#ifdef __cplusplus
} // extern "C"
#endif
