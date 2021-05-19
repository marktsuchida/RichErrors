// RichErrors: A C library for rich (nested) error information.
// Author: Mark A. Tsuchida
//
// Copyright 2019-2021 Board of Regents of the University of Wisconsin System
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

#include "../catch2/catch.hpp"

#include "../../src/RichErrors/DynArray.h"


TEST_CASE("Create and destroy", "[RERR_DynArray]") {
    RERR_DynArrayPtr a = RERR_DynArray_Create(10);
    REQUIRE(a != nullptr);
    RERR_DynArray_Destroy(a);
}


TEST_CASE("Back insert and erase", "[RERR_DynArray]") {
    RERR_DynArrayPtr a = RERR_DynArray_Create(sizeof(int));
    REQUIRE(a != nullptr);

    for (int i = 0; i < 10; ++i) {
        REQUIRE(RERR_DynArray_GetSize(a) == i);
        auto p = RERR_DynArray_Insert(a, RERR_DynArray_End(a));
        *static_cast<int*>(p) = i + 42;
        REQUIRE(*static_cast<int*>(RERR_DynArray_Back(a)) == i + 42);
        REQUIRE(*static_cast<int*>(RERR_DynArray_Front(a)) == 42);
    }

    for (int i = 0; i < 10; ++i) {
        auto p = RERR_DynArray_At(a, i);
        REQUIRE(*static_cast<int*>(p) == i + 42);
    }

    for (int i = 0; i < 10; ++i) {
        REQUIRE(RERR_DynArray_GetSize(a) == 10 - i);
        auto p = RERR_DynArray_Erase(a, RERR_DynArray_Back(a));
        REQUIRE(p == RERR_DynArray_End(a));
    }
    REQUIRE(RERR_DynArray_GetSize(a) == 0);

    RERR_DynArray_Destroy(a);
}


TEST_CASE("Front insert and erase", "[RERR_DynArray]") {
    RERR_DynArrayPtr a = RERR_DynArray_Create(sizeof(int));
    REQUIRE(a != nullptr);

    for (int i = 0; i < 10; ++i) {
        REQUIRE(RERR_DynArray_GetSize(a) == i);
        auto p = RERR_DynArray_Insert(a, RERR_DynArray_Begin(a));
        *static_cast<int*>(p) = i + 42;
        REQUIRE(*static_cast<int*>(RERR_DynArray_Front(a)) == i + 42);
        REQUIRE(*static_cast<int*>(RERR_DynArray_Back(a)) == 42);
    }

    for (int i = 0; i < 10; ++i) {
        auto p = RERR_DynArray_At(a, i);
        REQUIRE(*static_cast<int*>(p) == (10 - i - 1) + 42);
    }

    for (int i = 0; i < 10; ++i) {
        REQUIRE(RERR_DynArray_GetSize(a) == 10 - i);
        auto p = RERR_DynArray_Erase(a, RERR_DynArray_Front(a));
        REQUIRE(p == RERR_DynArray_Begin(a));
    }
    REQUIRE(RERR_DynArray_GetSize(a) == 0);

    RERR_DynArray_Destroy(a);
}


extern "C" int CompareInt(const void* elem, const void* key) {
    const int* e = static_cast<const int*>(elem);
    const int* k = static_cast<const int*>(key);
    return *e - *k; // For testing only; assume no overflow
}


TEST_CASE("BSearch", "[RERR_DynArray]") {
    RERR_DynArrayPtr a = RERR_DynArray_Create(sizeof(int));
    REQUIRE(a != nullptr);

    int key = 42;
    REQUIRE(RERR_DynArray_BSearch(a, &key, CompareInt) == nullptr);
    REQUIRE(RERR_DynArray_BSearchInsertionPoint(a, &key, CompareInt) == RERR_DynArray_Begin(a));
    REQUIRE(RERR_DynArray_FindFirst(a, &key, CompareInt) == nullptr);

    size_t size = 7;
    for (size_t i = 0; i < size; ++i) {
        auto p = RERR_DynArray_Insert(a, RERR_DynArray_End(a));
        *static_cast<int*>(p) = static_cast<int>(i);
    }

    key = -1;
    REQUIRE(RERR_DynArray_BSearch(a, &key, CompareInt) == nullptr);
    REQUIRE(RERR_DynArray_BSearchInsertionPoint(a, &key, CompareInt) == RERR_DynArray_Begin(a));
    REQUIRE(RERR_DynArray_FindFirst(a, &key, CompareInt) == nullptr);

    key = 7;
    REQUIRE(RERR_DynArray_BSearch(a, &key, CompareInt) == nullptr);
    REQUIRE(RERR_DynArray_BSearchInsertionPoint(a, &key, CompareInt) == RERR_DynArray_End(a));
    REQUIRE(RERR_DynArray_FindFirst(a, &key, CompareInt) == nullptr);

    key = 8;
    REQUIRE(RERR_DynArray_BSearch(a, &key, CompareInt) == nullptr);
    REQUIRE(RERR_DynArray_BSearchInsertionPoint(a, &key, CompareInt) == RERR_DynArray_End(a));
    REQUIRE(RERR_DynArray_FindFirst(a, &key, CompareInt) == nullptr);

    SECTION("Find existing value") {
        int k = GENERATE(0, 1, 2, 3, 4, 5, 6);
        REQUIRE(RERR_DynArray_BSearch(a, &k, CompareInt) == RERR_DynArray_At(a, k));
        REQUIRE(RERR_DynArray_BSearchInsertionPoint(a, &k, CompareInt) == RERR_DynArray_At(a, k));
        REQUIRE(RERR_DynArray_FindFirst(a, &k, CompareInt) == RERR_DynArray_At(a, k));
    }

    RERR_DynArray_Destroy(a);
}
