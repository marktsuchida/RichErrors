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

#include "RichErrors/RichErrors.h"

#include "Threads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Error domains are stored in a global sorted dynamic array, allowing binary
// search.
static const char** domains; // Sorted in naive (ascii) lexicographical order
static size_t domainsSize; // Number of registered domains
static size_t domainsCapacity; // Allocated length of 'domains'
static RecursiveMutex DECLARE_STATIC_MUTEX(domainsLock);
static MutexInitializer DECLARE_MUTEX_INITIALIZER(domainsLockInit);


#define MAX_DOMAIN_LENGTH 63 // Not including null terminator


struct RERR_Error {
    const char* domain; // Always points to the registered copy
    int32_t code; // Zero if no domain
    const char* message; // String owned by this RERR_Error object
    struct RERR_Error* cause; // Original error, owned by this RERR_Error
};


// Special value we use so that we can return "out of memory" errors without
// allocating anything. All functions that inspect struct RERR_Error must check
// for this value first.
#define RERR_OUT_OF_MEMORY ((RERR_ErrorPtr) -1)


static RERR_ErrorPtr Domain_Check(const char* domain)
{
    if (!domain) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_DOMAIN_NULL, "Null error domain");
    }
    if (domain[0] == '\0') {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_DOMAIN_NARERR_EMPTY, "Empty error domain name");
    }

    size_t len = strnlen_s(domain, MAX_DOMAIN_LENGTH + 1);
    if (len > MAX_DOMAIN_LENGTH) {
        char fmt[] = "Error domain name exceeding %d characters: ";
        char msg[sizeof(fmt) + MAX_DOMAIN_LENGTH + 32];
        snprintf(msg, sizeof(msg), fmt, MAX_DOMAIN_LENGTH);
        strncat_s(msg, sizeof(msg), domain, MAX_DOMAIN_LENGTH);
        strcat_s(msg, sizeof(msg), "...");

        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_DOMAIN_NARERR_TOO_LONG, msg);
    }

    for (int i = 0; i < len; ++i) {
        // Allow ASCII graphic or space only. Disallow (reserve) forward and
        // backward slashes and colon for possible future extensions.
        if (domain[i] < ' ' || domain[i] > '~' || domain[i] == '/' ||
            domain[i] == '\\' || domain[i] == ':') {
            return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
                RERR_ECODE_DOMAIN_NARERR_INVALID,
                "Error domain containing unallowed characters");
        }
    }

    return RERR_NO_ERROR;
}


static int Domain_Compare(const char* const* lhs, const char* const* rhs)
{
    return strcmp(*lhs, *rhs);
}


// Argument must be pre-checked by caller
static const char* Domain_Find(const char* domain)
{
    // The system domain always exists, but is not sotred in the array.
    if (strcmp(domain, RERR_DOMAIN_RICHERRORS) == 0) {
        return RERR_DOMAIN_RICHERRORS;
    }

    EnsureInitMutex(&domainsLock, &domainsLockInit);

    LockMutex(&domainsLock);

    const char** found = bsearch(&domain, domains, domainsSize,
        sizeof(const char*), Domain_Compare);

    UnlockMutex(&domainsLock);

    return found ? *found : NULL;
}


// Mutex must be held by caller
RERR_ErrorPtr Domain_EnsureCapacity()
{
    if (domainsCapacity == 0) {
        domainsCapacity = 32;
        domains = malloc(domainsCapacity * sizeof(const char*));
        if (!domains) {
            domainsCapacity = 0;
            return RERR_OUT_OF_MEMORY;
        }
    }
    else if (domainsCapacity == domainsSize) {
        domainsCapacity += 32;
        const char** newDomains = realloc((char*)domains, domainsCapacity);
        if (!newDomains) {
            domainsCapacity -= 32;
            return RERR_OUT_OF_MEMORY;
        }
        domains = newDomains;
    }
    return RERR_NO_ERROR;
}


