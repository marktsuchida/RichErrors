// RichErrors: A C library for rich (nested) error information.
// Author: Mark A. Tsuchida
//
// Copyright 2019-2021 Board of Regents of the University of Wisconsin System
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

    const char* domain = TESTSTR("domain");
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
    for (auto e : wrapped.GetCauseChain()) {
        REQUIRE(!e.GetMessage().empty());
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
    const char* msg = TESTSTR("msg");
    try {
        RERR::Error(msg).ThrowIfError();
    }
    catch (RERR::Exception const& e) {
        REQUIRE(e.what() != nullptr);
        REQUIRE(e.Error().GetMessage() == msg);
    }

    const char* msg2 = TESTSTR("msg");
    try {
        RERR_ErrorPtr lvalueErr = RERR_Error_Create(msg2);
        // RERR::ThrowIfError(lvalueErr); // error
        RERR::ThrowIfError(std::move(lvalueErr));
    }
    catch (RERR::Exception const& e) {
        REQUIRE(e.Error().GetMessage() == msg2);
    }

    RERR::UnregisterAllDomains();
}
