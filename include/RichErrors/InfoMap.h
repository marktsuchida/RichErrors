// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

/** \file
 * \brief A key-value map with string keys and POD values.
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
 * RERR_InfoMap_MakeImmutable(). Typical usage is to make the map immutable as
 * soon as the necessary values are added.
 *
 * In general, the map API is desinged to minimize the need for error checking
 * when constructing: the mutating functions do not return error status.
 * Runtime errors (out of memory) and programming errors (such as passing a
 * null key to a mutating method) are handled silently in a way that does not
 * disrupt the code constructing the map. This design choice was made because
 * error handling code has the tendency to be poorly tested, and reporting an
 * error back to code that is already handling another error is unlikely to end
 * well unless the code is very carefully written (which in turn would take
 * disproportionate effort).
 *
 * Because info maps are used to convey error information, it is desirable for
 * them to never cause their own errors. To this end, an info map can switch
 * into an "out-of-memory" mode if any allocation for its internal storage
 * fails. This way, error reporting code can continue to run, and the reported
 * error will gracefully degrade such that it is still valid, albeit without
 * the extra information.
 */
typedef struct RERR_InfoMap *RERR_InfoMapPtr;

/// Iterator for info map.
/**
 * This is an opaque pointer to be used for iterating (in a C++-like style) the
 * items of an info map.
 */
typedef struct RERR_InfoMapItem *RERR_InfoMapIterator;

/// Value type.
typedef int32_t RERR_InfoValueType;

/// Values of ::RERR_InfoValueType.
enum {
    RERR_InfoValueTypeInvalid = 0, ///< Indicates absence of value
    RERR_InfoValueTypeString,      ///< String value
    RERR_InfoValueTypeBool,        ///< Boolean value
    RERR_InfoValueTypeI64,         ///< Signed integer value
    RERR_InfoValueTypeU64,         ///< Unsigned integer value
    RERR_InfoValueTypeF64,         ///< Floating-point value
};

/// Create an info map.
/**
 * \return Opaque pointer to map (never null).
 *
 * \sa RERR_InfoMap_Destroy()
 */
RERR_InfoMapPtr RERR_InfoMap_Create(void);

/// Create an info map, simulating an allocation failure.
/**
 * This function is provided for testing purposes.
 */
RERR_InfoMapPtr RERR_InfoMap_CreateOutOfMemory(void);

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
 * As a consequence, copying is not thread-safe: making a copy is not
 * not sufficient for safe sharing of a map across threads. This is
 * intentional, as info maps are not intended for such use. (See the
 * documentation for RERR_Error_Copy()). If you need a new copy of the map that
 * is independent of the original (for example, because you want to add to it
 * or modify it before attaching to a new error), you should use
 * RERR_InfoMap_MutableCopy().
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
 * \sa RERR_InfoMap_Copy()
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
 * \sa RERR_InfoMap_Copy()
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

/// Simulate a mutating operation that causes an allocation error.
/**
 * This function is provided for testing purposes.
 */
void RERR_InfoMap_MakeOutOfMemory(RERR_InfoMapPtr map);

/// Return whether an info map is in the out-of-memory state.
/**
 * \return `true` if \p map is in the out-of-memory state.
 * \return `false` if \p map is null or it is not in the out-of-memory state.
 */
bool RERR_InfoMap_IsOutOfMemory(RERR_InfoMapPtr map);

/// Return whether the map has seen incorrect construction.
bool RERR_InfoMap_HasProgrammingErrors(RERR_InfoMapPtr map);

/// Get error message for incorrect construction.
size_t RERR_InfoMap_GetProgrammingErrors(RERR_InfoMapPtr map, char *dest,
                                         size_t destSize);

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
 * Both the key and the value are copied into the map.
 *
 * If \p map is null, nothing is done.
 *
 * If \p key or \p value is null, or if \p map is immutable, behavior is
 * undefined.
 *
 * If memory cannot be allocated to store the key and value, the map will
 * switch to out-of-memory mode.
 *
 * \param map the info map, which should not be null
 * \param[in] key the key string, which must not be null
 * \param[in] value the value string, which must not be null
 */
void RERR_InfoMap_SetString(RERR_InfoMapPtr map, const char *key,
                            const char *value);

/// Add or replace a boolean value in an info map.
/**
 * The key is copied.
 *
 * If \p map is null, nothing is done.
 *
 * If \p key is null, or if \p map is immutable, behavior is undefined.
 *
 * If memory cannot be allocated to store the key and value, the map will
 * switch to out-of-memory mode.
 *
 * \param map the info map, which should not be null
 * \param[in] key the key string, which must not be null
 * \param[in] value the boolean value
 */
