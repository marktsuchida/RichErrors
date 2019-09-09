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

/// Create an error map.
/**
 * The map will automatically assign error codes between `minMappedCode` and
 * `maxMappedCode`. It is allowed for `minMappedCode` to be greater than
 * `maxmappedCode`, in which case codes between `minMappedCode` and the maximum
 * value of `int32_t`, as well as codes between the minimum value of `int32_t`
 * and `maxMappedCode`, are used.
 *
 * The created map can be accessed from any thread. The caller is responsible
 * for synchronizing creation of the map with access and destruction.
 *
 * An error is returned in the case of internal memory allocation failure.
 */
RERR_ErrorPtr RERR_ErrorMap_Create(RERR_ErrorMapPtr* map,
    int32_t minMappedCode, int32_t maxMappedCode, int32_t noErrorCode);

/// Destroy an error map.
/**
 * The caller is responsible for synchronizing destruction of the map with
 * creation and access.
 */
void RERR_ErrorMap_Destroy(RERR_ErrorMapPtr map);

/// Assign an integer code to a rich error object.
/**
 * A code that is unique within the current thread is selected and returned in
 * the location given by `mappedCode`. This function takes ownership of the
 * error object, which should not be accessed after this function returns
 * successfully.
 *
 * An error is returned in the case of internal memory allocation failure, or
 * if all error codes allowed by the map have been registered on the current
 * thread.
 *
 * If this function returns an error, the error passed in remains valid and the
 * caller is responsible for destroying it. (This is so that fallback actions
 * can be taken, such as logging the error that could not be handled.)
 */
RERR_ErrorPtr RERR_ErrorMap_RegisterThreadLocal(RERR_ErrorMapPtr map,
    RERR_ErrorPtr error, int32_t* mappedCode);

/// Retrieve the rich error object registered with the given code.
/**
 * The error, previously registered via RERR_ErrorMap_RegisterThreadLocal(), is
 * returned in the location given by the `error` parameter. The caller is
 * responsible for destroying the error when done with it. The error is
 * unregistered from the map.
 *
 * An error is returned if the given `mappedCode` has not been registered or
 * has already been retrieved or otherwise unregistered.
 */
RERR_ErrorPtr RERR_ErrorMap_RetrieveThreadLocal(RERR_ErrorMapPtr map,
    int32_t mappedCode, RERR_ErrorPtr* error);

/// Clear the error code registrations for the current thread.
/**
 * A central part of the application can call this function at carefully chosen
 * moments to prevent accumulation of leaked registrations (such leakage is
 * expected because there is no resource management in parts of legacy code
 * that pass around only the integer code).
 */
void RERR_ErrorMap_ClearThreadLocal(RERR_ErrorMapPtr map);


#ifdef __cplusplus
} // extern "C"
#endif
