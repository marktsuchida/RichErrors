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

#include "RichErrors/Err2Code.hpp"


TEST_CASE("Err2Code C++ Example") {
    RERR::ErrorMap map(RERR::ErrorMap::Config().
        SetNoErrorCode(0).
        SetOutOfMemoryCode(-1).
        SetMapFailureCode(-2).
        SetMappedRange(1, 32767));

    int32_t code = map.RegisterThreadLocal(RERR::Error("test"));
    REQUIRE(code == 1);

    code = map.RegisterThreadLocal(RERR::Error());
    REQUIRE(code == 0);

    code = map.RegisterThreadLocal(RERR::Error::OutOfMemory());
    REQUIRE(code == -1);

    auto err = map.RetrieveThreadLocal(1);
    REQUIRE(err.GetMessage() == "test");
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
