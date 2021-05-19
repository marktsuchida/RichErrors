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

#include "RichErrors/InfoMap.hpp"


TEST_CASE("C++ Iteration", "[RERR::InfoMap]") {
    RERR::InfoMap m;

    for (auto const& item : m) {
        (void)item; // Avoid 'unused' warning
        REQUIRE(false); // Empty
    }

    m.SetString("k0", "value");
    m.SetBool("k1", true);
    m.SetI64("k2", -42);
    m.SetU64("k3", 42);
    m.SetF64("k4", 42.5);

    REQUIRE(m.Keys().size() == 5);

    for (auto const& item : m) {
        switch (item.GetType()) {
        case RERR_InfoValueTypeString:
            REQUIRE(item.GetKey() == "k0");
            REQUIRE(item.GetString() == "value");
            break;
        case RERR_InfoValueTypeBool:
            REQUIRE(item.GetKey() == "k1");
            REQUIRE(item.GetBool());
            break;
        case RERR_InfoValueTypeI64:
            REQUIRE(item.GetKey() == "k2");
            REQUIRE(item.GetI64() == -42);
            break;
        case RERR_InfoValueTypeU64:
            REQUIRE(item.GetKey() == "k3");
            REQUIRE(item.GetU64() == 42);
            break;
        case RERR_InfoValueTypeF64:
            REQUIRE(item.GetKey() == "k4");
            REQUIRE(item.GetF64() == 42.5);
            break;
        }
    }
}
