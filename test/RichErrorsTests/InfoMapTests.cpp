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
#include "TestDefs.h"

#include "RichErrors/InfoMap.h"


TEST_CASE("Null behavior", "[RERR_InfoMap]") {
    RERR_InfoMap_Destroy(nullptr);

    RERR_InfoMapPtr m = RERR_InfoMap_Create();

    REQUIRE(RERR_InfoMap_Copy(nullptr) == nullptr);
    REQUIRE(RERR_InfoMap_MutableCopy(nullptr) == nullptr);
    REQUIRE(RERR_InfoMap_ImmutableCopy(nullptr) == nullptr);
    RERR_InfoMap_MakeImmutable(nullptr);
    REQUIRE(!RERR_InfoMap_IsMutable(nullptr));
    REQUIRE(RERR_InfoMap_GetSize(nullptr) == 0);
    RERR_InfoMap_ReserveCapacity(nullptr, 0);
    RERR_InfoMap_ReserveCapacity(nullptr, 42);
    RERR_InfoMap_Remove(nullptr, "key");
    RERR_InfoMap_Remove(nullptr, nullptr);
    RERR_InfoMap_Remove(m, nullptr);
    RERR_InfoMap_Clear(nullptr);
    REQUIRE(RERR_InfoMap_HasKey(nullptr, "key") == false);
    REQUIRE(RERR_InfoMap_HasKey(nullptr, nullptr) == false);
    REQUIRE(RERR_InfoMap_HasKey(m, nullptr) == false);

    RERR_InfoMap_Destroy(m);
}


TEST_CASE("Lifecycle", "[RERR_InfoMap]") {
    RERR_InfoMapPtr m = RERR_InfoMap_Create();
    REQUIRE(m != nullptr);
    REQUIRE(RERR_InfoMap_GetSize(m) == 0);
    REQUIRE(RERR_InfoMap_IsMutable(m));
    RERR_InfoMap_MakeImmutable(m);
    REQUIRE(!RERR_InfoMap_IsMutable(m));

    RERR_InfoMapPtr c = RERR_InfoMap_Copy(m);
    REQUIRE(c != nullptr);
    REQUIRE(c == m); // Shared immutable copy
    REQUIRE(!RERR_InfoMap_IsMutable(c));

    RERR_InfoMapPtr mc = RERR_InfoMap_MutableCopy(m);
    REQUIRE(mc != nullptr);
    REQUIRE(mc != m);
    REQUIRE(RERR_InfoMap_IsMutable(mc));

    RERR_InfoMapPtr mc2 = RERR_InfoMap_Copy(mc);
    REQUIRE(mc2 != nullptr);
    REQUIRE(mc2 != mc);
    REQUIRE(RERR_InfoMap_IsMutable(mc2));

    RERR_InfoMap_Destroy(mc2);
    RERR_InfoMap_Destroy(mc);
    RERR_InfoMap_Destroy(c);
    RERR_InfoMap_Destroy(m);
}


TEST_CASE("Strings", "[RERR_InfoMap]") {
    RERR_InfoMapPtr m = RERR_InfoMap_Create();
    REQUIRE(m != nullptr);

    const char* key;
    const char* value;

    key = TESTSTR("key");
    value = TESTSTR("value");
    RERR_InfoMap_SetString(m, key, value);

    RERR_InfoMap_Clear(m);
    REQUIRE(RERR_InfoMap_GetSize(m) == 0);

    key = TESTSTR("key");
    value = TESTSTR("value");
    RERR_InfoMap_SetString(m, key, value);

    key = TESTSTR("key");
    value = TESTSTR("value");
    RERR_InfoMap_SetString(m, key, value);

    key = TESTSTR("key");
    value = TESTSTR("value");
    RERR_InfoMap_SetString(m, key, value);

    key = TESTSTR("key");
    value = TESTSTR("value");
    RERR_InfoMap_SetString(m, key, value);

    REQUIRE(RERR_InfoMap_GetSize(m) == 4);

    REQUIRE(RERR_InfoMap_HasKey(m, key));
    REQUIRE(!RERR_InfoMap_HasKey(m, "foo"));

    REQUIRE(RERR_InfoMap_GetType(m, key) == RERR_InfoValueTypeString);

    const char* s;
    REQUIRE(RERR_InfoMap_GetString(m, key, &s));
    REQUIRE(strcmp(s, value) == 0);

    RERR_InfoMap_Destroy(m);
}


