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
    RERR_ErrorMapConfig config;
    RERR_ErrorMapPtr map;
    RERR_ErrorPtr err;

    RERR_ErrorMap_Destroy(NULL); // Must not crash

    config.minMappedCode = 1;
    config.maxMappedCode = 32767;
    config.noErrorCode = 0;
    config.outOfMemoryCode = -1;
    config.mapFailureCode = -2;

    // Error: null
    err = RERR_ErrorMap_Create(NULL, &config);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_NULL_ARGUMENT);
    RERR_Error_Destroy(err);

    // Typical
    err = RERR_ErrorMap_Create(&map, &config);
    REQUIRE(err == RERR_NO_ERROR);
    REQUIRE(map != NULL);
    REQUIRE(RERR_ErrorMap_IsRegisteredThreadLocal(map, config.noErrorCode));
    REQUIRE(RERR_ErrorMap_IsRegisteredThreadLocal(map, config.outOfMemoryCode));
    REQUIRE(RERR_ErrorMap_IsRegisteredThreadLocal(map, config.mapFailureCode));
    RERR_ErrorMap_Destroy(map);

    // All but special codes
    config.maxMappedCode = -3;
    err = RERR_ErrorMap_Create(&map, &config);
    REQUIRE(err == RERR_NO_ERROR);
    RERR_ErrorMap_Destroy(map);

    // Error: range contains no-error code
    config.minMappedCode = 0;
    config.maxMappedCode = 32767;
    err = RERR_ErrorMap_Create(&map, &config);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_MAP_INVALID_CONFIG);
    RERR_Error_Destroy(err);
}


TEST_CASE("Basic map and retrieve") {
    RERR_ErrorMapConfig config;
    RERR_ErrorMapPtr map;
    RERR_ErrorPtr err;

    config.minMappedCode = 1;
    config.maxMappedCode = 32767;
    config.noErrorCode = 0;
    config.outOfMemoryCode = -1;
    config.mapFailureCode = -2;

    err = RERR_ErrorMap_Create(&map, &config);
    REQUIRE(err == RERR_NO_ERROR);

    int32_t code;

    // Null map 
    // code = RERR_ErrorMap_RegisterThreadLocal(NULL, testErr); // Aborts

    // No-error
    code = RERR_ErrorMap_RegisterThreadLocal(map, RERR_NO_ERROR);
    REQUIRE(code == config.noErrorCode);

    RERR_ErrorPtr testErr = RERR_Error_Create("Test message");
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.minMappedCode);
    REQUIRE(RERR_ErrorMap_IsRegisteredThreadLocal(map, code));

    // Null map
    err = RERR_ErrorMap_RetrieveThreadLocal(NULL, code);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_NULL_ARGUMENT);
    RERR_Error_Destroy(err);

    // Normal retrieval
    err = RERR_ErrorMap_RetrieveThreadLocal(map, code);
    REQUIRE(err == testErr); // Note testErr is technically dangling
    RERR_Error_Destroy(err);
    REQUIRE(!RERR_ErrorMap_IsRegisteredThreadLocal(map, code));

    // Unregistered code
    err = RERR_ErrorMap_RetrieveThreadLocal(map, 42);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_MAP_INVALID_CODE);
    RERR_Error_Destroy(err);

    RERR_ErrorMap_Destroy(map);
}


TEST_CASE("Code exhaustion") {
    RERR_ErrorMapConfig config;
    RERR_ErrorMapPtr map;
    RERR_ErrorPtr err;
    int32_t code;
    RERR_ErrorPtr testErr;

    config.noErrorCode = 0;
    config.outOfMemoryCode = -1;
    config.mapFailureCode = -2;

    // Single available code
    config.minMappedCode = 1;
    config.maxMappedCode = 1;
    err = RERR_ErrorMap_Create(&map, &config);
    REQUIRE(err == RERR_NO_ERROR);

    testErr = RERR_Error_Create("Test message");
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.minMappedCode);

    testErr = RERR_Error_Create("Test message 2");
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.mapFailureCode);

    RERR_ErrorMap_Destroy(map);

    // Wrap around
    config.minMappedCode = INT32_MAX;
    config.maxMappedCode = INT32_MIN;
    err = RERR_ErrorMap_Create(&map, &config);
    REQUIRE(err == RERR_NO_ERROR);

    testErr = RERR_Error_Create("Test message");
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.minMappedCode);

    testErr = RERR_Error_Create("Test message 2");
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.maxMappedCode);

    testErr = RERR_Error_Create("Test message 3");
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.mapFailureCode);

    RERR_ErrorMap_Destroy(map);
}


TEST_CASE("Clear") {
    RERR_ErrorMap_ClearThreadLocal(NULL); // Must not crash

    RERR_ErrorMapConfig config;
    RERR_ErrorMapPtr map;
    RERR_ErrorPtr err;

    config.minMappedCode = 1;
    config.maxMappedCode = 32767;
    config.noErrorCode = 0;
    config.outOfMemoryCode = -1;
    config.mapFailureCode = -2;

    err = RERR_ErrorMap_Create(&map, &config);
    REQUIRE(err == RERR_NO_ERROR);

    RERR_ErrorPtr testErr = RERR_Error_Create("Test message");
    int32_t code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);

    RERR_ErrorMap_ClearThreadLocal(map);

    RERR_ErrorPtr retrieved = RERR_ErrorMap_RetrieveThreadLocal(map, code);
    REQUIRE(retrieved != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(retrieved), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(retrieved) == RERR_ECODE_MAP_INVALID_CODE);
    RERR_Error_Destroy(retrieved);

    RERR_ErrorMap_Destroy(map);
}
