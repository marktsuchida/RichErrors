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
 * \brief A key-value map with string keys and POD values.
 *
 * Note that these functions, though documented, are internal only and not part
 * of the RichErrors API.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif

// User code is expected to know the type used for any key, or to handle any
// type upon retrieval. Only 64-bit numeric types are provided, for simplicity.

/// Handle for error information map.
/**
 * This is an opaque type, for use as a container for very small key-value
 * mappings, where the keys are strings and the values are strings, numbers, or
 * boolean.
 *
 * It is optimized for very small maps and will be very inefficient if large
 * numbers of items are added. Use in a context where an unbound number of
 * items may be added is not recommended.
 *
 * The map is initially mutable, but can be turned immutable by calling
 * RERR_InfoMap_MakeImmutable().
 */
typedef struct RERR_InfoMap* RERR_InfoMapPtr;

/// Iterator for info map.
/**
 * This is an opaque pointer to be used for iterating (in a C++-like style) the
 * items of an info map.
 */
typedef struct RERR_InfoMapItem* RERR_InfoMapIterator;

/// Value type.
typedef int32_t RERR_InfoValueType;

/// Values of ::RERR_InfoValueType.
enum {
    // Zero reserved for invalid type
    RERR_InfoValueTypeString = 1,
    RERR_InfoValueTypeBool,
    RERR_InfoValueTypeI64,
    RERR_InfoValueTypeU64,
    RERR_InfoValueTypeF64,
};

/// Error code type for info map functions.
typedef int32_t RERR_InfoMapError;
// TODO Eliminate error codes that would require error checks in client code.

/// Error codes returned by info map functions.
/**
 * The possible error codes and their meaning are documented for each function.
 */
enum {
    RERR_InfoMapNoError = 0,
    RERR_InfoMapErrorOutOfMemory,
    RERR_InfoMapErrorNullArg,
    RERR_InfoMapErrorMapImmutable,
    RERR_InfoMapErrorKeyNotFound,
    RERR_InfoMapErrorWrongType,
};

/// Create an info map.
/**
 * \return Null if allocation failed.
 * \return Opaque pointer to map otherwise.
 *
 * \sa RERR_InfoMap_Destroy()
 */
RERR_InfoMapPtr RERR_InfoMap_Create(void);

/// Destroy an info map.
/**
 * If \p map is null, nothing is done.
 *
 * \sa RERR_InfoMap_Create()
 */
void RERR_InfoMap_Destroy(RERR_InfoMapPtr map);

/// Make a copy of an info map.
/**
 * The copy is semantically a deep copy, meaning that no keys or values share
 * storage.
 *
 * If \p source is immutable, the returned map is also immutable and may share
 * storage with the source.
 *
 * The return value is guaranteed to be non-null if \p source is not null and
 * is immutable.
 *
 * \return Null if allocation failed or if \p source is null.
 * \return Opaque pointer to the copy otherwise.
 */
RERR_InfoMapPtr RERR_InfoMap_Copy(RERR_InfoMapPtr source);

/// Make a mutable copy of an info map.
/**
 * The copy is mutable even if the source is immutable.
 *
 * \return Null if allocation failed or if \p source is null.
 * \return Opaque pointer to the copy otherwise.
 */
RERR_InfoMapPtr RERR_InfoMap_MutableCopy(RERR_InfoMapPtr source);

/// Make an immutable copy of an info map.
/**
 * The copy is immutable even if the source is mutable.
 *
 * This is a convenience function equivalent to calling RERR_InfoMap_Copy()
 * followed by RERR_InfoMap_MakeImmutable().
 *
 * \return Null if allocation failed or if \p source is null.
 * \return Opaque pointer to the copy otherwise.
 */
RERR_InfoMapPtr RERR_InfoMap_ImmutableCopy(RERR_InfoMapPtr source);

/// Forbid further modification of an info map.
/**
 * If \p map is null, nothing is done. If \p map is already immutable, nothing
 * is done.
 *
 * \sa RERR_InfoMap_IsImmutable()
 */
