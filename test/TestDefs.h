// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

// Automatically generate strings containing line number (facilitates debugging
// memory leaks)
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define TESTSTR(p) (p "-" TOSTRING(__LINE__))
