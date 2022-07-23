// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#define _CRT_SECURE_NO_WARNINGS
#include "RichErrors/RichErrors.h"

#include "DynArray.h"
#include "Threads.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct RERR_Domain {
    const char *name; // Unique key
    RERR_CodeFormat codeFormat;
};
typedef struct RERR_Domain *RERR_DomainPtr;

static struct RERR_Domain RichErrorsCriticalDomain = {
    RERR_DOMAIN_CRITICAL,
    RERR_CodeFormat_I32,
};

static struct RERR_Domain RichErrorsDomain = {
    RERR_DOMAIN_RICHERRORS,
    RERR_CodeFormat_I32,
};

// Globally registered domains. We use an array of pointers, so that domain
// pointers always remain valid.
static RERR_DynArrayPtr domains; // Elements are RERR_DomainPtr, sorted by key
static Mutex domainsLock;
static CallOnceFlag domainsLockInit = CALL_ONCE_FLAG_INITIALIZER;
static void InitDomainsLock(void) { InitRecursiveMutex(&domainsLock); }

#define MAX_DOMAIN_LENGTH 63 // Not including null terminator

struct RERR_Error {
    const struct RERR_Domain *domain; // Non-owning ref
    int32_t code;                     // Zero if no domain
    const char *message;              // String owned by this RERR_Error object
    struct RERR_Error *cause; // Original error, owned by this RERR_Error
    RERR_InfoMapPtr info;
    uint32_t refCount;
};

// Special value we use so that we can return "out of memory" errors without
// allocating anything. All functions that inspect struct RERR_Error must check
// for this value first.
#define RERR_OUT_OF_MEMORY ((RERR_ErrorPtr)-1)

static inline void FreeConst(const void *m) { free((void *)m); }

static RERR_ErrorPtr CodeFormat_Check(RERR_CodeFormat format) {
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

    return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
                                     RERR_ECODE_CODEFORMAT_INVALID,
                                     "Invalid error code format");
}

// Precondition: domainName != NULL
static RERR_ErrorPtr Domain_Check(const char *domainName) {
    if (domainName[0] == '\0') {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
                                         RERR_ECODE_DOMAIN_NAME_EMPTY,
                                         "Empty error domain name");
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

    for (size_t i = 0; i < len; ++i) {
        // Allow ASCII graphic or space only.
        if (domainName[i] < ' ' || domainName[i] > '~') {
            return RERR_Error_CreateWithCode(
                RERR_DOMAIN_RICHERRORS, RERR_ECODE_DOMAIN_NAME_INVALID,
                "Error domain containing disallowed characters");
        }
    }

    return RERR_NO_ERROR;
}

static int Domain_Compare(const void *lhs, const void *rhs) {
    const char *rhs_charp = rhs;
    RERR_DomainPtr elem = *(const RERR_DomainPtr *)lhs;
    return strcmp(elem->name, rhs_charp);
}

// Argument must be pre-checked by caller
static RERR_DomainPtr Domain_Find(const char *domainName) {
    // The system domains always exist, but is not sotred in the array.
    if (strcmp(domainName, RERR_DOMAIN_CRITICAL) == 0) {
        return &RichErrorsCriticalDomain;
    }
    if (strcmp(domainName, RERR_DOMAIN_RICHERRORS) == 0) {
        return &RichErrorsDomain;
    }

    CallOnce(&domainsLockInit, InitDomainsLock);
    LockMutex(&domainsLock);

    RERR_DomainPtr *found = NULL;
    if (domains) {
        found = RERR_DynArray_BSearch(domains, domainName, Domain_Compare);
    }
    RERR_DomainPtr ret = found ? *found : NULL;

    UnlockMutex(&domainsLock);

    return ret;
}

