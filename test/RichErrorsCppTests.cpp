// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#include <catch2/catch.hpp>

#include "RichErrors/RichErrors.hpp"

#include "TestDefs.h"

#include <utility>

TEST_CASE("C++ Example") {
    RERR::Error noerror;
    REQUIRE(noerror.IsSuccess());

    REQUIRE(RERR::Error::OutOfMemory().IsError());
    REQUIRE(RERR::Error::OutOfMemory().IsOutOfMemory());
    REQUIRE(RERR::Error::OutOfMemory().GetDomain() == RERR::CriticalDomain());
    REQUIRE(RERR::Error::OutOfMemory().GetCode() == RERR_ECODE_OUT_OF_MEMORY);
    REQUIRE(RERR::Error::OutOfMemory().FormatCode() == "-1");

    const char *domain = TESTSTR("domain");
    RERR::Error e = RERR::RegisterDomain(domain, RERR_CodeFormat_I32);
    REQUIRE(e.IsSuccess());

    RERR::Error err(domain, 42, TESTSTR("msg"));

    RERR::Error err2 = std::move(err);
    REQUIRE(!err.IsError()); // Moved out
    REQUIRE(err2.IsError());

    RERR::Error wrapped(std::move(err2), TESTSTR("msg"));
    REQUIRE(!err2.IsError()); // Moved out
    REQUIRE(wrapped.IsError());
    REQUIRE(wrapped.HasCause());

    REQUIRE(wrapped.GetCauseChain().size() == 2);
    for (auto e2 : wrapped.GetCauseChain()) {
        REQUIRE(!e2.GetMessage().empty());
    }

    // C++ to C
    RERR::Error err3(TESTSTR("msg"));
    std::string msg3 = err3.GetMessage();
    RERR_ErrorPtr cptr = err3.ReleaseCPtr();
    REQUIRE(cptr != RERR_NO_ERROR);
    REQUIRE(!err3.IsError()); // Moved out

    // C to C++
    err3 = RERR::Error(std::move(cptr));
    REQUIRE(err3.GetMessage() == msg3);
    REQUIRE(cptr == RERR_NO_ERROR);

    // Throw
    const char *msg = TESTSTR("msg");
    try {
        RERR::Error(msg).ThrowIfError();
    } catch (RERR::Exception const &e) {
        REQUIRE(e.what() != nullptr);
        REQUIRE(e.Error().GetMessage() == msg);
    }

    const char *msg2 = TESTSTR("msg");
    try {
        RERR_ErrorPtr lvalueErr = RERR_Error_Create(msg2);
        // RERR::ThrowIfError(lvalueErr); // error
        RERR::ThrowIfError(std::move(lvalueErr));
    } catch (RERR::Exception const &e) {
        REQUIRE(e.Error().GetMessage() == msg2);
    }

    RERR::UnregisterAllDomains();
}
