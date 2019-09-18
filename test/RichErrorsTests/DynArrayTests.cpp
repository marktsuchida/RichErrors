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


// Zero -> Zero
static_assert(DynArray_CapacityForSize(0, 0, 0) == 0, "");
static_assert(DynArray_CapacityForSize(0, 1, 0) == 0, "");
static_assert(DynArray_CapacityForSize(0, 0, 1) == 0, "");
static_assert(DynArray_CapacityForSize(0, 1, 1) == 0, "");

// Fixed
static_assert(DynArray_CapacityForSize(1, 1, 0) == 1, "");
static_assert(DynArray_CapacityForSize(1, 2, 0) == 2, "");
static_assert(DynArray_CapacityForSize(2, 2, 0) == 2, "");
static_assert(DynArray_CapacityForSize(1, 1, 1) == 1, "");
static_assert(DynArray_CapacityForSize(1, 1, 2) == 1, "");
static_assert(DynArray_CapacityForSize(1, 2, 1) == 2, "");
static_assert(DynArray_CapacityForSize(2, 2, 1) == 2, "");
static_assert(DynArray_CapacityForSize(1, 2, 2) == 2, "");
static_assert(DynArray_CapacityForSize(2, 2, 2) == 2, "");

// Linear
static_assert(DynArray_CapacityForSize(1, 0, 0) == 1, "");
static_assert(DynArray_CapacityForSize(2, 0, 0) == 2, "");
static_assert(DynArray_CapacityForSize(1, 0, 1) == 2, "");
static_assert(DynArray_CapacityForSize(2, 0, 1) == 2, "");
static_assert(DynArray_CapacityForSize(3, 0, 1) == 4, "");
static_assert(DynArray_CapacityForSize(4, 0, 1) == 4, "");
static_assert(DynArray_CapacityForSize(5, 0, 1) == 6, "");
static_assert(DynArray_CapacityForSize(4, 0, 2) == 4, "");
static_assert(DynArray_CapacityForSize(5, 0, 2) == 8, "");
static_assert(DynArray_CapacityForSize(8, 0, 2) == 8, "");
static_assert(DynArray_CapacityForSize(9, 0, 2) == 12, "");

// Power of 2
static_assert(DynArray_CapacityForSize(1, 0, 2) == 2, "");
static_assert(DynArray_CapacityForSize(1, 0, 3) == 2, "");
static_assert(DynArray_CapacityForSize(2, 0, 3) == 2, "");
static_assert(DynArray_CapacityForSize(3, 0, 3) == 4, "");
static_assert(DynArray_CapacityForSize(4, 0, 3) == 4, "");
static_assert(DynArray_CapacityForSize(5, 0, 3) == 8, "");
static_assert(DynArray_CapacityForSize(7, 0, 3) == 8, "");
static_assert(DynArray_CapacityForSize(1, 0, 4) == 2, "");
static_assert(DynArray_CapacityForSize(2, 0, 4) == 2, "");
static_assert(DynArray_CapacityForSize(3, 0, 4) == 4, "");
static_assert(DynArray_CapacityForSize(4, 0, 4) == 4, "");
static_assert(DynArray_CapacityForSize(5, 0, 4) == 8, "");
static_assert(DynArray_CapacityForSize(7, 0, 4) == 8, "");
static_assert(DynArray_CapacityForSize(8, 0, 4) == 8, "");
static_assert(DynArray_CapacityForSize(9, 0, 4) == 16, "");
static_assert(DynArray_CapacityForSize(15, 0, 4) == 16, "");


TEST_CASE("SetCapacity", "[DynArray]") {
    struct Element {
        char v;
    };

    SECTION("No allocation for capacity 0") {
        Element* elements = nullptr;
        REQUIRE(DynArray_SetCapacity((void**)&elements, 0, sizeof(Element)));
        REQUIRE(elements == nullptr);
    }

    SECTION("Alloc, realloc, dealloc") {
        Element* elements = nullptr;

        REQUIRE(DynArray_SetCapacity((void**)&elements, 1, sizeof(Element)));
        REQUIRE(elements != nullptr);
        memset(elements, 0, sizeof(Element) * 1);

        REQUIRE(DynArray_SetCapacity((void**)&elements, 100, sizeof(Element)));
        REQUIRE(elements != nullptr);
        memset(elements, 0, sizeof(Element) * 100);

        REQUIRE(DynArray_SetCapacity((void**)&elements, 0, sizeof(Element)));
        REQUIRE(elements == nullptr);

        REQUIRE(DynArray_SetCapacity((void**)&elements, 1, sizeof(Element)));
        DynArray_Dealloc((void**)&elements);
        REQUIRE(elements == nullptr);
    }
}


