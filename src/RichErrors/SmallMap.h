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

/// Handle for key-value map.
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
 * SmallMap_Freeze().
 */
typedef struct SmallMap* SmallMapPtr;

/// Iterator for SmallMap.
/**
 * This is an opaque pointer to be used for iterating (in a C++-like style) the
 * items of a SmallMap.
 */
typedef struct SmallMapItem* SmallMapIterator;

/// Value type.
typedef int32_t SmallMapValueType;

/// Values of ::SmallMapValueType.
enum {
    // Zero reserved for invalid type
    SmallMapValueTypeString = 1,
    SmallMapValueTypeBool,
    SmallMapValueTypeI64,
    SmallMapValueTypeU64,
    SmallMapValueTypeF64,
};

/// Error code type for SmallMap functions.
typedef int32_t SmallMapError;

/// Error codes returned by SmallMap functions.
/**
 * The possible error codes and their meaning are documented for each function.
 */
enum {
    SmallMapNoError = 0,
    SmallMapErrorOutOfMemory,
    SmallMapErrorNullArg,
    SmallMapErrorMapFrozen,
    SmallMapErrorKeyNotFound,
    SmallMapErrorKeyExists,
    SmallMapErrorWrongType,
    SmallMapErrorDestSizeTooSmall,
};

/// Create a SmallMap.
/**
 * \return Null if allocation failed.
 * \return Opaque pointer to map otherwise.
 *
 * \sa SmallMap_Destroy()
 */
SmallMapPtr SmallMap_Create(void);

/// Destroy a SmallMap.
/**
 * If \p map is null, nothing is done.
 */
void SmallMap_Destroy(SmallMapPtr map);

/// Make a copy of a SmallMap.
/**
 * The copy is semantically a deep copy, meaning that no keys or values share
 * storage.
 *
 * If \p source is frozen, the returned map is also frozen and may share
 * storage with the source.
 *
 * The return value is guaranteed to be non-null if \p source is not null and
 * is frozen.
 *
 * \return Null if allocation failed or if \p source is null.
 * \return Opaque pointer to the copy otherwise.
 */
SmallMapPtr SmallMap_Copy(SmallMapPtr source);

/// Make a mutable copy of a SmallMap.
/**
 * The copy is unfrozen even if the source is frozen.
 *
 * \return Null if allocation failed or if \p source is null.
 * \return Opaque pointer to the copy otherwise.
 */
SmallMapPtr SmallMap_UnfrozenCopy(SmallMapPtr source);

/// Make an immutable copy of a SmallMap.
/**
 * The copy is frozen even if the source is unfrozen.
 *
 * This is a convenience function equivalent to calling SmallMap_Copy()
 * followed by SmallMap_Freeze().
 *
 * \return Null if allocation failed or if \p source is null.
 * \return Opaque pointer to the copy otherwise.
 */
SmallMapPtr SmallMap_FrozenCopy(SmallMapPtr source);

/// Forbid further modification of a SmallMap.
/**
 * If \p map is null, nothing is done. If \p map is already frozen, nothing is
 * done.
 *
 * \sa SmallMap_IsFrozen()
 */
void SmallMap_Freeze(SmallMapPtr map);

/// Return whether a SmallMap is frozen.
/**
 * Note that this function returns `true` if \p map is null. This is because
 * null cannot be mutated.
 *
 * \return `true` if \p map is frozen or null.
 * \return `false` if \p map is not frozen.
 *
 * \sa SmallMap_Freeze()
 */
bool SmallMap_IsFrozen(SmallMapPtr map);

/// Return the number of items in a SmallMap.
/**
 * \return Zero if \p map is null.
 * \return Number of items in \p map otherwise.
 */
size_t SmallMap_GetSize(SmallMapPtr map);

/// Return whether a SmallMap is empty.
/**
 * \return `true` if \p map is empty or null.
 * \return `false` otherwise.
 */
bool SmallMap_IsEmpty(SmallMapPtr map);