TEST_CASE("Numeic", "[RERR_InfoMap]") {
    RERR_InfoMapPtr m = RERR_InfoMap_Create();
    REQUIRE(m != nullptr);

    RERR_InfoMap_SetBool(m, "bool", true);
    RERR_InfoMap_SetI64(m, "i64", -42);
    RERR_InfoMap_SetU64(m, "u64", 42);
    RERR_InfoMap_SetF64(m, "f64", 42.5);

    REQUIRE(RERR_InfoMap_GetType(m, "bool") == RERR_InfoValueTypeBool);
    REQUIRE(RERR_InfoMap_GetType(m, "i64") == RERR_InfoValueTypeI64);
    REQUIRE(RERR_InfoMap_GetType(m, "u64") == RERR_InfoValueTypeU64);
    REQUIRE(RERR_InfoMap_GetType(m, "f64") == RERR_InfoValueTypeF64);

    bool boolValue;
    REQUIRE(RERR_InfoMap_GetBool(m, "bool", &boolValue));
    REQUIRE(boolValue == true);

    int64_t i64Value;
    REQUIRE(RERR_InfoMap_GetI64(m, "i64", &i64Value));
    REQUIRE(i64Value == -42);

    uint64_t u64Value;
    REQUIRE(RERR_InfoMap_GetU64(m, "u64", &u64Value));
    REQUIRE(u64Value == 42);

    double f64Value;
    REQUIRE(RERR_InfoMap_GetF64(m, "f64", &f64Value));
    REQUIRE(f64Value == 42.5);

    RERR_InfoMap_Destroy(m);
}


TEST_CASE("Out of memory", "[RERR_InfoMap]") {
    RERR_InfoMapPtr m = RERR_InfoMap_CreateOutOfMemory();
    REQUIRE(m != nullptr);
    REQUIRE(RERR_InfoMap_IsOutOfMemory(m));
    RERR_InfoMap_SetString(m, "key", "value"); // Should be ignored
    REQUIRE(RERR_InfoMap_IsEmpty(m));
    RERR_InfoMap_Destroy(m);

    m = RERR_InfoMap_Create();
    RERR_InfoMap_SetString(m, "key", "value");
    RERR_InfoMap_MakeOutOfMemory(m);
    REQUIRE(RERR_InfoMap_IsOutOfMemory(m));
    RERR_InfoMap_SetString(m, "key2", "value2");
    REQUIRE(RERR_InfoMap_IsEmpty(m));
    RERR_InfoMap_MakeImmutable(m); // Still works
    REQUIRE(!RERR_InfoMap_IsMutable(m));
    RERR_InfoMap_Destroy(m);
}


