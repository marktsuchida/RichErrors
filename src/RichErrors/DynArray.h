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

typedef struct DynArray* DynArrayPtr;

typedef int (*DynArrayCompareFunc)(const void* lhsElem, const void* rhsKey);


DynArrayPtr DynArray_Create(size_t elemSize);
void DynArray_Destroy(DynArrayPtr arr);
void DynArray_ReserveCapacity(DynArrayPtr arr, size_t capacity);
void DynArray_Clear(DynArrayPtr arr);
void* DynArray_Erase(DynArrayPtr arr, void* pos);
void* DynArray_Insert(DynArrayPtr arr, void* pos);
size_t DynArray_Size(DynArrayPtr arr);
void* DynArray_At(DynArrayPtr arr, size_t index);
void* DynArray_BSearchInsertionPoint(DynArrayPtr arr, const void* key,
    DynArrayCompareFunc compare);
void* DynArray_BSearchExact(DynArrayPtr arr, const void* key,
    DynArrayCompareFunc compare);

void* DynArray_Begin(DynArrayPtr arr);
void* DynArray_End(DynArrayPtr arr);
void* DynArray_Front(DynArrayPtr arr);
void* DynArray_Back(DynArrayPtr arr);
void* DynArray_Advance(DynArrayPtr arr, void* it);


#ifdef __cplusplus
} // extern "C"
#endif