void RERR_InfoMap_SetBool(RERR_InfoMapPtr map, const char *key, bool value);

/// Add or replace a signed integer value in an info map.
/**
 * The key is copied.
 *
 * If \p map is null, nothing is done.
 *
 * If \p key is null, or if \p map is immutable, behavior is undefined.
 *
 * If memory cannot be allocated to store the key and value, the map will
 * switch to out-of-memory mode.
 *
 * \param map the info map, which should not be null
 * \param[in] key the key string, which must not be null
 * \param[in] value the signed integer value
 */
void RERR_InfoMap_SetI64(RERR_InfoMapPtr map, const char *key, int64_t value);

/// Add or replace an unsigned integer value in an info map.
/**
 * The key is copied.
 *
 * If \p map is null, nothing is done.
 *
 * If \p key is null, or if \p map is immutable, behavior is undefined.
 *
 * If memory cannot be allocated to store the key and value, the map will
 * switch to out-of-memory mode.
 *
 * \param map the info map, which should not be null
 * \param[in] key the key string, which must not be null
 * \param[in] value the unsigned integer value
 */
void RERR_InfoMap_SetU64(RERR_InfoMapPtr map, const char *key, uint64_t value);

/// Add or replace a floating point value in an info map.
/**
 * The key is copied.
 *
 * If \p map is null, nothing is done.
 *
 * If \p key is null, or if \p map is immutable, behavior is undefined.
 *
 * If memory cannot be allocated to store the key and value, the map will
 * switch to out-of-memory mode.
 *
 * \param map the info map, which should not be null
 * \param[in] key the key string, which must not be null
 * \param[in] value the floating-point value
 */
void RERR_InfoMap_SetF64(RERR_InfoMapPtr map, const char *key, double value);

/// Remove a key from an info map.
/**
 * Nothing is done if \p map is null or if \p map does not contain \p key.
 *
 * If \p map is immutable or \p key is null, behavior is undefined.
 */
void RERR_InfoMap_Remove(RERR_InfoMapPtr map, const char *key);

/// Remove all items from an info map.
/**
 * Nothing is done if \p map is null.
 *
 * If \p map is immutable, behavior is undefined.
 */
void RERR_InfoMap_Clear(RERR_InfoMapPtr map);

/// Return whether an info map contains the given key.
/**
 * \return `true` if \p map contains \p key.
 * \return `false` if \p map is null or does not contain \p key, or if \p key
 * is null.
 */
bool RERR_InfoMap_HasKey(RERR_InfoMapPtr map, const char *key);

/// Get the value type for the given key.
/**
 * \return ::RERR_InfoValueTypeInvalid if \p map or \p key is null or if \p map
 * does not contain \p key.
 * \return the type of the value stored under key otherwise
 */
RERR_InfoValueType RERR_InfoMap_GetType(RERR_InfoMapPtr map, const char *key);

/// Retrieve a string value from an info map.
/**
 * If \p map or \p key is null, or if \p map does not contain a string value
 * for \p key, then `*value` is set to NULL and `false` is returned.
 *
 * If \p value is null, nothing is done and `false` is returned.
 *
 * Otherwise `*value` is set to an internal copy of the string value, and
 * `true` is returned. The string returned in `*value` is valid until the map
 * is destroyed (if the map is immutable) or until the key is removed or
 * overwritten (if the map is mutable).
 *
 * \param map the info map
 * \param[in] key the key
 * \param[out] value location for pointer to the string value
 * \return `true` if a string could be retrieved and has been set to `*value`
 * \return `false` otherwise
 */
bool RERR_InfoMap_GetString(RERR_InfoMapPtr map, const char *key,
                            const char **value);

/// Retrieve a boolean value from an info map.
/**
 * If \p map or \p key is null, or if \p map does not contain a boolean value
 * for \p key, `false` is returned and `*value` is undefined.
 *
 * If \p value is null, nothing is done and `false` is returned.
 *
 * Otherwise `*value` is set to the boolean value for \p key.
 *
 * \param map the info map
 * \param[in] key the key
 * \param[out] value location for the boolean value
 */
bool RERR_InfoMap_GetBool(RERR_InfoMapPtr map, const char *key, bool *value);