TEST_CASE("EraseElem", "[DynArray]") {
    int* elements = nullptr;
    REQUIRE(DynArray_SetCapacity((void**)&elements, 3, sizeof(int)));

    size_t size = 3;
    for (int i = 0; i < size; ++i) {
        elements[i] = i;
    }
    DynArray_EraseElem(elements, elements + size, &size, sizeof(int));
    REQUIRE(size == 2);
    REQUIRE(elements[0] == 1);
    REQUIRE(elements[1] == 2);
    REQUIRE(elements[2] == 2); // Not touched

    size = 3;
    for (int i = 0; i < size; ++i) {
        elements[i] = i;
    }
    DynArray_EraseElem(elements + 1, elements + size, &size, sizeof(int));
    REQUIRE(size == 2);
    REQUIRE(elements[0] == 0);
    REQUIRE(elements[1] == 2);
    REQUIRE(elements[2] == 2); // Not touched

    size = 3;
    for (int i = 0; i < size; ++i) {
        elements[i] = i;
    }
    DynArray_EraseElem(elements + 2, elements + size, &size, sizeof(int));
    REQUIRE(size == 2);
    REQUIRE(elements[0] == 0);
    REQUIRE(elements[1] == 1);
    REQUIRE(elements[2] == 2); // Not touched

    size = 3;
    for (int i = 0; i < size; ++i) {
        elements[i] = i;
    }
    DynArray_EraseElem(elements + 3, elements + size, &size, sizeof(int));
    REQUIRE(size == 2);
    REQUIRE(elements[0] == 0);
    REQUIRE(elements[1] == 1);
    REQUIRE(elements[2] == 2);

    DynArray_Dealloc((void**)&elements);
}


TEST_CASE("InsertElem", "[DynArray]") {
    int* elements = nullptr;
    REQUIRE(DynArray_SetCapacity((void**)&elements, 3, sizeof(int)));

    size_t size = 2;
    for (int i = 0; i < 3; ++i) {
        elements[i] = i;
    }
    DynArray_InsertElem(elements, elements + size, &size, sizeof(int));
    REQUIRE(size == 3);
    REQUIRE(elements[0] == 0); // Not touched
    REQUIRE(elements[1] == 0);
    REQUIRE(elements[2] == 1);

    size = 2;
    for (int i = 0; i < 3; ++i) {
        elements[i] = i;
    }
    DynArray_InsertElem(elements + 1, elements + size, &size, sizeof(int));
    REQUIRE(size == 3);
    REQUIRE(elements[0] == 0);
    REQUIRE(elements[1] == 1); // Not touched
    REQUIRE(elements[2] == 1);

    size = 2;
    for (int i = 0; i < 3; ++i) {
        elements[i] = i;
    }
    DynArray_InsertElem(elements + 2, elements + size, &size, sizeof(int));
    REQUIRE(size == 3);
    REQUIRE(elements[0] == 0);
    REQUIRE(elements[1] == 1);
    REQUIRE(elements[2] == 2); // Not touched

    DynArray_Dealloc((void**)&elements);
}


extern "C" int CompareInt(const void* elem, const void* key) {
    const int* e = static_cast<const int*>(elem);
    const int* k = static_cast<const int*>(key);
    return *e - *k; // For testing only; assume no overflow
}


TEST_CASE("BSearch", "[DynArray]") {
    int* elements = nullptr;
    int key = 42;
    REQUIRE(DynArray_BSearchExact(elements, &key, 0, sizeof(int), CompareInt) == nullptr);
    REQUIRE(DynArray_BSearchInsertionPoint(elements, &key, 0, sizeof(int), CompareInt) == elements);

    size_t capacity = 7;
    size_t size = capacity;
    DynArray_SetCapacity((void**)&elements, capacity, sizeof(int));
    for (int i = 0; i < size; ++i) {
        elements[i] = i;
    }

    key = -1;
    REQUIRE(DynArray_BSearchExact(elements, &key, size, sizeof(int), CompareInt) == nullptr);
    REQUIRE(DynArray_BSearchInsertionPoint(elements, &key, size, sizeof(int), CompareInt) == elements);
    SECTION("Search finds existing values") {
        int k = GENERATE(0, 1, 2, 3, 4, 5, 6);
        REQUIRE(DynArray_BSearchExact(elements, &k, size, sizeof(int), CompareInt) == (int*)elements + k);
        REQUIRE(DynArray_BSearchInsertionPoint(elements, &k, size, sizeof(int), CompareInt) == (int*)elements + k);
    }
    key = 7;
    REQUIRE(DynArray_BSearchExact(elements, &key, size, sizeof(int), CompareInt) == nullptr);
    REQUIRE(DynArray_BSearchInsertionPoint(elements, &key, size, sizeof(int), CompareInt) == (int*)elements + 7);
    key = 8;
    REQUIRE(DynArray_BSearchInsertionPoint(elements, &key, size, sizeof(int), CompareInt) == (int*)elements + 7);

    DynArray_Dealloc((void**)&elements);
}
