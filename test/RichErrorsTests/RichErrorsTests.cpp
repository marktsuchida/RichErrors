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

#include "RichErrors/RichErrors.h"

#include "TestDefs.h"

#include <cstring>


TEST_CASE("No-error should behave normally") {
    RERR_ErrorPtr noerr = RERR_NO_ERROR;
    REQUIRE(noerr == NULL);
    REQUIRE(!RERR_Error_HasCode(noerr));
    REQUIRE(RERR_Error_GetDomain(noerr) != NULL);
    REQUIRE(strcmp(RERR_Error_GetDomain(noerr), "") == 0);
    REQUIRE(RERR_Error_GetCode(noerr) == 0);
    REQUIRE(RERR_Error_GetMessage(noerr) != NULL);
    REQUIRE(strlen(RERR_Error_GetMessage(noerr)) > 0);
    REQUIRE(!RERR_Error_HasCause(noerr));
    REQUIRE(RERR_Error_GetCause(noerr) == RERR_NO_ERROR);
    RERR_Error_Destroy(noerr);
}


TEST_CASE("Out-of-memory error should behave normally") {
    RERR_ErrorPtr oom = RERR_Error_CreateOutOfMemory();
    REQUIRE(oom != NULL);
    REQUIRE(RERR_Error_HasCode(oom));
    REQUIRE(strcmp(RERR_Error_GetDomain(oom), RERR_DOMAIN_CRITICAL) == 0);
    REQUIRE(RERR_Error_GetCode(oom) == RERR_ECODE_OUT_OF_MEMORY);
    REQUIRE(RERR_Error_GetMessage(oom) != NULL);
    REQUIRE(!RERR_Error_HasCause(oom));
    REQUIRE(RERR_Error_GetCause(oom) == NULL);
    RERR_Error_Destroy(oom);
}


TEST_CASE("Reject duplicate domain registration") {
    const char* d = TESTSTR("domain");
    RERR_ErrorPtr err_first = RERR_Domain_Register(d, RERR_CodeFormat_I32);
    RERR_ErrorPtr err_second = RERR_Domain_Register(d, RERR_CodeFormat_I32);
    RERR_Domain_UnregisterAll();

    REQUIRE(err_first == RERR_NO_ERROR);
    REQUIRE(err_second != RERR_NO_ERROR);
    RERR_Error_Destroy(err_second);
}


TEST_CASE("Reject invalid domain registration") {
    RERR_ErrorPtr err_null = RERR_Domain_Register(NULL, RERR_CodeFormat_I32);
    REQUIRE(err_null != RERR_NO_ERROR);
    REQUIRE(RERR_Error_HasCode(err_null));
    REQUIRE(strcmp(RERR_Error_GetDomain(err_null), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err_null) == RERR_ECODE_NULL_ARGUMENT);
    REQUIRE(RERR_Error_GetMessage(err_null) != NULL);
    RERR_Error_Destroy(err_null);

    RERR_ErrorPtr err_empty = RERR_Domain_Register("", RERR_CodeFormat_I32);
    REQUIRE(err_empty != RERR_NO_ERROR);
    REQUIRE(RERR_Error_HasCode(err_empty));
    REQUIRE(strcmp(RERR_Error_GetDomain(err_empty), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err_empty) == RERR_ECODE_DOMAIN_NAME_EMPTY);
    REQUIRE(RERR_Error_GetMessage(err_empty) != NULL);
    RERR_Error_Destroy(err_empty);

    // Domain name limited to 63 chars
    char longname[65];
    memset(longname, 'a', sizeof(longname));
    longname[sizeof(longname) - 1] = '\0';
    RERR_ErrorPtr err_too_long = RERR_Domain_Register(longname, RERR_CodeFormat_I32);
    REQUIRE(err_too_long != RERR_NO_ERROR);
    REQUIRE(RERR_Error_HasCode(err_too_long));
    REQUIRE(strcmp(RERR_Error_GetDomain(err_too_long), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err_too_long) == RERR_ECODE_DOMAIN_NAME_TOO_LONG);
    REQUIRE(RERR_Error_GetMessage(err_too_long) != NULL);
    RERR_Error_Destroy(err_too_long);

    longname[sizeof(longname) - 2] = '\0';
    RERR_ErrorPtr err_not_too_long = RERR_Domain_Register(longname, RERR_CodeFormat_I32);
    REQUIRE(err_not_too_long == RERR_NO_ERROR);

    RERR_ErrorPtr err_exists = RERR_Domain_Register(longname, RERR_CodeFormat_I32);
    REQUIRE(err_exists != RERR_NO_ERROR);
    REQUIRE(RERR_Error_HasCode(err_exists));
    REQUIRE(strcmp(RERR_Error_GetDomain(err_exists), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err_exists) == RERR_ECODE_DOMAIN_ALREADY_EXISTS);
    REQUIRE(RERR_Error_GetMessage(err_exists) != NULL);
    RERR_Error_Destroy(err_exists);

    RERR_ErrorPtr err_invalid = RERR_Domain_Register("\b", RERR_CodeFormat_I32);
    REQUIRE(err_invalid != RERR_NO_ERROR);
    REQUIRE(RERR_Error_HasCode(err_invalid));
    REQUIRE(strcmp(RERR_Error_GetDomain(err_invalid), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err_invalid) == RERR_ECODE_DOMAIN_NAME_INVALID);
    REQUIRE(RERR_Error_GetMessage(err_invalid) != NULL);
    RERR_Error_Destroy(err_invalid);

    // The system domain always exists
    RERR_ErrorPtr err_system = RERR_Domain_Register(RERR_DOMAIN_RICHERRORS, RERR_CodeFormat_I32);
    REQUIRE(err_system != RERR_NO_ERROR);
    REQUIRE(RERR_Error_HasCode(err_system));
    REQUIRE(strcmp(RERR_Error_GetDomain(err_system), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err_system) == RERR_ECODE_DOMAIN_ALREADY_EXISTS);
    REQUIRE(RERR_Error_GetMessage(err_system) != NULL);
    RERR_Error_Destroy(err_system);

    RERR_Domain_UnregisterAll();
}


