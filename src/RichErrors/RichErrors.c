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

#define _CRT_SECURE_NO_WARNINGS
#include "RichErrors/RichErrors.h"

#include "DynArray.h"
#include "Threads.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_DOMAIN_LENGTH 63 // Not including null terminator


struct RERR_Error {
    const char* domain; // String owned by this RERR_Error object
    int32_t code; // Zero if no domain
    RERR_CodeFormat format;
    const char* message; // String owned by this RERR_Error object
    struct RERR_Error* cause; // Original error, owned by this RERR_Error
    RERR_InfoMapPtr info;
    uint32_t refCount; // For immutable copy support
};


// Special value we use so that we can return "out of memory" errors without
// allocating anything. All functions that inspect struct RERR_Error must check
// for this value first.
#define RERR_OUT_OF_MEMORY ((RERR_ErrorPtr) -1)


static RERR_ErrorPtr Create_RichErrorsError(int32_t code, const char* message)
{
    return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS, code,
        RERR_CodeFormat_I32, message);
}


static RERR_ErrorPtr CodeFormat_Check(RERR_CodeFormat format)
{
    switch (format & ~RERR_CodeFormat_HexNoPad) {
    case RERR_CodeFormat_I32:
    case RERR_CodeFormat_U32:
    case RERR_CodeFormat_Hex32:
    case RERR_CodeFormat_I32 | RERR_CodeFormat_Hex32:
    case RERR_CodeFormat_U32 | RERR_CodeFormat_Hex32:
    case RERR_CodeFormat_I16:
    case RERR_CodeFormat_U16:
    case RERR_CodeFormat_Hex16:
    case RERR_CodeFormat_I16 | RERR_CodeFormat_Hex16:
    case RERR_CodeFormat_U16 | RERR_CodeFormat_Hex16:
        return RERR_NO_ERROR;
    }

    return Create_RichErrorsError(RERR_ECODE_CODEFORMAT_INVALID,
        "Invalid error code format");
}


// Precondition: domain != NULL
static RERR_ErrorPtr Domain_Check(const char* domain)
{
    if (domain[0] == '\0') {
        return Create_RichErrorsError(RERR_ECODE_DOMAIN_NAME_EMPTY,
            "Empty error domain name");
    }

    size_t len = strlen(domain);
    if (len > MAX_DOMAIN_LENGTH) {
        char fmt[] = "Error domain name exceeding %d characters: ";
        char msg[sizeof(fmt) + MAX_DOMAIN_LENGTH + 32];
        snprintf(msg, sizeof(msg), fmt, MAX_DOMAIN_LENGTH);
        strncat(msg, domain, MAX_DOMAIN_LENGTH);
        strcat(msg, "...");

        return Create_RichErrorsError(RERR_ECODE_DOMAIN_NAME_TOO_LONG, msg);
    }

    for (int i = 0; i < len; ++i) {
        // Allow ASCII graphic or space only.
        if (domain[i] < ' ' || domain[i] > '~') {
            return Create_RichErrorsError(RERR_ECODE_DOMAIN_NAME_INVALID,
                "Error domain containing disallowed characters");
        }
    }

    return RERR_NO_ERROR;
}


RERR_ErrorPtr RERR_Error_Create(const char* message)
{
    RERR_ErrorPtr ret = calloc(1, sizeof(struct RERR_Error));
    if (!ret) {
        return RERR_OUT_OF_MEMORY;
    }

    if (message) {
        size_t len = strlen(message);
        // We allocate the message even if empty, so that we retain the
        // information that an empty message was used (to help debugging).
        char* msgCopy = malloc(len + 1);
        if (!msgCopy) {
            free(ret);
            return RERR_OUT_OF_MEMORY;
        }
        strcpy(msgCopy, message);
        ret->message = msgCopy;
    }

    ++ret->refCount;
    return ret;
}


RERR_ErrorPtr RERR_Error_CreateWithCode(const char* codeDomain, int32_t code,
    RERR_CodeFormat codeFormat, const char* message)
{

    // Allow NULL (but not empty string) for domain, as long as code is zero.
    if (!codeDomain) {
        if (code == 0) {
            return RERR_Error_Create(message);
        }
        return Create_RichErrorsError(RERR_ECODE_NULL_ARGUMENT,
            "Null code domain with nonzero code");
    }

    RERR_ErrorPtr err = Domain_Check(codeDomain);
    if (err) {
        return err;
    }

    err = CodeFormat_Check(codeFormat);
    if (err) {
        return err;
    }

    RERR_ErrorPtr ret = RERR_Error_Create(message);
    if (ret != RERR_OUT_OF_MEMORY) {
        size_t len = strlen(codeDomain);
        char* domainCopy = malloc(len + 1);
        if (!domainCopy) {
            RERR_Error_Destroy(ret);
            return RERR_OUT_OF_MEMORY;
        }
        strcpy(domainCopy, codeDomain);
        ret->domain = domainCopy;
        ret->code = code;
        ret->format = codeFormat;
    }
    return ret;
}


