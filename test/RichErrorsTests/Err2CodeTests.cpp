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

#include "RichErrors/Err2Code.h"


TEST_CASE("Map creation parameters") {
    RERR_ErrorMapPtr map;
    RERR_ErrorPtr err;

    RERR_ErrorMap_Destroy(NULL); // Must not crash

    // Error: null
    err = RERR_ErrorMap_Create(NULL, 1, 32767, 0);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_NULL_ARGUMENT);
    RERR_Error_Destroy(err);

    // Typical
    err = RERR_ErrorMap_Create(&map, 1, 32767, 0);
    REQUIRE(err == RERR_NO_ERROR);
    REQUIRE(map != NULL);
    RERR_ErrorMap_Destroy(map);

    // All but zero
    err = RERR_ErrorMap_Create(&map, 1, -1, 0);
    REQUIRE(err == RERR_NO_ERROR);
    RERR_ErrorMap_Destroy(map);

    // Error: range contains no-error code
    err = RERR_ErrorMap_Create(&map, -1, 1, 0);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_MAP_INVALID_RANGE);
    RERR_Error_Destroy(err);
}


TEST_CASE("Basic map and retrieve") {
    RERR_ErrorMapPtr map;
    RERR_ErrorPtr err;

    err = RERR_ErrorMap_Create(&map, 1, 32767, 0);
    REQUIRE(err == RERR_NO_ERROR);

    int32_t code;
    RERR_ErrorPtr testErr = RERR_Error_Create("Test message");

    // Null map
    err = RERR_ErrorMap_RegisterThreadLocal(NULL, testErr, &code);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_NULL_ARGUMENT);
    RERR_Error_Destroy(err);

    // No-error
    code = 42;
    err = RERR_ErrorMap_RegisterThreadLocal(map, RERR_NO_ERROR, &code);
    REQUIRE(err == RERR_NO_ERROR);
    REQUIRE(code == 0);

    err = RERR_ErrorMap_RegisterThreadLocal(map, testErr, &code);
    REQUIRE(err == RERR_NO_ERROR);
    REQUIRE(code == 1);

    RERR_ErrorPtr retrieved;

    // Null map
    err = RERR_ErrorMap_RetrieveThreadLocal(NULL, code, &retrieved);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_NULL_ARGUMENT);
    RERR_Error_Destroy(err);

    // Null destination
    err = RERR_ErrorMap_RetrieveThreadLocal(map, code, NULL);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_NULL_ARGUMENT);
    RERR_Error_Destroy(err);

    // Normal retrieval
    err = RERR_ErrorMap_RetrieveThreadLocal(map, code, &retrieved);
    REQUIRE(err == RERR_NO_ERROR);
    REQUIRE(retrieved == testErr); // No copying took place
    RERR_Error_Destroy(retrieved);

    // Unregistered code
    err = RERR_ErrorMap_RetrieveThreadLocal(map, 42, &retrieved);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_MAP_INVALID_CODE);
    RERR_Error_Destroy(err);

    RERR_ErrorMap_Destroy(map);
}


TEST_CASE("Code exhaustion") {
    RERR_ErrorMapPtr map;
    RERR_ErrorPtr err;
    int32_t code;
    RERR_ErrorPtr testErr;

    err = RERR_ErrorMap_Create(&map, 1, 1, 0);
    REQUIRE(err == RERR_NO_ERROR);

    testErr = RERR_Error_Create("Test message");
    err = RERR_ErrorMap_RegisterThreadLocal(map, testErr, &code);
    REQUIRE(err == RERR_NO_ERROR);
    REQUIRE(code == 1);

    testErr = RERR_Error_Create("Test message 2");
    err = RERR_ErrorMap_RegisterThreadLocal(map, testErr, &code);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_MAP_FULL);
    RERR_Error_Destroy(err);
    RERR_Error_Destroy(testErr);

    RERR_ErrorMap_Destroy(map);

    // Wrap around
    err = RERR_ErrorMap_Create(&map, INT32_MAX, INT32_MIN, 0);
    REQUIRE(err == RERR_NO_ERROR);

    testErr = RERR_Error_Create("Test message");
    err = RERR_ErrorMap_RegisterThreadLocal(map, testErr, &code);
    REQUIRE(err == RERR_NO_ERROR);
    REQUIRE(code == INT32_MAX);

    testErr = RERR_Error_Create("Test message 2");
    err = RERR_ErrorMap_RegisterThreadLocal(map, testErr, &code);
    REQUIRE(err == RERR_NO_ERROR);
    REQUIRE(code == INT32_MIN);

    testErr = RERR_Error_Create("Test message 3");
    err = RERR_ErrorMap_RegisterThreadLocal(map, testErr, &code);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_MAP_FULL);
    RERR_Error_Destroy(err);
    RERR_Error_Destroy(testErr);

    RERR_ErrorMap_Destroy(map);
}


TEST_CASE("Clear") {
    RERR_ErrorMap_ClearThreadLocal(NULL); // Must not crash

    RERR_ErrorMapPtr map;
    RERR_ErrorPtr err;

    err = RERR_ErrorMap_Create(&map, 1, 32767, 0);
    REQUIRE(err == RERR_NO_ERROR);

    RERR_ErrorPtr testErr = RERR_Error_Create("Test message");

    int32_t code;
    err = RERR_ErrorMap_RegisterThreadLocal(map, testErr, &code);
    REQUIRE(err == RERR_NO_ERROR);

    RERR_ErrorMap_ClearThreadLocal(map);

    RERR_ErrorPtr retrieved;
    err = RERR_ErrorMap_RetrieveThreadLocal(map, code, &retrieved);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_MAP_INVALID_CODE);
    RERR_Error_Destroy(err);

    RERR_ErrorMap_Destroy(map);
}