TEST_CASE("Programming errors", "[RERR_InfoMap]") {
    SECTION("No errors") {
        RERR_InfoMapPtr m = RERR_InfoMap_Create();
        REQUIRE(!RERR_InfoMap_HasProgrammingErrors(m));

        size_t size = RERR_InfoMap_GetProgrammingErrors(m, nullptr, 0);
        REQUIRE(size == 1);
        char msg[256];
        RERR_InfoMap_GetProgrammingErrors(m, msg, sizeof(msg));
        REQUIRE(strlen(msg) == 0);
        RERR_InfoMap_Destroy(m);
    }

    SECTION("Null key") {
        RERR_InfoMapPtr m = RERR_InfoMap_Create();
        REQUIRE(!RERR_InfoMap_HasProgrammingErrors(m));

        RERR_InfoMap_SetString(m, nullptr, "value");

        REQUIRE(RERR_InfoMap_HasProgrammingErrors(m));
        size_t size = RERR_InfoMap_GetProgrammingErrors(m, nullptr, 0);
        REQUIRE(size > 0);
        char* msg = (char*)malloc(size);
        REQUIRE(msg != nullptr);
        RERR_InfoMap_GetProgrammingErrors(m, msg, size);
        REQUIRE(strlen(msg) + 1 == size);
        free(msg);
        RERR_InfoMap_Destroy(m);
    }

    SECTION("Null value") {
        RERR_InfoMapPtr m = RERR_InfoMap_Create();
        REQUIRE(!RERR_InfoMap_HasProgrammingErrors(m));

        RERR_InfoMap_SetString(m, "key", nullptr);

        REQUIRE(RERR_InfoMap_HasProgrammingErrors(m));
        size_t size = RERR_InfoMap_GetProgrammingErrors(m, nullptr, 0);
        REQUIRE(size > 0);
        char* msg = (char*)malloc(size);
        REQUIRE(msg != nullptr);
        RERR_InfoMap_GetProgrammingErrors(m, msg, size);
        if (msg != nullptr) { // Avoid MSVC 'msg may be null' warning
            REQUIRE(strlen(msg) + 1 == size);
        }
        free(msg);
        RERR_InfoMap_Destroy(m);
    }

    SECTION("Mutate immutable") {
        RERR_InfoMapPtr m = RERR_InfoMap_Create();
        REQUIRE(!RERR_InfoMap_HasProgrammingErrors(m));

        RERR_InfoMap_MakeImmutable(m);
        RERR_InfoMap_SetString(m, "key", "value");

        REQUIRE(RERR_InfoMap_HasProgrammingErrors(m));
        size_t size = RERR_InfoMap_GetProgrammingErrors(m, nullptr, 0);
        REQUIRE(size > 0);
        char* msg = (char*)malloc(size);
        REQUIRE(msg != nullptr);
        RERR_InfoMap_GetProgrammingErrors(m, msg, size);
        if (msg != nullptr) { // Avoid MSVC 'msg may be null' warning
            REQUIRE(strlen(msg) + 1 == size);
        }
        free(msg);
        RERR_InfoMap_Destroy(m);
    }

    SECTION("All errors") {
        RERR_InfoMapPtr m = RERR_InfoMap_Create();
        REQUIRE(!RERR_InfoMap_HasProgrammingErrors(m));

        RERR_InfoMap_SetString(m, nullptr, "value");
        RERR_InfoMap_SetString(m, "key", nullptr);
        RERR_InfoMap_MakeImmutable(m);
        RERR_InfoMap_SetString(m, "key", "value");

        REQUIRE(RERR_InfoMap_HasProgrammingErrors(m));
        size_t size = RERR_InfoMap_GetProgrammingErrors(m, nullptr, 0);
        REQUIRE(size > 0);
        char* msg = (char*)malloc(size);
        REQUIRE(msg != nullptr);
        RERR_InfoMap_GetProgrammingErrors(m, msg, size);
        if (msg != nullptr) { // Avoid MSVC 'msg may be null' warning
            REQUIRE(strlen(msg) + 1 == size);
        }
        free(msg);
        RERR_InfoMap_Destroy(m);
    }
}


TEST_CASE("Iteration", "[RERR_InfoMap]") {
    RERR_InfoMapPtr m = RERR_InfoMap_Create();

    RERR_InfoMap_SetString(m, "k0", "value");
    RERR_InfoMap_SetBool(m, "k1", true);
    RERR_InfoMap_SetI64(m, "k2", -42);
    RERR_InfoMap_SetU64(m, "k3", 42);
    RERR_InfoMap_SetF64(m, "k4", 42.5);
    RERR_InfoMap_MakeImmutable(m);

    RERR_InfoMapIterator begin = RERR_InfoMap_Begin(m);
    RERR_InfoMapIterator end = RERR_InfoMap_End(m);
    for (RERR_InfoMapIterator it = begin; it != end; it = RERR_InfoMap_Advance(m, it)) {
        const char* key = RERR_InfoMapIterator_GetKey(it);
        REQUIRE(strncmp(key, "k", 1) == 0);
        REQUIRE(RERR_InfoMapIterator_GetType(it) != RERR_InfoValueTypeInvalid);
        switch (RERR_InfoMapIterator_GetType(it)) {
        case RERR_InfoValueTypeString:
            REQUIRE(strcmp(key, "k0") == 0);
            REQUIRE(strcmp(RERR_InfoMapIterator_GetString(it), "value") == 0);
            break;
        case RERR_InfoValueTypeBool:
            REQUIRE(strcmp(key, "k1") == 0);
            REQUIRE(RERR_InfoMapIterator_GetBool(it));
            break;
        case RERR_InfoValueTypeI64:
            REQUIRE(strcmp(key, "k2") == 0);
            REQUIRE(RERR_InfoMapIterator_GetI64(it) == -42);
            break;
        case RERR_InfoValueTypeU64:
            REQUIRE(strcmp(key, "k3") == 0);
            REQUIRE(RERR_InfoMapIterator_GetU64(it) == 42);
            break;
        case RERR_InfoValueTypeF64:
            REQUIRE(strcmp(key, "k4") == 0);
            REQUIRE(RERR_InfoMapIterator_GetF64(it) == 42.5);
            break;
        }
    }

    RERR_InfoMap_Destroy(m);
}
