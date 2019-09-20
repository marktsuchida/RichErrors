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

#define _CRT_SECURE_NO_WARNINGS
#include "RichErrors/RichErrors.h"

#include "DynArray.h"
#include "Threads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct RERR_ErrorDomain {
    const char* name; // Unique key
    RERR_CodeFormat codeFormat;
};
typedef struct RERR_ErrorDomain* RERR_ErrorDomainPtr;

static struct RERR_ErrorDomain RichErrorsCriticalDomain = {
    RERR_DOMAIN_CRITICAL, RERR_CodeFormat_I32,
};

static struct RERR_ErrorDomain RichErrorsDomain = {
    RERR_DOMAIN_RICHERRORS, RERR_CodeFormat_I32,
};


// Globally registered domains. We use an array of pointers, so that domain
// pointers always remain valid.
static DynArrayPtr domains; // Elements are RERR_ErrorDomainPtr, sorted by key
static RecursiveMutex DECLARE_STATIC_MUTEX(domainsLock);
static MutexInitializer DECLARE_MUTEX_INITIALIZER(domainsLockInit);


#define MAX_DOMAIN_LENGTH 63 // Not including null terminator


struct RERR_Error {
    const struct RERR_ErrorDomain* domain; // Non-owning ref
    int32_t code; // Zero if no domain
    const char* message; // String owned by this RERR_Error object
    struct RERR_Error* cause; // Original error, owned by this RERR_Error
    uint32_t refCount;
};


// Special value we use so that we can return "out of memory" errors without
// allocating anything. All functions that inspect struct RERR_Error must check
// for this value first.
#define RERR_OUT_OF_MEMORY ((RERR_ErrorPtr) -1)


static inline void FreeConst(const void* m)
{
    free((void*)m);
}


static RERR_ErrorPtr CodeFormat_Check(RERR_CodeFormat format)
{
    switch (format) {
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

    return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
        RERR_ECODE_CODEFORMAT_INVALID,
        "Invalid error code format");
}


// Precondition: domainName != NULL
static RERR_ErrorPtr Domain_Check(const char* domainName)
{
    if (domainName[0] == '\0') {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_DOMAIN_NAME_EMPTY, "Empty error domain name");
    }

    size_t len = strlen(domainName);
    if (len > MAX_DOMAIN_LENGTH) {
        char fmt[] = "Error domain name exceeding %d characters: ";
        char msg[sizeof(fmt) + MAX_DOMAIN_LENGTH + 32];
        snprintf(msg, sizeof(msg), fmt, MAX_DOMAIN_LENGTH);
        strncat(msg, domainName, MAX_DOMAIN_LENGTH);
        strcat(msg, "...");

        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_DOMAIN_NAME_TOO_LONG, msg);
    }

    for (int i = 0; i < len; ++i) {
        // Allow ASCII graphic or space only. Disallow (reserve) forward and
        // backward slashes and colon for possible future extensions.
        if (domainName[i] < ' ' || domainName[i] > '~' || domainName[i] == '/' ||
            domainName[i] == '\\' || domainName[i] == ':') {
            return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
                RERR_ECODE_DOMAIN_NAME_INVALID,
                "Error domain containing unallowed characters");
        }
    }

    return RERR_NO_ERROR;
}


static int Domain_Compare(const void* lhs, const char* rhs)
{
    RERR_ErrorDomainPtr elem = *(const RERR_ErrorDomainPtr*)lhs;
    return strcmp(elem->name, rhs);
}


// Argument must be pre-checked by caller
static RERR_ErrorDomainPtr Domain_Find(const char* domainName)
{
    // The system domains always exist, but is not sotred in the array.
    if (strcmp(domainName, RERR_DOMAIN_CRITICAL) == 0) {
        return &RichErrorsCriticalDomain;
    }
    if (strcmp(domainName, RERR_DOMAIN_RICHERRORS) == 0) {
        return &RichErrorsDomain;
    }

    EnsureInitMutex(&domainsLock, &domainsLockInit);

    LockMutex(&domainsLock);

    RERR_ErrorDomainPtr* found = NULL;
    if (domains) {
        found = DynArray_BSearchExact(domains, domainName, Domain_Compare);
    }
    RERR_ErrorDomainPtr ret = found ? *found : NULL;

    UnlockMutex(&domainsLock);

    return ret;
}


