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

#pragma once

/** \file
 * \brief Public header for the RichErrors library.
 */

/** \mainpage RichErrors: A C library for rich (nested) error information.
 * \author Mark A. Tsuchida
 *
 * RichErrors is a small library for supporting error handling in C or C++
 * software consisting of multiple dynamically loaded modules (DLLs, shared
 * objects, Mach-O bundles, etc.). It uses a pure C interface for maximum ABI
 * compatibility between modules.
 *
 * Unlike some C libraries for "exception handling", RichErrors does not use
 * setjmp/longjmp to emulate C++-style exceptions. Instead, it extends the
 * concept of an error code return value and allows construction of nested
 * (wrapped) error objects (cf. Java Exception cause, Python `__cause__`).
 *
 * Because integer error codes defined by various subsystems can be valuable
 * information, RichErrors allows explicit wrapping of error codes (together
 * with a specific message string). In order to segregate error codes arising
 * from different subsystems or third-party libraries, every error containing
 * an error code must be assigned a pre-registered "domain".
 *
 * Known limitations and possible future extensions:
 *
 * - The error code domains are global. This limitation could be removed by
 *   introducing "realm" objects to which domains and errors belong (with a
 *   default global realm).
 *
 * - Registration of error code domains is mandatory. We could probably make
 *   registration optional, auto-registering any unencountered domain
 *   (preserving the current string-interning behavior). Pre-registration could
 *   still be optionally enforced.
 *
 * - There should be a way to specify, per domain, how to format error codes
 *   (signed decimal, unsigned decimal, or hexadecimal). This would be useful
 *   for domains such as "Windows HRESULT".
 *
 * - There is no possibility of localized error messages. This is by design
 *   because this library was created for use by non-localized software.
 */

#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


/// Handle for error information.
/**
 * This is an opaque, immutable type, designed to be used as function return
 * values (or output parameters) or as a parameter to a function that sends
 * asynchronous error notifications.
 *
 * A special value, #RERR_NO_ERROR, indicates success. An RERR_ErrorPtr can be
 * directly compared to #RERR_NO_ERROR using the `==` operator.
 *
 * Error objects can be created by RERR_Error_Create(),
 * RERR_Error_CreateWithCode(), RERR_Error_CreateOutOfMemory(),
 * RERR_Error_Wrap(), or RERR_Error_WrapWithCode().
 *
 * An error object (unless known to be #RERR_NO_ERROR) must be deallocated by
 * RERR_Error_Destroy().
 *
 * Information contained in an error object can be accessed via these
 * functions: RERR_Error_HasCode(), RERR_Error_GetDomain(),
 * RERR_Error_GetCode(), RERR_Error_GetMessage(), RERR_Error_HasCause(),
 * RERR_Error_GetCause(), RERR_Error_IsOutOfMemory().
 */
typedef struct RERR_Error* RERR_ErrorPtr;

/// Value of type ::RERR_ErrorPtr representing success (lack of error).
#define RERR_NO_ERROR ((RERR_ErrorPtr) NULL)

/// Error domain for out-of-memory error.
#define RERR_DOMAIN_OUT_OF_MEMORY "Out of memory"

/// Error code belonging to the #RERR_DOMAIN_OUT_OF_MEMORY domain.
enum {
    // Maintainer: do not change once released!

    /// Out of memory
    /**
     * Ran out of memory, either during the original operation or during error
     * handling.
     */
    RERR_ECODE_OUT_OF_MEMORY = 1,
};

/// Error domain for errors arising in RichErrors functions.
#define RERR_DOMAIN_RICHERRORS "RichErrors"

/// Error codes belonging to the #RERR_DOMAIN_RICHERRORS domain.
enum {
    // Maintainer: do not change once released!

    RERR_ECODE_NULL_ARGUMENT = 101, ///< Argument NULL when not allowed

    // Domain errors
    RERR_ECODE_DOMAIN_NAME_EMPTY = 201, ///< Domain name cannot be empty
    RERR_ECODE_DOMAIN_NAME_TOO_LONG = 202, ///< Domain name too long
    RERR_ECODE_DOMAIN_NAME_INVALID = 203, ///< Domain name contains forbidden characters
    RERR_ECODE_DOMAIN_ALREADY_EXISTS = 204, ///< Domain alraedy registered
    RERR_ECODE_DOMAIN_NOT_REGISTERED = 205, ///< Domain not registered

    // Error maps
    RERR_ECODE_MAP_INVALID_RANGE = 301,
    RERR_ECODE_MAP_INVALID_CODE = 302,
    RERR_ECODE_MAP_FULL = 303,
};

/// Unregister all previously registered error domains (for testing).
void RERR_Domain_UnregisterAll(void);

/// Register an error code domain.
/**
 * The domain must be a non-empty string of at most 63 ASCII graphic characters
 * (space is allowed). It must not contain the characters `/`, `\`, or `:`.
 *
 * Typically the domain name should be the name of the subsystem, third-party
 * library, or operating system that generates error codes, and the phrase
 * "DOMAIN error code 123" (where DOMAIN is the domain name) should make sense.
 */
RERR_ErrorPtr RERR_Domain_Register(const char* domain);

/// Create an error without an error code.
/**
 * Errors without an error code can be used when it is not expected that the
 * caller can take any error-specific actions to recover from the error. The
 * message should be a human-readable string, containing specific error
 * information. A copy of the message is made, so the string passed as message
 * may be deallocated once this function returns.
 *
 * The returned error object is owned by the caller, who is responsible for
 * deallocation.
 *
 * This function may return an out-of-memory error (not containing the given
 * message) if memory allocation fails.
 */