static RERR_DomainPtr Domain_Create(const char *name,
                                    RERR_CodeFormat codeFormat) {
    char *nameCopy = NULL;
    RERR_DomainPtr ret = NULL;

    nameCopy = malloc(strlen(name) + 1);
    if (!nameCopy) {
        goto error;
    }
    strcpy(nameCopy, name);

    ret = calloc(1, sizeof(struct RERR_Domain));
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

static void Domain_Destroy(RERR_DomainPtr domain) {
    if (!domain) {
        return;
    }

    FreeConst(domain->name);
    free(domain);
}

// Mutex must be held by caller; domain != NULL
static RERR_ErrorPtr Domain_Insert(RERR_DomainPtr domain) {
    if (!domains) {
        domains = RERR_DynArray_Create(sizeof(RERR_DomainPtr));
        if (!domains) {
            return RERR_OUT_OF_MEMORY;
        }
    }

    RERR_DomainPtr *it = RERR_DynArray_BSearchInsertionPoint(
        domains, domain->name, Domain_Compare);
    it = RERR_DynArray_Insert(domains, it);
    if (!it) {
        return RERR_OUT_OF_MEMORY;
    }
    *it = domain;
    return RERR_NO_ERROR;
}

void RERR_Domain_UnregisterAll(void) {
    CallOnce(&domainsLockInit, InitDomainsLock);
    LockMutex(&domainsLock);

    if (!domains) {
        return;
    }

    RERR_DomainPtr *begin = RERR_DynArray_Begin(domains);
    RERR_DomainPtr *end = RERR_DynArray_End(domains);
    for (RERR_DomainPtr *it = begin; it != end;
         it = RERR_DynArray_Advance(domains, it)) {
        Domain_Destroy(*it);
    }

    // Actually we only need to clear, but we deallocate so that memory leak
    // detection in unit tests will not see the leftover static array.
    RERR_DynArray_Destroy(domains);
    domains = NULL;

    UnlockMutex(&domainsLock);
}

RERR_ErrorPtr RERR_Domain_Register(const char *domainName,
                                   RERR_CodeFormat codeFormat) {
    if (!domainName) {
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
                                         RERR_ECODE_NULL_ARGUMENT,
                                         "Null error domain");
    }
    RERR_ErrorPtr err = Domain_Check(domainName);
    if (err) {
        return err;
    }

    err = CodeFormat_Check(codeFormat);
    if (err) {
        return err;
    }

    RERR_DomainPtr domain = Domain_Create(domainName, codeFormat);
    if (!domain) {
        return RERR_OUT_OF_MEMORY;
    }

    RERR_ErrorPtr ret = RERR_NO_ERROR;
    CallOnce(&domainsLockInit, InitDomainsLock);
    LockMutex(&domainsLock);

    const struct RERR_Domain *found = Domain_Find(domainName);
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

RERR_ErrorPtr RERR_Error_Create(const char *message) {
    RERR_ErrorPtr ret = calloc(1, sizeof(struct RERR_Error));
    if (!ret) {
        return RERR_OUT_OF_MEMORY;
    }

    if (message) {
        size_t len = strlen(message);
        // We allocate the message even if empty, so that we retain the
        // information that an empty message was used (to help debugging).
        char *msgCopy = malloc(len + 1);
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

RERR_ErrorPtr RERR_Error_CreateWithCode(const char *domainName, int32_t code,
                                        const char *message) {
    // Allow NULL (but not empty string) for domain, as long as code is zero.
    if (!domainName) {
        if (code == 0) {
            return RERR_Error_Create(message);
        }
        return RERR_Error_CreateWithCode(RERR_DOMAIN_RICHERRORS,
                                         RERR_ECODE_NULL_ARGUMENT,
                                         "Null error domain");
    }

    RERR_ErrorPtr err = Domain_Check(domainName);
    if (err) {
        return err;
    }

    const struct RERR_Domain *d = Domain_Find(domainName);
    if (!d) {
        char msg0[] = "Error domain not registered: ";
        char msg[sizeof(msg0) + MAX_DOMAIN_LENGTH + 32];
        strcpy(msg, msg0);
        strcat(msg, domainName);
        return RERR_Error_CreateWithCode(
            RERR_DOMAIN_RICHERRORS, RERR_ECODE_DOMAIN_NOT_REGISTERED, msg);
    }

    RERR_ErrorPtr ret = RERR_Error_Create(message);
    if (ret != RERR_OUT_OF_MEMORY) {
        ret->domain = d;
        ret->code = code;
    }
    return ret;
}

RERR_ErrorPtr RERR_Error_CreateWithInfo(const char *domainName, int32_t code,
                                        RERR_InfoMapPtr info,
                                        const char *message) {
    RERR_ErrorPtr ret = RERR_Error_CreateWithCode(domainName, code, message);

    // We treat empty info map the same as no info map.
    if (RERR_InfoMap_IsEmpty(info)) { // Includes info == NULL
        goto exit;
    }

    // Domain is required for having info.
    if (!domainName) {
        goto exit;
    }

    // If there was an error while creating the error, the info no longer
    // pertains to the error to be returned. Since we have ownership anyway,
    // destroy it.
    if (ret == RERR_OUT_OF_MEMORY || !ret->domain ||
        strcmp(ret->domain->name, domainName) != 0 || ret->code != code) {
        goto exit;
    }

    ret->info = RERR_InfoMap_ImmutableCopy(info);

exit:
    RERR_InfoMap_Destroy(info);
    return ret;
}

void RERR_Error_Destroy(RERR_ErrorPtr error) {
    if (!error || error == RERR_OUT_OF_MEMORY)
        return;

    if (--error->refCount == 0) {
        free((char *)error->message);
        RERR_Error_Destroy(error->cause);
        free(error);
    }
}

void RERR_Error_Copy(RERR_ErrorPtr source, RERR_ErrorPtr *destination) {
    *destination = source;

    if (!source || source == RERR_OUT_OF_MEMORY) {
        return;
    }

    ++source->refCount;
    return;
}

RERR_ErrorPtr RERR_Error_CreateOutOfMemory(void) { return RERR_OUT_OF_MEMORY; }

RERR_ErrorPtr RERR_Error_Wrap(RERR_ErrorPtr cause, const char *message) {
    RERR_ErrorPtr ret = RERR_Error_Create(message);
    if (ret == RERR_OUT_OF_MEMORY) {
        RERR_Error_Destroy(cause);
        return ret;
    }
    ret->cause = cause;
    return ret;
}

RERR_ErrorPtr RERR_Error_WrapWithCode(RERR_ErrorPtr cause,
                                      const char *domainName, int32_t code,
                                      const char *message) {
    RERR_ErrorPtr ret = RERR_Error_CreateWithCode(domainName, code, message);
    if (ret == RERR_OUT_OF_MEMORY) {
        RERR_Error_Destroy(cause);
        return ret;
    }
    ret->cause = cause;
    return ret;
}

RERR_ErrorPtr RERR_Error_WrapWithInfo(RERR_ErrorPtr cause,
                                      const char *domainName, int32_t code,
                                      RERR_InfoMapPtr info,
                                      const char *message) {
    RERR_ErrorPtr ret =
        RERR_Error_CreateWithInfo(domainName, code, info, message);
    if (ret == RERR_OUT_OF_MEMORY) {
        RERR_Error_Destroy(cause);
        return ret;
    }
    ret->cause = cause;
    return ret;
}

bool RERR_Error_HasCode(RERR_ErrorPtr error) {
    if (!error) {
        return false;
    }
    if (error == RERR_OUT_OF_MEMORY) {
        return true;
    }
    return error->domain != NULL;
}

const char *RERR_Error_GetDomain(RERR_ErrorPtr error) {
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

int32_t RERR_Error_GetCode(RERR_ErrorPtr error) {
    if (!error) {
        return 0;
    }
    if (error == RERR_OUT_OF_MEMORY) {
        return RERR_ECODE_OUT_OF_MEMORY;
    }
    return error->code;
}

void RERR_Error_FormatCode(RERR_ErrorPtr error, char *dest, size_t destSize) {
    if (!dest || destSize == 0) {
        return;
    }

    if (error == RERR_NO_ERROR) {
        strncpy(dest, "(no code)", destSize);
        dest[destSize - 1] = '\0';
        return;
    }

    const struct RERR_Domain *domain;
    int32_t code;
    if (error == RERR_OUT_OF_MEMORY) {
        domain = &RichErrorsCriticalDomain;
        code = RERR_ECODE_OUT_OF_MEMORY;
    } else {
        domain = error->domain;
        code = error->code;
    }

    if (domain == NULL) {
        strncpy(dest, "(no code)", destSize);
        dest[destSize - 1] = '\0';
        return;
    }

    RERR_CodeFormat format = domain->codeFormat;

    // The format has been checked upon domain registration, so we assume it
    // has a valid combination of flags. See CodeFormat_Check().

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
        const char *const fmt =
            format & RERR_CodeFormat_HexNoPad ? "0x%" PRIx32 : "0x%08" PRIx32;
        snprintf(hex, sizeof(hex), fmt, code);
    }
    if (format & RERR_CodeFormat_I16) {
        snprintf(dec, sizeof(dec), "%" PRId16, (int16_t)code);
    }
    if (format & RERR_CodeFormat_U16) {
        snprintf(dec, sizeof(dec), "%" PRIu16, (uint16_t)code);
    }
    if (format & RERR_CodeFormat_Hex16) {
        const char *const fmt =
            format & RERR_CodeFormat_HexNoPad ? "0x%" PRIx16 : "0x%04" PRIx16;
        snprintf(hex, sizeof(hex), fmt, (uint16_t)code);
    }

    size_t decLen = strlen(dec);
    size_t hexLen = strlen(hex);
    const char *primary;
    const char *secondary = NULL;
    size_t primaryLen;
    size_t secondaryLen = 0;
    if (decLen > 0) {
        primary = dec;
        primaryLen = decLen;
        if (hexLen > 0) {
            secondary = hex;
            secondaryLen = hexLen;
        }
    } else {
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
    if (destSize <
        primaryLen + strlen(lparen) + secondaryLen + strlen(rparen) + 1) {
        // Cannot fit secondary, leave it out entirely
        return;
    }

    strcat(dest, lparen);
    strcat(dest, secondary);
    strcat(dest, rparen);
}

bool RERR_Error_HasInfo(RERR_ErrorPtr error) {
    if (!error || error == RERR_OUT_OF_MEMORY) {
        return false;
    }
    return error->info && !RERR_InfoMap_IsEmpty(error->info);
}

RERR_InfoMapPtr RERR_Error_GetInfo(RERR_ErrorPtr error) {
    if (!error || error == RERR_OUT_OF_MEMORY || !error->info) {
        RERR_InfoMapPtr ret = RERR_InfoMap_Create();
        RERR_InfoMap_MakeImmutable(ret);
        return ret;
    }
    return RERR_InfoMap_ImmutableCopy(error->info);
}

const char *RERR_Error_GetMessage(RERR_ErrorPtr error) {
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

bool RERR_Error_HasCause(RERR_ErrorPtr error) {
    if (!error || error == RERR_OUT_OF_MEMORY) {
        return false;
    }
    return error->cause != NULL;
}

RERR_ErrorPtr RERR_Error_GetCause(RERR_ErrorPtr error) {
    if (!error || error == RERR_OUT_OF_MEMORY) {
        return NULL;
    }
    return error->cause;
}

bool RERR_Error_IsOutOfMemory(RERR_ErrorPtr error) {
    return error == RERR_OUT_OF_MEMORY;
}
