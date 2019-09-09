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

#include "RichErrors/RichErrors.hpp"

#include <utility>


TEST_CASE("Example") {
    RERR::Error noerror;
    REQUIRE(noerror.IsSuccess());

    RERR::Error e = RERR::RegisterDomain("TestDomain");
    REQUIRE(e.IsSuccess());

    RERR::Error err("TestDomain", 42, "Test message");

    RERR::Error err2 = std::move(err);
    REQUIRE(!err.IsError()); // Moved out
    REQUIRE(err2.IsError());

    RERR::Error wrapped(std::move(err2), "Higher-level message");
    REQUIRE(!err2.IsError()); // Moved out
    REQUIRE(wrapped.IsError());
    REQUIRE(wrapped.HasCause());

    for (auto e : wrapped.GetCauseChain()) {
        REQUIRE(!e.GetMessage().empty());
    }

    // C++ to C
    RERR::Error err3("Another message");
    std::string msg3 = err3.GetMessage();
    RERR_ErrorPtr cptr = err3.ReleaseCPtr();
    REQUIRE(cptr != RERR_NO_ERROR);
    REQUIRE(!err3.IsError()); // Moved out

    // C to C++
    err3 = RERR::Error(std::move(cptr));
    REQUIRE(err3.GetMessage() == msg3);

    // Non-owning copy can be made
    RERR::WeakError w1(err3);

    // But cannot take ownership of non-owning
    // RERR::Error e1(w1); // error

    // Not even from rvalue
    // RERR::Error e2(std::move(w1)); // error

    RERR::UnregisterAllDomains();
}