RERR_ErrorPtr RERR_Error_Create(const char* message);

/// Create an error with an error code.
/**
 * The domain must be an error code domain previously registered via
 * RERR_Domain_Register(). The code should be an error code identifying the
 * type of the error, uniquely within the domain. See RERR_Error_Create()
 * regarding the message.
 *
 * The returned error object is owned by the caller, who is responsible for
 * deallocation.
 *
 * If domain is `NULL` and code is zero, this function is equivalent to
 * RERR_Error_Create(). If domain is `NULL` but code is nonzero (likely a
 * programming error, as error codes without specific domain are not allowed),
 * this function fails with an error.
 *
 * This function may return an out-of-memory error (not containing the given
 * message, domain, and code) if memory allocation fails.
 */
RERR_ErrorPtr RERR_Error_CreateWithCode(const char* domain, int32_t code,
    const char* message);

/// Destroy an error object.
/**
 * This function is safe to call on any properly constructed error, including
 * #RERR_NO_ERROR and lightweight out-of-memory errors.
 */
void RERR_Error_Destroy(RERR_ErrorPtr error);

/// Copy an error object.
/**
 * Note that there should normally be no need to copy errors in C code. This
 * function exists to support copy construction of C++ exceptions that wrap an
 * error.
 *
 * Because C++ exception objects must be no-throw copyable, this function is
 * designed not to return an error. How this is achieved is an implementation
 * detail that user code should not depend upon, but the error pointer returned
 * in destination may potentially be equal to the source pointer (with internal
 * reference counting) or it may indicate an out-of-memory error. In any case,
 * the source remains owned by the caller, who is also responsible for
 * destroying the destination when done with it.
 */
void RERR_Error_Copy(RERR_ErrorPtr source, RERR_ErrorPtr* destination);

/// Create a lightweight out-of-memory error.
/**
 * This function is equivalent to creating an error with domain
 * #RERR_DOMAIN_RICHERRORS and code ::RERR_ECODE_OUT_OF_MEMORY, except that it
 * requires no dynamic memory allocation. Note that it is safe to call any of
 * the error creation functions when memory is low, because they all handle
 * allocation failures and return the same value as RERR_Create_OutOfMemory().
 */
RERR_ErrorPtr RERR_Error_CreateOutOfMemory(void);

/// Create a nested error, talking ownership of an original error.
/**
 * The cause can later be retrieved by calling RERR_Error_GetCause() on the new
 * error. The message is handled as in RERR_Error_Create().
 *
 * Ownership of the error object passed in as cause is transferred to the new
 * error, so the cause error should not be used or deallocated after this
 * function returns.
 *
 * The returned error object is owned by the caller, who is responsible for
 * deallocation.
 *
 * This function may return an out-of-memory error (not containing the given
 * message or cause) if internal memory allocation fails. Even in that case,
 * the cause error is deallocated.
 */
RERR_ErrorPtr RERR_Error_Wrap(RERR_ErrorPtr cause, const char* message);

/// Create a nested error with error code, taking ownership of the cause.
/**
 * The cause can later be retrieved by calling RERR_Error_GetCause() on the new
 * error. The message, domain, and code are handled as in
 * RERR_Error_CreateWithCode().
 *
 * Ownership of the error object passed in as cause is transferred to the new
 * error, so the cause error should not be used or deallocated after this
 * function returns.
 *
 * The returned error object is owned by the caller, who is responsible for
 * deallocation.
 *
 * This function may return an out-of-memory error (not containing the given
 * message, domain, code, or cause) if internal memory allocation fails. Even
 * in that case, the cause error is deallocated.
 */
RERR_ErrorPtr RERR_Error_WrapWithCode(RERR_ErrorPtr cause, const char* domain,
    int32_t code, const char* message);

/// Return whether the given error has an error domain and code.
bool RERR_Error_HasCode(RERR_ErrorPtr error);

/// Return the error code domain of the given error.
/**
 * If the given error does not have an error code, the empty string is
 * returned.
 *
 * The returned string is valid for the lifetime of the error object.
 */
const char* RERR_Error_GetDomain(RERR_ErrorPtr error);

/// Return the error code of the given error.
/**
 * If the given error does not have an error code, zero is returned.
 */
int32_t RERR_Error_GetCode(RERR_ErrorPtr error);

/// Return the error message of the given error.
/**
 * A human-readable description is returned in the case that the given error is
 * #RERR_NO_ERROR or the case that the given error has no message or has an
 * empty message. Thus, it is always safe to treat the return value as a C
 * string.
 *
 * The returned string is valid for the lifetime of the error object.
 */
const char* RERR_Error_GetMessage(RERR_ErrorPtr error);

/// Return whether the given error has a cause, or original error.
bool RERR_Error_HasCause(RERR_ErrorPtr error);

/// Return the cause, or original error, of the given error.
/**
 * If the given error does not have a cause, #RERR_NO_ERROR is returned.
 *
 * The returned error is not owned by the caller and is valid for the lifetime
 * of the given error.
 */
RERR_ErrorPtr RERR_Error_GetCause(RERR_ErrorPtr error);

/// Return whether the given error is an out-of-memory error.
/**
 * The return value is true if the error was created by
 * RERR_Error_CreateOutOfMemory() or if the error was returned by RichErrors as
 * a result of allocation failure.
 */
bool RERR_Error_IsOutOfMemory(RERR_ErrorPtr error);


#ifdef __cplusplus
} // extern "C"
#endif