// Mutex must be held by caller, and domain must be pre-checked
RERR_ErrorPtr Domain_Insert(const char* domain)
{
    RERR_ErrorPtr err = Domain_EnsureCapacity();
    if (err) {
        return err;
    }

    size_t len = strlen(domain);
    char* dCopy = malloc(len + 1);
    if (!dCopy) {
        return RERR_OUT_OF_MEMORY;
    }
    strcpy_s(dCopy, len + 1, domain);

    bool shifting = false;
    const char* wk = NULL;
    for (int i = 0; i < domainsSize; ++i) {
        if (shifting) {
            const char* wk2 = domains[i];
            domains[i] = wk;
            wk = wk2;
        }
        if (strcmp(dCopy, domains[i]) < 0) {
            wk = domains[i];
            domains[i] = dCopy;
            shifting = true;
        }
    }
    ++domainsSize;
    if (wk) {
        domains[domainsSize - 1] = wk;
    }
    else {
        domains[domainsSize - 1] = dCopy;
    }

    return RERR_NO_ERROR;
}


void RERR_Domain_UnregisterAll(void)
{
    EnsureInitMutex(&domainsLock, &domainsLockInit);
    LockMutex(&domainsLock);
    for (size_t i = 0; i < domainsSize; ++i) {
        free((char*)domains[i]);
    }
    free((char**)domains);
    domains = NULL;
    domainsCapacity = 0;
    domainsSize = 0;
    UnlockMutex(&domainsLock);
}


RERR_ErrorPtr RERR_Domain_Register(const char* domain)
{
    RERR_ErrorPtr err = Domain_Check(domain);
    if (err) {
        return err;
    }

    EnsureInitMutex(&domainsLock, &domainsLockInit);

    RERR_ErrorPtr ret = RERR_NO_ERROR;
    LockMutex(&domainsLock);

    const char* found = Domain_Find(domain);
    if (found) {
        char msg0[] = "Cannot register already registered domain: ";
        char msg[sizeof(msg0) + MAX_DOMAIN_LENGTH + 32];
        strcpy_s(msg, sizeof(msg), msg0);
        strcat_s(msg, sizeof(msg), domain);
        ret = RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_DOMAIN_ALREADY_EXISTS, msg);
        goto cleanup;
    }

    ret = Domain_Insert(domain);
    if (ret) {
        goto cleanup;
    }

cleanup:
    UnlockMutex(&domainsLock);
    return ret;
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
        strcpy_s(msgCopy, len + 1, message);
        ret->message = msgCopy;
    }

    return ret;
}


RERR_ErrorPtr RERR_Error_CreateWithCode(const char* domain, int32_t code,
    const char* message)
{
    // Allow NULL (but not empty string) for domain, as long as code is zero.
    if (!domain && !code) {
        return RERR_Error_Create(message);
    }

    RERR_ErrorPtr err = Domain_Check(domain);
    if (err) {
        return err;
    }

    const char* d = Domain_Find(domain);
    if (!d) {
        char msg0[] = "Error domain not registered: ";
        char msg[sizeof(msg0) + MAX_DOMAIN_LENGTH + 32];
        strcpy_s(msg, sizeof(msg), msg0);
        strcat_s(msg, sizeof(msg), domain);
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_DOMAIN_NOT_REGISTERED, msg);
    }

    RERR_ErrorPtr ret = RERR_Error_Create(message);
    if (ret != RERR_OUT_OF_MEMORY) {
        ret->domain = d;
        ret->code = code;
    }
    return ret;
}


void RERR_Error_Destroy(RERR_ErrorPtr error)
{
    if (!error || error == RERR_OUT_OF_MEMORY)
        return;
    free((char*)error->message);
    RERR_Error_Destroy(error->cause);
}


RERR_ErrorPtr RERR_Error_OutOfMemory(void)
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


RERR_ErrorPtr RERR_Error_WrapWithCode(RERR_ErrorPtr cause, const char* domain,
    int32_t code, const char* message)
{
    RERR_ErrorPtr ret = RERR_Error_CreateWithCode(domain, code, message);
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
        return RERR_DOMAIN_RICHERRORS;
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
