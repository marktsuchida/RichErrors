// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#include "../catch2/catch.hpp"

#include "RichErrors/Err2Code.hpp"

#include "TestDefs.h"


TEST_CASE("Err2Code C++ Example") {
    RERR::ErrorMap map(RERR::ErrorMap::Config().
        SetNoErrorCode(0).
        SetOutOfMemoryCode(-1).
        SetMapFailureCode(-2).
        SetMappedRange(1, 32767));

    const char* msg = TESTSTR("msg");
    int32_t code = map.RegisterThreadLocal(RERR::Error(msg));
    REQUIRE(code == 1);

    code = map.RegisterThreadLocal(RERR::Error());
    REQUIRE(code == 0);

    code = map.RegisterThreadLocal(RERR::Error::OutOfMemory());
    REQUIRE(code == -1);

    auto err = map.RetrieveThreadLocal(1);
    REQUIRE(err.GetMessage() == msg);
    err = map.RetrieveThreadLocal(0);
    REQUIRE(err.IsSuccess());
    err = map.RetrieveThreadLocal(-1);
    REQUIRE(err.IsOutOfMemory());
    err = map.RetrieveThreadLocal(-2);
    REQUIRE(err.IsError());
    REQUIRE(err.GetDomain() == RERR::RichErrorsDomain());
    REQUIRE(err.GetCode() == RERR_ECODE_MAP_FAILURE);
    err = map.RetrieveThreadLocal(42);
    REQUIRE(err.GetDomain() == RERR::RichErrorsDomain());
    REQUIRE(err.GetCode() == RERR_ECODE_MAP_INVALID_CODE);

    map.ClearThreadLocal();
    err = map.RetrieveThreadLocal(1);
    REQUIRE(err.GetDomain() == RERR::RichErrorsDomain());
    REQUIRE(err.GetCode() == RERR_ECODE_MAP_INVALID_CODE);
}