TEST_CASE("Create without code") {
    const char* msg = TESTSTR("msg");
    RERR_ErrorPtr err = RERR_Error_Create(msg);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(!RERR_Error_HasCode(err));
    REQUIRE(RERR_Error_GetDomain(err) != NULL);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), "") == 0);
    REQUIRE(RERR_Error_GetCode(err) == 0);
    REQUIRE(RERR_Error_GetMessage(err) != NULL);
    REQUIRE(RERR_Error_GetMessage(err) != msg); // Must be a copy
    REQUIRE(strcmp(RERR_Error_GetMessage(err), msg) == 0);
    REQUIRE(!RERR_Error_HasCause(err));
    RERR_Error_Destroy(err);

    // Message can be null
    err = RERR_Error_Create(NULL);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(RERR_Error_GetMessage(err) != NULL);
    REQUIRE(strlen(RERR_Error_GetMessage(err)) > 0);
    RERR_Error_Destroy(err);

    // Message can be empty but is not empty when retrieved
    err = RERR_Error_Create("");
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(RERR_Error_GetMessage(err) != NULL);
    REQUIRE(strlen(RERR_Error_GetMessage(err)) > 0);
    RERR_Error_Destroy(err);
}


TEST_CASE("Create with code") {
    const char* domain = TESTSTR("domain");
    const char* msg = TESTSTR("msg");
    RERR_ErrorPtr e = RERR_Domain_Register(domain, RERR_CodeFormat_I32);
    REQUIRE(e == RERR_NO_ERROR);

    RERR_ErrorPtr err = RERR_Error_CreateWithCode(domain, 42, msg);
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(RERR_Error_HasCode(err));
    REQUIRE(RERR_Error_GetDomain(err) != NULL);
    REQUIRE(RERR_Error_GetDomain(err) != domain); // Must be a copy
    REQUIRE(strcmp(RERR_Error_GetDomain(err), domain) == 0);
    REQUIRE(RERR_Error_GetCode(err) == 42);
    REQUIRE(RERR_Error_GetMessage(err) != NULL);
    REQUIRE(RERR_Error_GetMessage(err) != msg); // Must be a copy
    REQUIRE(strcmp(RERR_Error_GetMessage(err), msg) == 0);
    REQUIRE(!RERR_Error_HasCause(err));
    RERR_Error_Destroy(err);

    // Cannot create with unregistered domain
    err = RERR_Error_CreateWithCode("bad domain", 42, TESTSTR("msg"));
    REQUIRE(RERR_Error_GetDomain(err) != NULL);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_DOMAIN_NOT_REGISTERED);
    RERR_Error_Destroy(err);

    // Create without code if domain == NULL and code == 0
    err = RERR_Error_CreateWithCode(NULL, 0, TESTSTR("msg"));
    REQUIRE(err != RERR_NO_ERROR);
    REQUIRE(!RERR_Error_HasCode(err));
    RERR_Error_Destroy(err);

    // But reject domain == NULL with nonzero code (likely a programming error)
    err = RERR_Error_CreateWithCode(NULL, 42, TESTSTR("msg"));
    REQUIRE(RERR_Error_GetDomain(err) != NULL);
    REQUIRE(strcmp(RERR_Error_GetDomain(err), RERR_DOMAIN_RICHERRORS) == 0);
    REQUIRE(RERR_Error_GetCode(err) == RERR_ECODE_NULL_ARGUMENT);
    RERR_Error_Destroy(err);

    RERR_Domain_UnregisterAll();
}