RERR_ErrorPtr RERR_Error_CreateWithInfo(const char* codeDomain, int32_t code,
    RERR_CodeFormat codeFormat, RERR_InfoMapPtr info, const char* message)
{
    RERR_ErrorPtr ret = RERR_Error_CreateWithCode(codeDomain, code, codeFormat,
        message);

    // We treat empty info map the same as no info map.
    if (RERR_InfoMap_IsEmpty(info)) { // Includes info == NULL
        goto exit;
    }

    // Domain is required for having info.
    if (!codeDomain) {
        goto exit;
    }

    // If there was an error while creating the error, the info no longer
    // pertains to the error to be returned. Since we have ownership anyway,
    // destroy it.
    if (ret == RERR_OUT_OF_MEMORY ||
        !ret->domain ||
        strcmp(ret->domain, codeDomain) != 0 ||
        ret->code != code)
    {
        goto exit;
    }

    ret->info = RERR_InfoMap_ImmutableCopy(info);

exit:
    RERR_InfoMap_Destroy(info);
    return ret;
}


void RERR_Error_Destroy(RERR_ErrorPtr error)
{
    if (!error || error == RERR_OUT_OF_MEMORY)
        return;

    if (--error->refCount == 0) {
        free((char*)error->message);
        free((char*)error->domain);
        RERR_Error_Destroy(error->cause);
        free(error);
    }
}


void RERR_Error_Copy(RERR_ErrorPtr source, RERR_ErrorPtr* destination)
{
    *destination = source;

    if (!source || source == RERR_OUT_OF_MEMORY) {
        return;
    }

    ++source->refCount;
    return;
}


RERR_ErrorPtr RERR_Error_CreateOutOfMemory(void)
{
    return RERR_OUT_OF_MEMORY;
}


RERR_ErrorPtr RERR_Error_Wrap(RERR_ErrorPtr cause, const char* message)
{
    RERR_ErrorPtr ret = RERR_Error_Create(message);
    if (ret == RERR_OUT_OF_MEMORY) {
        RERR_Error_Destroy(cause);
        return ret;
    }
    ret->cause = cause;
    return ret;
}


RERR_ErrorPtr RERR_Error_WrapWithCode(RERR_ErrorPtr cause,
    const char* codeDomain, int32_t code, RERR_CodeFormat codeFormat,
    const char* message)
{
    RERR_ErrorPtr ret = RERR_Error_CreateWithCode(codeDomain, code, codeFormat,
        message);
    if (ret == RERR_OUT_OF_MEMORY) {
        RERR_Error_Destroy(cause);
        return ret;
    }
    ret->cause = cause;
    return ret;
}


RERR_ErrorPtr RERR_Error_WrapWithInfo(RERR_ErrorPtr cause,
    const char* codeDomain, int32_t code, RERR_CodeFormat codeFormat,
    RERR_InfoMapPtr info, const char* message)
{
    RERR_ErrorPtr ret = RERR_Error_CreateWithInfo(codeDomain, code, codeFormat,
        info, message);
    if (ret == RERR_OUT_OF_MEMORY) {
        RERR_Error_Destroy(cause);
        return ret;
    }
    ret->cause = cause;
    return ret;
}


bool RERR_Error_HasCode(RERR_ErrorPtr error)
{
    if (!error) {
        return false;
    }
    if (error == RERR_OUT_OF_MEMORY) {
        return true;
    }
    return error->domain != NULL;
}


const char* RERR_Error_GetDomain(RERR_ErrorPtr error)
{
    if (!error) {
        return "";
    }
    if (error == RERR_OUT_OF_MEMORY) {
        return RERR_DOMAIN_CRITICAL;
    }
    if (!error->domain) {
        return "";
    }
    return error->domain;
}


int32_t RERR_Error_GetCode(RERR_ErrorPtr error)
{
    if (!error) {
        return 0;
    }
    if (error == RERR_OUT_OF_MEMORY) {
        return RERR_ECODE_OUT_OF_MEMORY;
    }
    return error->code;
}