static RERR_ErrorDomainPtr Domain_Create(const char* name,
    RERR_CodeFormat codeFormat)
{
    char* nameCopy = NULL;
    RERR_ErrorDomainPtr ret = NULL;

    nameCopy = malloc(strlen(name) + 1);
    if (!nameCopy) {
        goto error;
    }
    strcpy(nameCopy, name);

    ret = calloc(1, sizeof(struct RERR_ErrorDomain));
    if (!ret) {
        goto error;
    }
    ret->name = nameCopy;
    ret->codeFormat = codeFormat;
    return ret;

error:
    free(ret);
    free(nameCopy);
    return NULL;
}


static void Domain_Destroy(RERR_ErrorDomainPtr domain)
{
    if (!domain) {
        return;
    }

    FreeConst(domain->name);
    free(domain);
}


// Mutex must be held by caller; domain != NULL
static RERR_ErrorPtr Domain_Insert(RERR_ErrorDomainPtr domain)
{
    if (!domains) {
        domains = DynArray_Create(sizeof(RERR_ErrorDomainPtr));
        if (!domains) {
            return RERR_OUT_OF_MEMORY;
        }
    }

    RERR_ErrorDomainPtr* it = DynArray_BSearchInsertionPoint(domains,
        domain->name, Domain_Compare);
    it = DynArray_Insert(domains, it);
    if (!it) {
        return RERR_OUT_OF_MEMORY;
    }
    *it = domain;
    return RERR_NO_ERROR;
}


void RERR_Domain_UnregisterAll(void)
{
    EnsureInitMutex(&domainsLock, &domainsLockInit);
    LockMutex(&domainsLock);

    if (!domains) {
        return;
    }

    RERR_ErrorDomainPtr* begin = DynArray_Begin(domains);
    RERR_ErrorDomainPtr* end = DynArray_End(domains);
    for (RERR_ErrorDomainPtr* it = begin; it != end; it = DynArray_Advance(domains, it)) {
        Domain_Destroy(*it);
    }

    // Actually we only need to clear, but we deallocate so that memory leak
    // detection in unit tests will not see the leftover static array.
    DynArray_Destroy(domains);
    domains = NULL;

    UnlockMutex(&domainsLock);
}


RERR_ErrorPtr RERR_Domain_Register(const char* domainName,
    RERR_CodeFormat codeFormat)
{
    if (!domainName) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT, "Null error domain");
    }
    RERR_ErrorPtr err = Domain_Check(domainName);
    if (err) {
        return err;
    }

    err = CodeFormat_Check(codeFormat);
    if (err) {
        return err;
    }

    RERR_ErrorDomainPtr domain = Domain_Create(domainName, codeFormat);
    if (!domain) {
        return RERR_OUT_OF_MEMORY;
    }

    RERR_ErrorPtr ret = RERR_NO_ERROR;
    EnsureInitMutex(&domainsLock, &domainsLockInit);
    LockMutex(&domainsLock);

    const struct RERR_ErrorDomain* found = Domain_Find(domainName);
    if (found) {
        char msg0[] = "Cannot register already registered domain: ";
        char msg[sizeof(msg0) + MAX_DOMAIN_LENGTH + 32];
        strcpy(msg, msg0);
        strcat(msg, domainName);
        ret = RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_DOMAIN_ALREADY_EXISTS, msg);
        goto cleanup;
    }

    ret = Domain_Insert(domain);
    if (ret) {
        goto cleanup;
    }
    domain = NULL; // Ownership has been transferred

cleanup:
    UnlockMutex(&domainsLock);
    Domain_Destroy(domain);
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
        strcpy(msgCopy, message);
        ret->message = msgCopy;
    }

    ++ret->refCount;
    return ret;
}


RERR_ErrorPtr RERR_Error_CreateWithCode(const char* domain, int32_t code,
    const char* message)
{
    // Allow NULL (but not empty string) for domain, as long as code is zero.
    if (!domain) {
        if (code == 0) {
            return RERR_Error_Create(message);
        }
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
            RERR_ECODE_NULL_ARGUMENT, "Null error domain");
    }

    RERR_ErrorPtr err = Domain_Check(domain);
    if (err) {
        return err;
    }

    const struct RERR_ErrorDomain* d = Domain_Find(domain);
    if (!d) {
        char msg0[] = "Error domain not registered: ";
        char msg[sizeof(msg0) + MAX_DOMAIN_LENGTH + 32];
        strcpy(msg, msg0);
        strcat(msg, domain);
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

    if (--error->refCount == 0) {
        free((char*)error->message);
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
        return RERR_DOMAIN_CRITICAL;
    }
    if (!error->domain) {
        return "";
    }
    return error->domain->name;
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


bool RERR_Error_IsOutOfMemory(RERR_ErrorPtr error)
{
    return error == RERR_OUT_OF_MEMORY;
}
