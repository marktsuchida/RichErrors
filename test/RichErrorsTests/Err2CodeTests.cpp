// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#include "../catch2/catch.hpp"

#include "RichErrors/Err2Code.h"

#include "TestDefs.h"

#include <cstring>


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

    RERR_ErrorPtr testErr = RERR_Error_Create(TESTSTR("msg"));
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

    testErr = RERR_Error_Create(TESTSTR("msg"));
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.minMappedCode);

    testErr = RERR_Error_Create(TESTSTR("msg"));
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.mapFailureCode);

    RERR_ErrorMap_Destroy(map);

    // Wrap around
    config.minMappedCode = INT32_MAX;
    config.maxMappedCode = INT32_MIN;
    err = RERR_ErrorMap_Create(&map, &config);
    REQUIRE(err == RERR_NO_ERROR);

    testErr = RERR_Error_Create(TESTSTR("msg"));
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.minMappedCode);

    testErr = RERR_Error_Create(TESTSTR("msg"));
    code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);
    REQUIRE(code == config.maxMappedCode);

    testErr = RERR_Error_Create(TESTSTR("msg"));
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

    RERR_ErrorPtr testErr = RERR_Error_Create(TESTSTR("msg"));
    int32_t code = RERR_ErrorMap_RegisterThreadLocal(map, testErr);

    RERR_ErrorMap_ClearThreadLocal(map);

    RERR_ErrorPtr retrieved = RERR_ErrorMap_RetrieveThreadLocal(map, code);
    REQUIRE(retrieved != RERR_NO_ERROR);
    REQUIRE(strcmp(RERR_Error_GetDomain(retrieved), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(retrieved) == RERR_ECODE_MAP_INVALID_CODE);
    RERR_Error_Destroy(retrieved);

    RERR_ErrorMap_Destroy(map);
}