/// Pre-allocate space for the given number of items.
/**
 * If \p map is null or frozen, nothing is done.
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
void SmallMap_ReserveCapacity(SmallMapPtr map, size_t capacity);

/// Add or replace a string value in a SmallMap.
/**
 * Both the key and the value are copied.
 *
 * \return ::SmallMapErrorOutOfMemory if allocation failed.
 * \return ::SmallMapErrorMapFrozen if \p map is frozen.
 * \return ::SmallMapErrorNullArg if any of \p map, \p key, or \p value is null.
 * \return ::SmallMapNoError otherwise.
 */
SmallMapError SmallMap_SetString(SmallMapPtr map, const char* key, const char* value);

SmallMapError SmallMap_SetUniqueString(SmallMapPtr map, const char* key, const char* value);

/// Add or replace a boolean value in a SmallMap.
/**
 * The key is copied.
 *
 * \return ::SmallMapErrorOutOfMemory if allocation failed.
 * \return ::SmallMapErrorMapFrozen if \p map is frozen.
 * \return ::SmallMapErrorNullArg if either \p map or \p key is null.
 * \return ::SmallMapNoError otherwise.
 */
SmallMapError SmallMap_SetBool(SmallMapPtr map, const char* key, bool value);

SmallMapError SmallMap_SetUniqueBool(SmallMapPtr map, const char* key, bool value);

/// Add or replace a signed integer value in a SmallMap.
/**
 * The key is copied.
 *
 * \return ::SmallMapErrorOutOfMemory if allocation failed.
 * \return ::SmallMapErrorMapFrozen if \p map is frozen.
 * \return ::SmallMapErrorNullArg if either \p map or \p key is null.
 * \return ::SmallMapNoError otherwise.
 */
SmallMapError SmallMap_SetI64(SmallMapPtr map, const char* key, int64_t value);

SmallMapError SmallMap_SetUniqueI64(SmallMapPtr map, const char* key, int64_t value);

/// Add or replace an unsigned integer value in a SmallMap.
/**
 * The key is copied.
 *
 * \return ::SmallMapErrorOutOfMemory if allocation failed.
 * \return ::SmallMapErrorMapFrozen if \p map is frozen.
 * \return ::SmallMapErrorNullArg if either \p map or \p key is null.
 * \return ::SmallMapNoError otherwise.
 */
SmallMapError SmallMap_SetU64(SmallMapPtr map, const char* key, uint64_t value);

SmallMapError SmallMap_SetUniqueU64(SmallMapPtr map, const char* key, uint64_t value);

/// Add or replace a floating point value in a SmallMap.
/**
 * The key is copied.
 *
 * \return ::SmallMapErrorOutOfMemory if allocation failed.
 * \return ::SmallMapErrorMapFrozen if \p map is frozen.
 * \return ::SmallMapErrorNullArg if either \p map or \p key is null.
 * \return ::SmallMapNoError otherwise.
 */
SmallMapError SmallMap_SetF64(SmallMapPtr map, const char* key, double value);

SmallMapError SmallMap_SetUniqueF64(SmallMapPtr map, const char* key, double value);

/// Remove a key from a SmallMap.
/**
 * Nothing is done if either \p map or \p key is null, or if \p key is not
 * found in \p map, or if \p map is frozen.
 *
 * \return `true` if removal took place; otherwise `false`.
 */
bool SmallMap_Remove(SmallMapPtr map, const char* key);

/// Remove all items from a SmallMap.
/**
 * Nothing is done if \p map is null or frozen.
 */
void SmallMap_Clear(SmallMapPtr map);

/// Return whether a SmallMap contains the given key.
/**
 * \return `true` if \p map contains \p key.
 * \return `false` if \p map is null or does not contain \p key, or if \p key
 * is null.
 */
bool SmallMap_HasKey(SmallMapPtr map, const char* key);

/// Get the value type for the given key.
/**
 * \return ::SmallMapErrorNullArg if \p map, \p key, or \p type is null.
 * \return ::SmallMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::SmallMapNoError otherwise, in which case `*type` is valid.
 */
