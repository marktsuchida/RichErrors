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

#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RERR_DynArray* RERR_DynArrayPtr;

typedef int (*RERR_DynArrayCompareFunc)(const void* lhsElem, const void* rhsKey);


RERR_DynArrayPtr RERR_DynArray_Create(size_t elemSize);
void RERR_DynArray_Destroy(RERR_DynArrayPtr arr);
void RERR_DynArray_ReserveCapacity(RERR_DynArrayPtr arr, size_t capacity);
void RERR_DynArray_Clear(RERR_DynArrayPtr arr);
void* RERR_DynArray_Erase(RERR_DynArrayPtr arr, void* pos);
void* RERR_DynArray_Insert(RERR_DynArrayPtr arr, void* pos);
size_t RERR_DynArray_GetSize(RERR_DynArrayPtr arr);
void* RERR_DynArray_At(RERR_DynArrayPtr arr, size_t index);
void* RERR_DynArray_BSearchInsertionPoint(RERR_DynArrayPtr arr, const void* key,
    RERR_DynArrayCompareFunc compare);
void* RERR_DynArray_BSearch(RERR_DynArrayPtr arr, const void* key,
    RERR_DynArrayCompareFunc compare);
void* RERR_DynArray_FindFirst(RERR_DynArrayPtr arr, const void* key,
    RERR_DynArrayCompareFunc compare);

void* RERR_DynArray_Begin(RERR_DynArrayPtr arr);
void* RERR_DynArray_End(RERR_DynArrayPtr arr);
void* RERR_DynArray_Front(RERR_DynArrayPtr arr);
void* RERR_DynArray_Back(RERR_DynArrayPtr arr);
void* RERR_DynArray_Advance(RERR_DynArrayPtr arr, void* it);


#ifdef __cplusplus
} // extern "C"
#endif