TEST_CASE("Wrap without code") {
    RERR_ErrorPtr cause = RERR_Error_Create(TESTSTR("msg"));
    RERR_ErrorPtr wrap = RERR_Error_Wrap(cause, TESTSTR("msg"));
    REQUIRE(wrap != RERR_NO_ERROR);
    REQUIRE(RERR_Error_HasCause(wrap));

    // The cause is not copied but ownership is transferred to wrap
    REQUIRE(RERR_Error_GetCause(wrap) == cause);

    RERR_Error_Destroy(wrap);
}


static void FormatCode(RERR_CodeFormat format, int32_t code, char* dest, size_t destSize) {
    REQUIRE(RERR_Domain_Register("test", format) == RERR_NO_ERROR);

    RERR_ErrorPtr err = RERR_Error_CreateWithCode("test", code, TESTSTR("msg"));
    REQUIRE(err != RERR_NO_ERROR);

    RERR_Error_FormatCode(err, dest, destSize);

    RERR_Error_Destroy(err);
    RERR_Domain_UnregisterAll();
}


TEST_CASE("Code formatting") {
    char buf[RERR_FORMATTED_CODE_MAX_SIZE];

    RERR_Error_FormatCode(RERR_NO_ERROR, buf, sizeof(buf));
    CHECK(strcmp(buf, "(no code)") == 0);

    RERR_Error_FormatCode(RERR_Error_CreateOutOfMemory(), buf, sizeof(buf));
    CHECK(strcmp(buf, "-1") == 0);

    FormatCode(RERR_CodeFormat_I32, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "-1") == 0);
    FormatCode(RERR_CodeFormat_I32, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0") == 0);

    FormatCode(RERR_CodeFormat_U32, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "4294967295") == 0);
    FormatCode(RERR_CodeFormat_U32, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0") == 0);

    FormatCode(RERR_CodeFormat_Hex32, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "0xffffffff") == 0);
    FormatCode(RERR_CodeFormat_Hex32, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0x00000000") == 0);

    FormatCode(RERR_CodeFormat_I32 | RERR_CodeFormat_Hex32, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "-1 (0xffffffff)") == 0);
    FormatCode(RERR_CodeFormat_I32 | RERR_CodeFormat_Hex32, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0 (0x00000000)") == 0);

    FormatCode(RERR_CodeFormat_U32 | RERR_CodeFormat_Hex32, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "4294967295 (0xffffffff)") == 0);
    FormatCode(RERR_CodeFormat_U32 | RERR_CodeFormat_Hex32, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0 (0x00000000)") == 0);

    FormatCode(RERR_CodeFormat_I16, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "-1") == 0);
    FormatCode(RERR_CodeFormat_I16, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0") == 0);

    FormatCode(RERR_CodeFormat_U16, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "65535") == 0);
    FormatCode(RERR_CodeFormat_U16, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0") == 0);

    FormatCode(RERR_CodeFormat_Hex16, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "0xffff") == 0);
    FormatCode(RERR_CodeFormat_Hex16, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0x0000") == 0);

    FormatCode(RERR_CodeFormat_I16 | RERR_CodeFormat_Hex16, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "-1 (0xffff)") == 0);
    FormatCode(RERR_CodeFormat_I16 | RERR_CodeFormat_Hex16, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0 (0x0000)") == 0);

    FormatCode(RERR_CodeFormat_U16 | RERR_CodeFormat_Hex16, -1, buf, sizeof(buf));
    CHECK(strcmp(buf, "65535 (0xffff)") == 0);
    FormatCode(RERR_CodeFormat_U16 | RERR_CodeFormat_Hex16, 0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0 (0x0000)") == 0);

    // No truncation (too short for primary)
    FormatCode(RERR_CodeFormat_I32, 1234, buf, strlen("1234") + 1 - 1);
    CHECK(strcmp(buf, "???") == 0);

    // Leave out secondary if it won't fit
    FormatCode(RERR_CodeFormat_I32 | RERR_CodeFormat_Hex32, -1, buf, strlen("-1 (0x") + 1);
    CHECK(strcmp(buf, "-1") == 0);
}