SmallMapError SmallMap_GetType(SmallMapPtr map, const char* key, SmallMapValueType* type);

/// Return the size required for a string value in a SmallMap.
/**
 * \return Zero if \p map or \p key is null or if \p map does not contain \p
 * key or if the value stored under \p key is not a string.
 * \return Size, in bytes, required to hold the string value, including the
 * null terminator.
 */
size_t SmallMap_GetStringSize(SmallMapPtr map, const char* key);

/// Retrieve a string value from a SmallMap.
/**
 * \return ::SmallMapErrorNullArg if \p map, \p key, or \p dest is null.
 * \return ::SmallMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::SmallMapErrorWrongType if the value stored under \p key is not a
 * string.
 * \return ::SmallMapErrorDestSizeTooSmall if \p destSize is insufficient to
 * copy the whole string value.
 * \return ::SmallMapNoError otherwise, in which case `*dest` contains the
 * string value.
 */
SmallMapError SmallMap_GetString(SmallMapPtr map, const char* key, char* dest, size_t destSize);

/// Retrieve a string value from a SmallMap, truncating if necessary.
/**
 * If the string value is too long to fit into \p destSize bytes, the first
 * `(destSize - 1)` bytes are copied into \p dest. Otherwise the behavior is
 * the same with SmallMap_GetString().
 *
 * \warning No consideration is made for non-ASCII encoding of the string. In
 * particular, UTF-8 strings may be truncated in the middle of a multi-byte
 * sequence.
 *
 * \return ::SmallMapErrorNullArg if \p map, \p key, or \p dest is null.
 * \return ::SmallMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::SmallMapErrorWrongType if the value stored under \p key is not a
 * string.
 * \return ::SmallMapErrorDestSizeTooSmall if \p destSize is insufficient to
 * copy the whole string value, in which case `*dest` contains the first
 * (destSize - 1) characters of the string value.
 * \return ::SmallMapNoError otherwise, in which case `*dest` contains the
 * string value.
 */
SmallMapError SmallMap_GetTruncatedString(SmallMapPtr map, const char* key, char* dest, size_t destSize);

/// Retrieve a boolean value from a SmallMap.
/**
 * \return ::SmallMapErrorNullArg if \p map or \p kay is null.
 * \return ::SmallMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::SmallMapErrorWrongType if the value stored under \p key is not a
 * boolean.
 * \return ::SmallMapNoError otherwise, in which case `*value` is valid.
 */
SmallMapError SmallMap_GetBool(SmallMapPtr map, const char* key, bool* value);

/// Retrieve a signed integer value from a SmallMap.
/**
 * \return ::SmallMapErrorNullArg if \p map or \p kay is null.
 * \return ::SmallMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::SmallMapErrorWrongType if the value stored under \p key is not a
 * signed integer.
 * \return ::SmallMapNoError otherwise, in which case `*value` is valid.
 */
SmallMapError SmallMap_GetI64(SmallMapPtr map, const char* key, int64_t* value);

/// Retrieve an unsigned integer value from a SmallMap.
/**
 * \return ::SmallMapErrorNullArg if \p map or \p kay is null.
 * \return ::SmallMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::SmallMapErrorWrongType if the value stored under \p key is not an
 * unsigned integer.
 * \return ::SmallMapNoError otherwise, in which case `*value` is valid.
 */
SmallMapError SmallMap_GetU64(SmallMapPtr map, const char* key, uint64_t* value);

/// Retrieve a floating point value from a SmallMap.
/**
 * \return ::SmallMapErrorNullArg if \p map or \p kay is null.
 * \return ::SmallMapErrorKeyNotFound if \p map does not contain \p key.
 * \return ::SmallMapErrorWrongType if the value stored under \p key is not a
 * floating point.
 * \return ::SmallMapNoError otherwise, in which case `*value` is valid.
 */
SmallMapError SmallMap_GetF64(SmallMapPtr map, const char* key, double* value);

/*
 * TODO
 *
 * - Iterate keys
 * - C++ interface
 */

#ifdef __cplusplus
} // extern "C"
#endif