void RERR_Error_FormatCode(RERR_ErrorPtr error, char* dest, size_t destSize)
{
    if (!dest || destSize == 0) {
        return;
    }

    if (error == RERR_NO_ERROR ||
        (error != RERR_OUT_OF_MEMORY && !error->domain)) {
        strncpy(dest, "(no code)", destSize);
        dest[destSize - 1] = '\0';
        return;
    }

    int32_t code;
    RERR_CodeFormat format;
    if (error == RERR_OUT_OF_MEMORY) {
        code = RERR_ECODE_OUT_OF_MEMORY;
        format = RERR_CodeFormat_I32;
    }
    else {
        code = error->code;
        format = error->format;
    }

    // The format has been checked upon error creation, so we assume it has a
    // valid combination of flags. See CodeFormat_Check().

    char dec[16]; // Max: strlen("-2147483648") == 11
    char hex[16]; // Max: strlen("0xffffffff") == 10
    dec[0] = '\0';
    hex[0] = '\0';

    if (format & RERR_CodeFormat_I32) {
        snprintf(dec, sizeof(dec), "%" PRId32, code);
    }
    if (format & RERR_CodeFormat_U32) {
        snprintf(dec, sizeof(dec), "%" PRIu32, code);
    }
    if (format & RERR_CodeFormat_Hex32) {
        const char* const fmt = format & RERR_CodeFormat_HexNoPad ?
            "0x%" PRIx32 : "0x%08" PRIx32;
        snprintf(hex, sizeof(hex), fmt, code);
    }
    if (format & RERR_CodeFormat_I16) {
        snprintf(dec, sizeof(dec), "%" PRId16, (int16_t)code);
    }
    if (format & RERR_CodeFormat_U16) {
        snprintf(dec, sizeof(dec), "%" PRIu16, (uint16_t)code);
    }
    if (format & RERR_CodeFormat_Hex16) {
        const char* const fmt = format & RERR_CodeFormat_HexNoPad ?
            "0x%" PRIx16 : "0x%04" PRIx16;
        snprintf(hex, sizeof(hex), fmt, (uint16_t)code);
    }

    size_t decLen = strlen(dec);
    size_t hexLen = strlen(hex);
    const char* primary;
    const char* secondary = NULL;
    size_t primaryLen;
    size_t secondaryLen = 0;
    if (decLen > 0) {
        primary = dec;
        primaryLen = decLen;
        if (hexLen > 0) {
            secondary = hex;
            secondaryLen = hexLen;
        }
    }
    else {
        primary = hex;
        primaryLen = hexLen;
    }

    // Ensure we never end up with a truncated code; prefer "???" over that.
    if (destSize < primaryLen + 1) { // Cannot fit code
        strncpy(dest, "???", destSize);
        dest[destSize - 1] = '\0';
        return;
    }

    strcpy(dest, primary);

    if (!secondary) {
        return;
    }

    const char lparen[] = " (";
    const char rparen[] = ")";
    if (destSize < primaryLen + strlen(lparen) + secondaryLen + strlen(rparen) + 1) {
        // Cannot fit secondary, leave it out entirely
        return;
    }

    strcat(dest, lparen);
    strcat(dest, secondary);
    strcat(dest, rparen);
}


bool RERR_Error_HasInfo(RERR_ErrorPtr error)
{
    if (!error || error == RERR_OUT_OF_MEMORY) {
        return false;
    }
    return error->info && !RERR_InfoMap_IsEmpty(error->info);
}


RERR_InfoMapPtr RERR_Error_GetInfo(RERR_ErrorPtr error)
{
    if (!error || error == RERR_OUT_OF_MEMORY || !error->info) {
        RERR_InfoMapPtr ret = RERR_InfoMap_Create();
        RERR_InfoMap_MakeImmutable(ret);
        return ret;
    }
    return RERR_InfoMap_ImmutableCopy(error->info);
}


const char* RERR_Error_GetMessage(RERR_ErrorPtr error)
{
    if (!error) {
        return "(no error)";
    }
    if (error == RERR_OUT_OF_MEMORY) {
        return "Out of memory";
    }
    if (!error->message) {
        return "(error message unavailable)";
    }
    if (error->message[0] == '\0') {
        return "(empty error message)";
    }
    return error->message;
}


bool RERR_Error_HasCause(RERR_ErrorPtr error)
{
    if (!error || error == RERR_OUT_OF_MEMORY) {
        return false;
    }
    return error->cause != NULL;
}


RERR_ErrorPtr RERR_Error_GetCause(RERR_ErrorPtr error)
{
    if (!error || error == RERR_OUT_OF_MEMORY) {
        return NULL;
    }
    return error->cause;
}


bool RERR_Error_IsOutOfMemory(RERR_ErrorPtr error)
{
    return error == RERR_OUT_OF_MEMORY;
}