/// Retrieve a signed integer value from an info map.
/**
 * If \p map or \p key is null, or if \p map does not contain a signed integer
 * value for \p key, `false` is returned and `*value` is undefined.
 *
 * If \p value is null, nothing is done and `false` is returned.
 *
 * Otherwise `*value` is set to the signed integer value for \p key.
 *
 * \param map the info map
 * \param[in] key the key
 * \param[out] value location for the signed integer value
 */
bool RERR_InfoMap_GetI64(RERR_InfoMapPtr map, const char *key, int64_t *value);

/// Retrieve an unsigned integer value from an info map.
/**
 * If \p map or \p key is null, or if \p map does not contain a unsigned
 * integer value for \p key, `false` is returned and `*value` is undefined.
 *
 * If \p value is null, nothing is done and `false` is returned.
 *
 * Otherwise `*value` is set to the unsigned integer value for \p key.
 *
 * \param map the info map
 * \param[in] key the key
 * \param[out] value location for the unsigned integer value
 */
bool RERR_InfoMap_GetU64(RERR_InfoMapPtr map, const char *key,
                         uint64_t *value);

/// Retrieve a floating point value from an info map.
/**
 * If \p map or \p key is null, or if \p map does not contain a floating-point
 * value for \p key, `false` is returned and `*value` is undefined.
 *
 * If \p value is null, nothing is done and `false` is returned.
 *
 * Otherwise `*value` is set to the floating-point value for \p key.
 *
 * \param map the info map
 * \param[in] key the key
 * \param[out] value location for the floating-point value
 */
bool RERR_InfoMap_GetF64(RERR_InfoMapPtr map, const char *key, double *value);

/// Get the beginning of the map as an iterator.
/**
 * Like a C++ iterator, the result can be used to iterate over all items in the
 * map.
 *
 * \sa RERR_InfoMap_End(), RERR_InfoMap_Advance()
 */
RERR_InfoMapIterator RERR_InfoMap_Begin(RERR_InfoMapPtr map);

/// Get the end of the map as an iterator.
/**
 * Like a C++ iterator, the result points to the past-last position of the map.
 *
 * \sa RERR_InfoMap_Begin(), RERR_InfoMap_Advance()
 */
RERR_InfoMapIterator RERR_InfoMap_End(RERR_InfoMapPtr map);

/// Advance an iterator.
/**
 * This is analogous to incrementing a C++ iterator. A loop over all items of a
 * map may be written like this:
 *
 *     RERR_InfoMapIterator begin = RERR_InfoMap_Begin(map);
 *     RERR_InfoMapIterator end = RERR_InfoMap_End(map);
 *     for (RERR_InfoMapIterator it = begin; it != end; it =
 * RERR_InfoMap_Advance(map, it)) { const char* key =
 * RERR_InfoMapIterator_GetKey(it);
 *         // ...
 *     }
 *
 * Iterator support for info maps is intended for reading out items only.
 * Iterators are invalid if the map is mutable and is modified.
 */
RERR_InfoMapIterator RERR_InfoMap_Advance(RERR_InfoMapPtr map,
                                          RERR_InfoMapIterator it);

/// Get the key of an item pointed to by an iterator.
const char *RERR_InfoMapIterator_GetKey(RERR_InfoMapIterator it);

/// Get the value type of an item pointed to by an iterator.
RERR_InfoValueType RERR_InfoMapIterator_GetType(RERR_InfoMapIterator it);

/// Get the string value of an item pointed to by an iterator.
/**
 * If the item does not contain a string value, behavior is undefined.
 */
const char *RERR_InfoMapIterator_GetString(RERR_InfoMapIterator it);

/// Get the boolean value of an item pointed to by an iterator.
/**
 * If the item does not contain a boolean value, behavior is undefined.
 */
bool RERR_InfoMapIterator_GetBool(RERR_InfoMapIterator it);

/// Get the signed integer value of an item pointed to by an iterator.
/**
 * If the item does not contain a signed integer value, behavior is undefined.
 */
int64_t RERR_InfoMapIterator_GetI64(RERR_InfoMapIterator it);

/// Get the unsigned integer value of an item pointed to by an iterator.
/**
 * If the item does not contain a unsigned integer value, behavior is
 * undefined.
 */
uint64_t RERR_InfoMapIterator_GetU64(RERR_InfoMapIterator it);

/// Get the floating-point value of an item pointed to by an iterator.
/**
 * If the item does not contain a floating-point value, behavior is undefined.
 */
double RERR_InfoMapIterator_GetF64(RERR_InfoMapIterator it);

#ifdef __cplusplus
} // extern "C"
#endif
