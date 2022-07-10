// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#include <catch2/catch.hpp>

#include "RichErrors/InfoMap.hpp"

TEST_CASE("C++ Iteration", "[RERR::InfoMap]") {
    RERR::InfoMap m;

    for (auto const &item : m) {
        (void)item;     // Avoid 'unused' warning
        REQUIRE(false); // Empty
    }

    m.SetString("k0", "value");
    m.SetBool("k1", true);
    m.SetI64("k2", -42);
    m.SetU64("k3", 42);
    m.SetF64("k4", 42.5);

    REQUIRE(m.Keys().size() == 5);

    for (auto const &item : m) {
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
