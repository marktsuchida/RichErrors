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
 * \brief Public header for Error2Code library.
 */

#include "RichErrors/RichErrors.h"


#ifdef __cplusplus
extern "C" {
#endif


/// Handle for error map.
/**
 * This is an opaque type.
 *
 * An error map is a thread-local map from integer error codes to rich errors,
 * used to transmit rich error information in a system or API that is designed
 * for integer error codes.
 *
 * Each thread uses an independent map, although the generated error codes may
 * be kept as globally unique as possible, to decrease the chance of confusion
 * during debugging.
 *
 * Use RERR_ErrorMap_Create() and RERR_ErrorMap_Destroy() to allocate and
 * deallocate an error map.
 *
 * Use RERR_ErrorMap_RegisterThreadLocal() to obtain an integer error code for
 * an error, and RERR_ErrorMap_RetrieveThreadLocal() to retrieve the error by
 * code.
 *
 * Use RERR_ErrorMap_ClearThreadLocal() to release all registered errors.
 */
typedef struct RERR_ErrorMap* RERR_ErrorMapPtr;

/// Configuration for error map.
/**
 * The map will automatically assign error codes between `minMappedCode` and
 * `maxMappedCode`. It is allowed for `minMappedCode` to be greater than
 * `maxmappedCode`, in which case codes between `minMappedCode` and the maximum
 * value of `int32_t`, as well as codes between the minimum value of `int32_t`
 * and `maxMappedCode`, are used.
 *
 * `noErrorCode`, `outOfMemoryCode`, and `mapFailureCode` must not be in the
 * above range. `outOfMemoryCode` and `mapFailureCode` may be equal to each
 * other, but not to `noErrorCode`.
 *
 * The map will use `outOfMemoryCode` if it could not map an error to a code
 * due to memory allocation failure. It will use `mapFailureCode` if it could
 * not map an error to a code for any other reason (for example code range
 * exhaustion). `outOfMemoryCode` is also used when the original error
 * indicated out-of-memory.
 */
typedef struct RERR_ErrorMapConfig {
    int32_t minMappedCode; ///< Minimum of code range
    int32_t maxMappedCode; ///< Maximum of code range
    int32_t noErrorCode; ///< Code to use when no error
    int32_t outOfMemoryCode; ///< Code used when out of memory
    int32_t mapFailureCode; ///< Code used when mapping failed

    // Maintainer: Never change or add fields once released. In the unlikely
    // event that new fields are needed, a struct with a different name should
    // be used. This is to prevent silent misconfiguration.
} RERR_ErrorMapConfig;

/// Create an error map.
/**
 * The created map can be accessed from any thread. The caller is responsible
 * for synchronizing creation of the map with access and destruction.
 *
 * An error is returned in the case of internal memory allocation failure.
 */
RERR_ErrorPtr RERR_ErrorMap_Create(RERR_ErrorMapPtr* map,
    const RERR_ErrorMapConfig* config);

/// Destroy an error map.
/**
 * The caller is responsible for synchronizing destruction of the map with
 * creation and access.
 */
void RERR_ErrorMap_Destroy(RERR_ErrorMapPtr map);

/// Assign an integer code to a rich error object.
/**
 * A code that is unique within the current thread is selected and returned.
 * If, for any reason, a code could not be assigned, a code indicating
 * out-of-memory or failure is returned.
 *
 * In either case, this function takes ownership of the error object, which
 * should not be accessed after this function returns successfully.
 */
int32_t RERR_ErrorMap_RegisterThreadLocal(RERR_ErrorMapPtr map,
    RERR_ErrorPtr error);

/// Return whether an error is registered under the given code.
/**
 * In the case where the given code is one of the special codes (no-error,
 * out-of-memory, map-failure), true is returned.
 */
bool RERR_ErrorMap_IsRegisteredThreadLocal(RERR_ErrorMapPtr map, int32_t code);

/// Retrieve the rich error object registered with the given code.
/**
 * The error, previously registered via RERR_ErrorMap_RegisterThreadLocal(), is
 * returned. The caller is responsible for destroying the error when done with
 * it. The error is unregistered from the map.
 *
 * If the given code is not registered, a new error is returned, with domain
 * RERR_DOMAIN_RICHERRORS and code RERR_ECODE_MAP_INVALID_CODE.
 */
RERR_ErrorPtr RERR_ErrorMap_RetrieveThreadLocal(RERR_ErrorMapPtr map,
    int32_t mappedCode);

/// Clear the error code registrations for the current thread.
/**
 * A central part of the application can call this function at carefully chosen
 * moments to prevent accumulation of leaked registrations (such leakage is
 * expected because there is no resource management in parts of legacy code
 * that pass around only the integer code).
 *
 * Nothing is done if the given map is NULL.
 */
void RERR_ErrorMap_ClearThreadLocal(RERR_ErrorMapPtr map);


#ifdef __cplusplus
} // extern "C"
#endif