void RERR_InfoMap_MakeImmutable(RERR_InfoMapPtr map);

/// Return whether an info map is mutable.
/**
 * \return `true` if \p map is mutable.
 * \return `false` if \p map is immutable or null.
 *
 * \sa RERR_InfoMap_MakeImmutable()
 */
bool RERR_InfoMap_IsMutable(RERR_InfoMapPtr map);

/// Return the number of items in an info map.
/**
 * \return Zero if \p map is null.
 * \return Number of items in \p map otherwise.
 */
size_t RERR_InfoMap_GetSize(RERR_InfoMapPtr map);

/// Return whether an info map is empty.
/**
 * \return `true` if \p map is empty or null.
 * \return `false` otherwise.
 */
bool RERR_InfoMap_IsEmpty(RERR_InfoMapPtr map);

/// Pre-allocate space for the given number of items.
/**
 * If \p map is null or immutable, nothing is done.
 *
 * If \p capacity is smaller than the current size of \p map, the current size
 * is used as the capacity hint. Thus this function can be called with a \p
 * capacity of zero, with the effect of releasing excess memory.
 *
 * In all cases, the actual capacity after this function returns may be greater
 * than the given hint.
 *
 * If an allocation failure occurs, nothing is changed and the map remains
 * valid.
 */
void RERR_InfoMap_ReserveCapacity(RERR_InfoMapPtr map, size_t capacity);

/// Add or replace a string value in an info map.
/**
 * Both the key and the value are copied.
 *
 * \return ::RERR_InfoMapErrorOutOfMemory if allocation failed.
 * \return ::RERR_InfoMapErrorMapImmutable if \p map is immutable.
 * \return ::RERR_InfoMapErrorNullArg if any of \p map, \p key, or \p value is null.
 * \return ::RERR_InfoMapNoError otherwise.
 */
RERR_InfoMapError RERR_InfoMap_SetString(RERR_InfoMapPtr map, const char* key, const char* value);

/// Add or replace a boolean value in an info map.
/**
 * The key is copied.
 *
 * \return ::RERR_InfoMapErrorOutOfMemory if allocation failed.
 * \return ::RERR_InfoMapErrorMapImmutable if \p map is immutable.
 * \return ::RERR_InfoMapErrorNullArg if either \p map or \p key is null.
 * \return ::RERR_InfoMapNoError otherwise.
 */
RERR_InfoMapError RERR_InfoMap_SetBool(RERR_InfoMapPtr map, const char* key, bool value);

/// Add or replace a signed integer value in an info map.
/**
 * The key is copied.
 *
 * \return ::RERR_InfoMapErrorOutOfMemory if allocation failed.
 * \return ::RERR_InfoMapErrorMapImmutable if \p map is immutable.
 * \return ::RERR_InfoMapErrorNullArg if either \p map or \p key is null.
 * \return ::RERR_InfoMapNoError otherwise.
 */
RERR_InfoMapError RERR_InfoMap_SetI64(RERR_InfoMapPtr map, const char* key, int64_t value);

/// Add or replace an unsigned integer value in an info map.
/**
 * The key is copied.
 *
 * \return ::RERR_InfoMapErrorOutOfMemory if allocation failed.
 * \return ::RERR_InfoMapErrorMapImmutable if \p map is immutable.
 * \return ::RERR_InfoMapErrorNullArg if either \p map or \p key is null.
 * \return ::RERR_InfoMapNoError otherwise.
 */
RERR_InfoMapError RERR_InfoMap_SetU64(RERR_InfoMapPtr map, const char* key, uint64_t value);

/// Add or replace a floating point value in an info map.
/**
 * The key is copied.
 *
 * \return ::RERR_InfoMapErrorOutOfMemory if allocation failed.
 * \return ::RERR_InfoMapErrorMapImmutable if \p map is immutable.
 * \return ::RERR_InfoMapErrorNullArg if either \p map or \p key is null.
 * \return ::RERR_InfoMapNoError otherwise.
 */
RERR_InfoMapError RERR_InfoMap_SetF64(RERR_InfoMapPtr map, const char* key, double value);

