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

#include "../../src/RichErrors/SmallMap.h"


TEST_CASE("Null behavior", "[SmallMap]") {
    SmallMap_Destroy(nullptr);

    SmallMapPtr m = SmallMap_Create();

    REQUIRE(SmallMap_Copy(nullptr) == nullptr);
    REQUIRE(SmallMap_UnfrozenCopy(nullptr) == nullptr);
    REQUIRE(SmallMap_FrozenCopy(nullptr) == nullptr);
    SmallMap_Freeze(nullptr);
    REQUIRE(SmallMap_IsFrozen(nullptr));
    REQUIRE(SmallMap_GetSize(nullptr) == 0);
    SmallMap_ReserveCapacity(nullptr, 0);
    SmallMap_ReserveCapacity(nullptr, 42);
    REQUIRE(SmallMap_Remove(nullptr, "key") == false);
    REQUIRE(SmallMap_Remove(nullptr, nullptr) == false);
    REQUIRE(SmallMap_Remove(m, nullptr) == false);
    SmallMap_Clear(nullptr);
    REQUIRE(SmallMap_HasKey(nullptr, "key") == false);
    REQUIRE(SmallMap_HasKey(nullptr, nullptr) == false);
    REQUIRE(SmallMap_HasKey(m, nullptr) == false);

    SmallMap_Destroy(m);
}


TEST_CASE("Lifecycle", "[SmallMap]") {
    SmallMapPtr m = SmallMap_Create();
    REQUIRE(m != nullptr);
    REQUIRE(SmallMap_GetSize(m) == 0);
    REQUIRE(!SmallMap_IsFrozen(m));
    SmallMap_Freeze(m);
    REQUIRE(SmallMap_IsFrozen(m));

    SmallMapPtr c = SmallMap_Copy(m);
    REQUIRE(c != nullptr);
    REQUIRE(c == m); // Shared immutable copy
    REQUIRE(SmallMap_IsFrozen(c));

    SmallMapPtr mc = SmallMap_UnfrozenCopy(m);
    REQUIRE(mc != nullptr);
    REQUIRE(mc != m);
    REQUIRE(!SmallMap_IsFrozen(mc));

    SmallMapPtr mc2 = SmallMap_Copy(mc);
    REQUIRE(mc2 != nullptr);
    REQUIRE(mc2 != mc);
    REQUIRE(!SmallMap_IsFrozen(mc2));

    SmallMap_Destroy(mc2);
    SmallMap_Destroy(mc);
    SmallMap_Destroy(c);
    SmallMap_Destroy(m);
}


TEST_CASE("Strings", "[SmallMap]") {
    SmallMapPtr m = SmallMap_Create();
    REQUIRE(m != nullptr);

    const char* key;
    const char* value;
    SmallMapError e;

    key = TESTSTR("key");
    value = TESTSTR("value");
    e = SmallMap_SetString(m, key, value);
    REQUIRE(e == SmallMapNoError);

    SmallMap_Clear(m);
    REQUIRE(SmallMap_GetSize(m) == 0);

    key = TESTSTR("key");
    value = TESTSTR("value");
    e = SmallMap_SetString(m, key, value);
    REQUIRE(e == SmallMapNoError);

    key = TESTSTR("key");
    value = TESTSTR("value");
    e = SmallMap_SetString(m, key, value);
    REQUIRE(e == SmallMapNoError);

    key = TESTSTR("key");
    value = TESTSTR("value");
    e = SmallMap_SetString(m, key, value);
    REQUIRE(e == SmallMapNoError);

    key = TESTSTR("key");
    value = TESTSTR("value");
    e = SmallMap_SetString(m, key, value);
    REQUIRE(e == SmallMapNoError);

    e = SmallMap_SetUniqueString(m, key, value);
    REQUIRE(e == SmallMapErrorKeyExists);

    REQUIRE(SmallMap_GetSize(m) == 4);

    REQUIRE(SmallMap_HasKey(m, key));
    REQUIRE(!SmallMap_HasKey(m, "foo"));

    SmallMapValueType t;
    REQUIRE(SmallMap_GetType(m, key, &t) == SmallMapNoError);
    REQUIRE(t == SmallMapValueTypeString);

    const char* s;
    e = SmallMap_GetString(m, key, &s);
    REQUIRE(e == SmallMapNoError);
    REQUIRE(strcmp(s, value) == 0);

    SmallMap_Destroy(m);
}


TEST_CASE("Numeic", "[SmallMap]") {
    SmallMapPtr m = SmallMap_Create();
    REQUIRE(m != nullptr);

    SmallMapError e;

    e = SmallMap_SetBool(m, "bool", true);
    REQUIRE(e == SmallMapNoError);
    e = SmallMap_SetUniqueBool(m, "bool", false);
    REQUIRE(e == SmallMapErrorKeyExists);

    e = SmallMap_SetI64(m, "i64", -42);
    REQUIRE(e == SmallMapNoError);
    e = SmallMap_SetUniqueI64(m, "i64", -43);
    REQUIRE(e == SmallMapErrorKeyExists);

    e = SmallMap_SetU64(m, "u64", 42);
    REQUIRE(e == SmallMapNoError);
    e = SmallMap_SetUniqueU64(m, "u64", 43);
    REQUIRE(e == SmallMapErrorKeyExists);

    e = SmallMap_SetF64(m, "f64", 42.5);
    REQUIRE(e == SmallMapNoError);
    e = SmallMap_SetUniqueF64(m, "f64", 43.5);
    REQUIRE(e == SmallMapErrorKeyExists);

    SmallMapValueType t;
    REQUIRE(SmallMap_GetType(m, "bool", &t) == SmallMapNoError);
    REQUIRE(t == SmallMapValueTypeBool);
    REQUIRE(SmallMap_GetType(m, "i64", &t) == SmallMapNoError);
    REQUIRE(t == SmallMapValueTypeI64);
    REQUIRE(SmallMap_GetType(m, "u64", &t) == SmallMapNoError);
    REQUIRE(t == SmallMapValueTypeU64);
    REQUIRE(SmallMap_GetType(m, "f64", &t) == SmallMapNoError);
    REQUIRE(t == SmallMapValueTypeF64);

    bool boolValue;
    e = SmallMap_GetBool(m, "bool", &boolValue);
    REQUIRE(e == SmallMapNoError);
    REQUIRE(boolValue == true);

    int64_t i64Value;
    e = SmallMap_GetI64(m, "i64", &i64Value);
    REQUIRE(e == SmallMapNoError);
    REQUIRE(i64Value == -42);

    uint64_t u64Value;
    e = SmallMap_GetU64(m, "u64", &u64Value);
    REQUIRE(e == SmallMapNoError);
    REQUIRE(u64Value == 42);

    double f64Value;
    e = SmallMap_GetF64(m, "f64", &f64Value);
    REQUIRE(e == SmallMapNoError);
    REQUIRE(f64Value == 42.5);

    SmallMap_Destroy(m);
}
