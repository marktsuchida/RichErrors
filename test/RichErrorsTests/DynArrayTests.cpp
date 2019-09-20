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

#include "../catch2/catch.hpp"

#include "../../src/RichErrors/DynArray.h"


TEST_CASE("Create and destroy", "[DynArray]") {
    DynArrayPtr a = DynArray_Create(10);
    REQUIRE(a != nullptr);
    DynArray_Destroy(a);
}


TEST_CASE("Back insert and erase", "[DynArray]") {
    DynArrayPtr a = DynArray_Create(sizeof(int));
    REQUIRE(a != nullptr);

    for (int i = 0; i < 10; ++i) {
        REQUIRE(DynArray_Size(a) == i);
        auto p = DynArray_Insert(a, DynArray_End(a));
        *static_cast<int*>(p) = i + 42;
        REQUIRE(*static_cast<int*>(DynArray_Back(a)) == i + 42);
        REQUIRE(*static_cast<int*>(DynArray_Front(a)) == 42);
    }

    for (int i = 0; i < 10; ++i) {
        auto p = DynArray_At(a, i);
        REQUIRE(*static_cast<int*>(p) == i + 42);
    }

    for (int i = 0; i < 10; ++i) {
        REQUIRE(DynArray_Size(a) == 10 - i);
        auto p = DynArray_Erase(a, DynArray_Back(a));
        REQUIRE(p == DynArray_End(a));
    }
    REQUIRE(DynArray_Size(a) == 0);

    DynArray_Destroy(a);
}


TEST_CASE("Front insert and erase", "[DynArray]") {
    DynArrayPtr a = DynArray_Create(sizeof(int));
    REQUIRE(a != nullptr);

    for (int i = 0; i < 10; ++i) {
        REQUIRE(DynArray_Size(a) == i);
        auto p = DynArray_Insert(a, DynArray_Begin(a));
        *static_cast<int*>(p) = i + 42;
        REQUIRE(*static_cast<int*>(DynArray_Front(a)) == i + 42);
        REQUIRE(*static_cast<int*>(DynArray_Back(a)) == 42);
    }

    for (int i = 0; i < 10; ++i) {
        auto p = DynArray_At(a, i);
        REQUIRE(*static_cast<int*>(p) == (10 - i - 1) + 42);
    }

    for (int i = 0; i < 10; ++i) {
        REQUIRE(DynArray_Size(a) == 10 - i);
        auto p = DynArray_Erase(a, DynArray_Front(a));
        REQUIRE(p == DynArray_Begin(a));
    }
    REQUIRE(DynArray_Size(a) == 0);

    DynArray_Destroy(a);
}


extern "C" int CompareInt(const void* elem, const void* key) {
    const int* e = static_cast<const int*>(elem);
    const int* k = static_cast<const int*>(key);
    return *e - *k; // For testing only; assume no overflow
}


TEST_CASE("BSearch", "[DynArray]") {
    DynArrayPtr a = DynArray_Create(sizeof(int));
    REQUIRE(a != nullptr);

    int key = 42;
    REQUIRE(DynArray_BSearchExact(a, &key, CompareInt) == nullptr);
    REQUIRE(DynArray_BSearchInsertionPoint(a, &key, CompareInt) == DynArray_Begin(a));
    REQUIRE(DynArray_FindFirstEqual(a, &key, CompareInt) == nullptr);

    size_t size = 7;
    for (int i = 0; i < size; ++i) {
        auto p = DynArray_Insert(a, DynArray_End(a));
        *static_cast<int*>(p) = i;
    }

    key = -1;
    REQUIRE(DynArray_BSearchExact(a, &key, CompareInt) == nullptr);
    REQUIRE(DynArray_BSearchInsertionPoint(a, &key, CompareInt) == DynArray_Begin(a));
    REQUIRE(DynArray_FindFirstEqual(a, &key, CompareInt) == nullptr);

    key = 7;
    REQUIRE(DynArray_BSearchExact(a, &key, CompareInt) == nullptr);
    REQUIRE(DynArray_BSearchInsertionPoint(a, &key, CompareInt) == DynArray_End(a));
    REQUIRE(DynArray_FindFirstEqual(a, &key, CompareInt) == nullptr);

    key = 8;
    REQUIRE(DynArray_BSearchExact(a, &key, CompareInt) == nullptr);
    REQUIRE(DynArray_BSearchInsertionPoint(a, &key, CompareInt) == DynArray_End(a));
    REQUIRE(DynArray_FindFirstEqual(a, &key, CompareInt) == nullptr);

    SECTION("Find existing value") {
        int k = GENERATE(0, 1, 2, 3, 4, 5, 6);
        REQUIRE(DynArray_BSearchExact(a, &k, CompareInt) == DynArray_At(a, k));
        REQUIRE(DynArray_BSearchInsertionPoint(a, &k, CompareInt) == DynArray_At(a, k));
        REQUIRE(DynArray_FindFirstEqual(a, &k, CompareInt) == DynArray_At(a, k));
    }

    DynArray_Destroy(a);
}