/// Remove a key from an info map.
/**
 * Nothing is done if either \p map or \p key is null, or if \p key is not
 * found in \p map, or if \p map is immutable.
 *
 * \return `true` if removal took place; otherwise `false`.
 */
bool RERR_InfoMap_Remove(RERR_InfoMapPtr map, const char* key);

/// Remove all items from an info map.
/**
 * Nothing is done if \p map is null or immutable.
 */
void RERR_InfoMap_Clear(RERR_InfoMapPtr map);

/// Return whether an info map contains the given key.
/**
 * \return `true` if \p map contains \p key.
 * \return `false` if \p map is null or does not contain \p key, or if \p key
 * is null.
 */
bool RERR_InfoMap_HasKey(RERR_InfoMapPtr map, const char* key);

/// Get the value type for the given key.
/**
 * \return ::RERR_InfoMapErrorNullArg if \p map, \p key, or \p type is null.
 * \return ::RERR_InfoMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::RERR_InfoMapNoError otherwise, in which case `*type` is valid.
 */
RERR_InfoMapError RERR_InfoMap_GetType(RERR_InfoMapPtr map, const char* key, RERR_InfoValueType* type);

/// Retrieve a string value from an info map.
/**
 * `*value` is set to an internal copy of the string value, which is valid
 * until the map is destroyed (if the map is immutable) or until the key is
 * removed or overwritten (if the map is mutable).
 *
 * \return ::RERR_InfoMapErrorNullArg if \p map or \p key or \p value is null.
 * \return ::RERR_InfoMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::RERR_InfoMapErrorWrongType if the value stored under \p key is not a
 * string.
 * \return ::RERR_InfoMapNoError otherwise, in which case `*value` is valid.
 */
RERR_InfoMapError RERR_InfoMap_GetString(RERR_InfoMapPtr map, const char* key, const char** value);

/// Retrieve a boolean value from an info map.
/**
 * \return ::RERR_InfoMapErrorNullArg if \p map or \p key is null.
 * \return ::RERR_InfoMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::RERR_InfoMapErrorWrongType if the value stored under \p key is not a
 * boolean.
 * \return ::RERR_InfoMapNoError otherwise, in which case `*value` is valid.
 */
RERR_InfoMapError RERR_InfoMap_GetBool(RERR_InfoMapPtr map, const char* key, bool* value);

/// Retrieve a signed integer value from an info map.
/**
 * \return ::RERR_InfoMapErrorNullArg if \p map or \p key is null.
 * \return ::RERR_InfoMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::RERR_InfoMapErrorWrongType if the value stored under \p key is not a
 * signed integer.
 * \return ::RERR_InfoMapNoError otherwise, in which case `*value` is valid.
 */
RERR_InfoMapError RERR_InfoMap_GetI64(RERR_InfoMapPtr map, const char* key, int64_t* value);

/// Retrieve an unsigned integer value from an info map.
/**
 * \return ::RERR_InfoMapErrorNullArg if \p map or \p key is null.
 * \return ::RERR_InfoMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::RERR_InfoMapErrorWrongType if the value stored under \p key is not an
 * unsigned integer.
 * \return ::RERR_InfoMapNoError otherwise, in which case `*value` is valid.
 */
RERR_InfoMapError RERR_InfoMap_GetU64(RERR_InfoMapPtr map, const char* key, uint64_t* value);

/// Retrieve a floating point value from an info map.
/**
 * \return ::RERR_InfoMapErrorNullArg if \p map or \p key is null.
 * \return ::RERR_InfoMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::RERR_InfoMapErrorWrongType if the value stored under \p key is not a
 * floating point.
 * \return ::RERR_InfoMapNoError otherwise, in which case `*value` is valid.
 */
RERR_InfoMapError RERR_InfoMap_GetF64(RERR_InfoMapPtr map, const char* key, double* value);

/*
 * TODO
 *
 * - Iterate keys
 * - C++ interface
 */

#ifdef __cplusplus
} // extern "C"
#endif
