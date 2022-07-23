// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#define _CRT_SECURE_NO_WARNINGS
#include "RichErrors/InfoMap.h"

#include "DynArray.h"

#include <assert.h>
#include <math.h> // for nan()
#include <string.h>

#ifdef _MSC_VER
#define restrict __restrict
#endif

/*
 * The point of this map implementation is to be simple and to be efficient for
 * very small maps containing up to tens of items. Therefore all data is stored
 * in an array.
 *
 * It is well known that naive arrays are faster than trees or has tables for
 * very small maps.
 *
 * An array-based map could be sorted or unsorted. Sorting enables binary
 * search, although it is not clear if that is an advantage or disadvantage for
 * tiny maps. For now I am using a sorted implementation, primarily because of
 * the convenience of always having sorted keys when debugging (both of this
 * implementation and of code using this implementation).
 *
 * An alternative possibility would be to use separate arrays for the keys and
 * values (and possibly for the value type and actual value). In this case it
 * would be preferable to allocate a single block containing all of the arrays,
 * to minimize the number of allocations and reallocations. However, in the
 * absence of comparative benchmarking, I am choosing the simpler
 * implementation.
 */

struct Value {
    RERR_InfoValueType type;
    union {
        const char *string; // Uniquely owned
        bool boolean;
        int64_t i64;
        uint64_t u64;
        double f64;
    } value;
};

struct RERR_InfoMapItem {
    const char *key; // Uniquely owned
    struct Value value;
};

enum {
    FLAG_IMMUTABLE = 1,
    FLAG_OUT_OF_MEMORY = 2,
    FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE =
        4, // only this flag may change once immutable
    FLAG_ERROR_NULL_KEY_GIVEN = 8,
    FLAG_ERROR_NULL_VALUE_GIVEN = 16,
};

struct RERR_InfoMap {
    uint32_t flags;         // See enum constants above
    RERR_DynArrayPtr items; // Sorted by key strcmp
    size_t refCount;        // Always 1 unless frozen
};

#define INFOMAP_OUT_OF_MEMORY ((RERR_InfoMapPtr)-1)

/*
 * We handle allocation failures in two ways.
 *
 * (1) If allocation failure occurs when creating the map, we return
 * INFOMAP_OUT_OF_MEMORY. That pointer value subsequently behaves like an
 * ordinary info map, except that it always behaves as if empty upon readout.
 *
 * (2) If allocation failure occurs when adding an item to an existing map, we
 * set FLAG_OUT_OF_MEMORY and deallocate all memory used for item storage. In
 * this case, the map again stores nothing and always behaves as if empty upon
 * readout, but the other flags and refCount remain valid.
 */

static inline void FreeConst(const void *m) { free((void *)m); }

// Precondition: it != NULL
static inline void ClearItem(RERR_InfoMapIterator it) {
    FreeConst(it->key);

    switch (it->value.type) {
    case RERR_InfoValueTypeString:
        FreeConst(it->value.value.string);
        break;
    default:
        break;
    }
}

// Precondition: src != NULL
// Precondition: dst != NULL
// Precondition: src != dst
static bool DeepCopyItem(struct RERR_InfoMapItem *restrict dst,
                         struct RERR_InfoMapItem *restrict src) {
    char *keyCopy = NULL;
    char *strCopy = NULL;

    size_t keyLen = strlen(src->key);
    keyCopy = malloc(keyLen + 1);
    if (!keyCopy) {
        goto error;
    }
    strncpy(keyCopy, src->key, keyLen + 1);
    dst->key = keyCopy;

    switch (src->value.type) {
    case RERR_InfoValueTypeString:
        dst->value.type = RERR_InfoValueTypeString;
        size_t len = strlen(src->value.value.string);
        strCopy = malloc(len + 1);
        if (!strCopy) {
            goto error;
        }
        strncpy(strCopy, src->value.value.string, len + 1);
        dst->value.value.string = strCopy;
        break;

    default:
        memcpy(&dst->value, &src->value, sizeof(src->value));
        break;
    }

    return true;

error:
    free(strCopy);
    free(keyCopy);
    return false;
}

// Precondition: map != NULL
static void Clear(RERR_InfoMapPtr map) {
    RERR_InfoMapIterator begin = RERR_DynArray_Begin(map->items);
    RERR_InfoMapIterator end = RERR_DynArray_End(map->items);
    for (RERR_InfoMapIterator it = begin; it != end; ++it) {
        ClearItem(it);
    }
    RERR_DynArray_Clear(map->items);
}

// Precondition: map != NULL
static RERR_InfoMapPtr MutableCopy(RERR_InfoMapPtr source) {
    RERR_InfoMapPtr ret = RERR_InfoMap_Create();
    if (!ret) {
        goto error;
    }
    if (RERR_InfoMap_IsOutOfMemory(ret)) {
        return ret;
    }

    RERR_InfoMapIterator begin = RERR_DynArray_Begin(source->items);
    RERR_InfoMapIterator end = RERR_DynArray_End(source->items);
    for (RERR_InfoMapIterator it = begin; it != end; ++it) {
        RERR_InfoMapIterator ins =
            RERR_DynArray_Insert(ret->items, RERR_DynArray_End(ret->items));
        if (!ins) {
            goto error;
        }

        bool ok = DeepCopyItem(ins, it);
        if (!ok) {
            goto error;
        }
    }

    return ret;

error:
    RERR_InfoMap_Destroy(ret);
    return NULL;
}

static int ItemKeyCompare(const void *item, const void *key) {
    const struct RERR_InfoMapItem *i = item;
    const char *k = key;
    return strcmp(i->key, k);
}

static inline RERR_InfoMapIterator Find(RERR_InfoMapPtr map, const char *key) {
    return RERR_DynArray_BSearch(map->items, key, ItemKeyCompare);
}

static void SwitchToOutOfMemory(RERR_InfoMapPtr map) {
    Clear(map); // TODO May shrink, which is a wasted step
    RERR_DynArray_Destroy(map->items);
    map->items = NULL;
    map->flags |= FLAG_OUT_OF_MEMORY;
}

// Ensures capacity and finds insertion point (or NULL on error)
// Precondition: map != NULL
// Precondition: key != NULL
// Postcondition: *it == NULL || (*it)->key contains copy of key
// Postcondition: (*it)->value is invalid
// Returns true if successful; false on allocation failure
static bool SetKey(RERR_InfoMapPtr map, const char *key,
                   RERR_InfoMapIterator *it) {
    *it = RERR_DynArray_BSearchInsertionPoint(map->items, key, ItemKeyCompare);

    // Found exact, so need to overwrite
    if (*it != RERR_DynArray_End(map->items) && strcmp((*it)->key, key) == 0) {
        // Overwriting value in place.
        // Evacuate key while we clear the item.
        const char *saveKey = (*it)->key;
        (*it)->key = NULL;
        ClearItem(*it);
        (*it)->key = saveKey;
        return true;
    }

    bool ret = true;

    size_t keyLen = strlen(key);
    char *keyCopy = malloc(keyLen + 1);
    if (!keyCopy) {
        SwitchToOutOfMemory(map);
        ret = false;
        goto exit;
    }
    strncpy(keyCopy, key, keyLen + 1);

    *it = RERR_DynArray_Insert(map->items, *it);
    if (!*it) {
        SwitchToOutOfMemory(map);
        ret = false;
        goto exit;
    }

    (*it)->key = keyCopy;
    keyCopy = NULL;

exit:
    free(keyCopy);
    return ret;
}

RERR_InfoMapPtr RERR_InfoMap_Create(void) {
    RERR_InfoMapPtr ret = calloc(1, sizeof(struct RERR_InfoMap));
    if (!ret) {
        return INFOMAP_OUT_OF_MEMORY;
    }
    ret->refCount = 1;

    // TODO In-place version of DynArray create/destroy?
    ret->items = RERR_DynArray_Create(sizeof(struct RERR_InfoMapItem));
    if (!ret->items) {
        free(ret);
        return INFOMAP_OUT_OF_MEMORY;
    }

    return ret;
}

RERR_InfoMapPtr RERR_InfoMap_CreateOutOfMemory(void) {
    return INFOMAP_OUT_OF_MEMORY;
}

void RERR_InfoMap_Destroy(RERR_InfoMapPtr map) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }

    if (--map->refCount > 0) {
        return;
    }

    if (!(map->flags & FLAG_OUT_OF_MEMORY)) {
        // TODO Clear() may realloc to shrink, which is a waste
        Clear(map);
        RERR_DynArray_Destroy(map->items);
    }
    free(map);
}

RERR_InfoMapPtr RERR_InfoMap_Copy(RERR_InfoMapPtr map) {
    if (!map) {
        return NULL;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return INFOMAP_OUT_OF_MEMORY;
    }

    if (map->flags & FLAG_IMMUTABLE) {
        ++map->refCount;
        return map;
    }

    return MutableCopy(map);
}

RERR_InfoMapPtr RERR_InfoMap_MutableCopy(RERR_InfoMapPtr map) {
    if (!map) {
        return NULL;
    }
    if (RERR_InfoMap_IsOutOfMemory(map)) {
        return INFOMAP_OUT_OF_MEMORY;
    }
    return MutableCopy(map);
}

RERR_InfoMapPtr RERR_InfoMap_ImmutableCopy(RERR_InfoMapPtr map) {
    RERR_InfoMapPtr ret = RERR_InfoMap_Copy(map);
    RERR_InfoMap_MakeImmutable(ret);
    return ret;
}

void RERR_InfoMap_MakeImmutable(RERR_InfoMapPtr map) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }

    map->flags |= FLAG_IMMUTABLE;
    // TODO Shrink if there is excess capacity
}

bool RERR_InfoMap_IsMutable(RERR_InfoMapPtr map) {
    if (!map) {
        return false;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return false;
    }

    return !(map->flags & FLAG_IMMUTABLE);
}

void RERR_InfoMap_MakeOutOfMemory(RERR_InfoMapPtr map) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }
    if (map->flags & FLAG_IMMUTABLE) {
        map->flags |= FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE;
        return;
    }
    if (map->flags & FLAG_OUT_OF_MEMORY) {
        return;
    }
    SwitchToOutOfMemory(map);
}

bool RERR_InfoMap_IsOutOfMemory(RERR_InfoMapPtr map) {
    return map == INFOMAP_OUT_OF_MEMORY ||
           (map && (map->flags & FLAG_OUT_OF_MEMORY));
}

bool RERR_InfoMap_HasProgrammingErrors(RERR_InfoMapPtr map) {
    const uint32_t errorFlags = FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE |
                                FLAG_ERROR_NULL_KEY_GIVEN |
                                FLAG_ERROR_NULL_VALUE_GIVEN;
    return map->flags & errorFlags;
}

static inline void ConcatString(char *restrict dest, const char *restrict src,
                                size_t destSize) {
    if (!dest || destSize == 0 || !src) {
        return;
    }
    size_t destLen = strlen(dest);
    if (destLen < destSize - 1) {
        strncat(dest, src, destSize - 1 - destLen);
        dest[destSize - 1] = '\0';
    }
}

size_t RERR_InfoMap_GetProgrammingErrors(RERR_InfoMapPtr map, char *dest,
                                         size_t destSize) {
    static const char *const err1 = "Attempt(s) made to mutate immutable map.";
    static const char *const err2 =
        "Null key(s) passed to mutating function(s).";
    static const char *const err3 =
        "Null value(s) passed to mutating function(s).";

    if (dest && destSize > 0) {
        dest[0] = '\0';
    }
    size_t totalLen = 0;

    if (map->flags & FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE) {
        ConcatString(dest, err1, destSize);
        totalLen += strlen(err1);
    }

    if (map->flags & FLAG_ERROR_NULL_KEY_GIVEN) {
        if (totalLen > 0) {
            ConcatString(dest, " ", destSize);
            ++totalLen;
        }
        ConcatString(dest, err2, destSize);
        totalLen += strlen(err2);
    }

    if (map->flags & FLAG_ERROR_NULL_VALUE_GIVEN) {
        if (totalLen > 0) {
            ConcatString(dest, " ", destSize);
            ++totalLen;
        }
        ConcatString(dest, err3, destSize);
        totalLen += strlen(err3);
    }

    return totalLen + 1;
}

size_t RERR_InfoMap_GetSize(RERR_InfoMapPtr map) {
    if (!map) {
        return 0;
    }
    if (RERR_InfoMap_IsOutOfMemory(map)) {
        return 0;
    }

    return RERR_DynArray_GetSize(map->items);
}

bool RERR_InfoMap_IsEmpty(RERR_InfoMapPtr map) {
    if (!map) {
        return true;
    }
    if (RERR_InfoMap_IsOutOfMemory(map)) {
        return true;
    }

    return RERR_DynArray_IsEmpty(map->items);
}

void RERR_InfoMap_ReserveCapacity(RERR_InfoMapPtr map, size_t capacity) {
    if (!map || map->flags & FLAG_IMMUTABLE) {
        return;
    }
    if (RERR_InfoMap_IsOutOfMemory(map)) {
        return;
    }

    RERR_DynArray_ReserveCapacity(map->items, capacity);
}

void RERR_InfoMap_SetString(RERR_InfoMapPtr map, const char *key,
                            const char *value) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }
    if (!key) {
        map->flags |= FLAG_ERROR_NULL_KEY_GIVEN;
        return;
    }
    if (!value) {
        map->flags |= FLAG_ERROR_NULL_VALUE_GIVEN;
        return;
    }
    if (map->flags & FLAG_IMMUTABLE) {
        map->flags |= FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE;
        return;
    }
    if (map->flags & FLAG_OUT_OF_MEMORY) {
        return;
    }

    size_t strLen = strlen(value);
    char *strCopy = malloc(strLen + 1);
    if (!strCopy) {
        SwitchToOutOfMemory(map);
        goto exit;
    }
    strncpy(strCopy, value, strLen + 1);

    RERR_InfoMapIterator it;
    bool ok = SetKey(map, key, &it);
    if (!ok) {
        goto exit;
    }

    it->value.type = RERR_InfoValueTypeString;
    it->value.value.string = strCopy;
    strCopy = NULL;

exit:
    free(strCopy);
}

void RERR_InfoMap_SetBool(RERR_InfoMapPtr map, const char *key, bool value) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }
    if (!key) {
        map->flags |= FLAG_ERROR_NULL_KEY_GIVEN;
        return;
    }
    if (map->flags & FLAG_IMMUTABLE) {
        map->flags |= FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE;
        return;
    }
    if (map->flags & FLAG_OUT_OF_MEMORY) {
        return;
    }

    RERR_InfoMapIterator it;
    bool ok = SetKey(map, key, &it);
    if (!ok) {
        return;
    }

    it->value.type = RERR_InfoValueTypeBool;
    it->value.value.boolean = value;
}

void RERR_InfoMap_SetI64(RERR_InfoMapPtr map, const char *key, int64_t value) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }
    if (!key) {
        map->flags |= FLAG_ERROR_NULL_KEY_GIVEN;
        return;
    }
    if (map->flags & FLAG_IMMUTABLE) {
        map->flags |= FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE;
        return;
    }
    if (map->flags & FLAG_OUT_OF_MEMORY) {
        return;
    }

    RERR_InfoMapIterator it;
    bool ok = SetKey(map, key, &it);
    if (!ok) {
        return;
    }

    it->value.type = RERR_InfoValueTypeI64;
    it->value.value.i64 = value;
}

void RERR_InfoMap_SetU64(RERR_InfoMapPtr map, const char *key,
                         uint64_t value) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }
    if (!key) {
        map->flags |= FLAG_ERROR_NULL_KEY_GIVEN;
        return;
    }
    if (map->flags & FLAG_IMMUTABLE) {
        map->flags |= FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE;
        return;
    }
    if (map->flags & FLAG_OUT_OF_MEMORY) {
        return;
    }

    RERR_InfoMapIterator it;
    bool ok = SetKey(map, key, &it);
    if (!ok) {
        return;
    }

    it->value.type = RERR_InfoValueTypeU64;
    it->value.value.u64 = value;
}

void RERR_InfoMap_SetF64(RERR_InfoMapPtr map, const char *key, double value) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }
    if (!key) {
        map->flags |= FLAG_ERROR_NULL_KEY_GIVEN;
        return;
    }
    if (map->flags & FLAG_IMMUTABLE) {
        map->flags |= FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE;
        return;
    }
    if (map->flags & FLAG_OUT_OF_MEMORY) {
        return;
    }

    RERR_InfoMapIterator it;
    bool ok = SetKey(map, key, &it);
    if (!ok) {
        return;
    }

    it->value.type = RERR_InfoValueTypeF64;
    it->value.value.f64 = value;
}

void RERR_InfoMap_Remove(RERR_InfoMapPtr map, const char *key) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }
    if (!key) {
        map->flags |= FLAG_ERROR_NULL_KEY_GIVEN;
        return;
    }
    if (map->flags & FLAG_IMMUTABLE) {
        map->flags |= FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE;
        return;
    }
    if (map->flags & FLAG_OUT_OF_MEMORY) {
        return;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return;
    }

    ClearItem(found);
    RERR_DynArray_Erase(map->items, found);
}

void RERR_InfoMap_Clear(RERR_InfoMapPtr map) {
    if (!map) {
        return;
    }
    if (map == INFOMAP_OUT_OF_MEMORY) {
        return;
    }
    if (map->flags & FLAG_IMMUTABLE) {
        map->flags |= FLAG_ERROR_ATTEMPT_TO_MUTATE_IMMUTABLE;
        return;
    }
    if (map->flags & FLAG_OUT_OF_MEMORY) {
        return;
    }

    Clear(map);
}

bool RERR_InfoMap_HasKey(RERR_InfoMapPtr map, const char *key) {
    if (!map || !key || RERR_InfoMap_IsOutOfMemory(map)) {
        return false;
    }

    RERR_InfoMapIterator found = Find(map, key);
    return found != NULL;
}

RERR_InfoValueType RERR_InfoMap_GetType(RERR_InfoMapPtr map, const char *key) {
    if (!map || !key || RERR_InfoMap_IsOutOfMemory(map)) {
        return RERR_InfoValueTypeInvalid;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return RERR_InfoValueTypeInvalid;
    }

    return found->value.type;
}

bool RERR_InfoMap_GetString(RERR_InfoMapPtr map, const char *key,
                            const char **value) {
    if (!value) {
        return false;
    }
    *value = NULL;

    if (!map || !key || RERR_InfoMap_IsOutOfMemory(map)) {
        return false;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found || found->value.type != RERR_InfoValueTypeString) {
        return false;
    }

    *value = found->value.value.string;
    return true;
}

bool RERR_InfoMap_GetBool(RERR_InfoMapPtr map, const char *key, bool *value) {
    if (!value) {
        return false;
    }
    *value = false; // Not promised in API, but be deterministic

    if (!map || !key || RERR_InfoMap_IsOutOfMemory(map)) {
        return false;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found || found->value.type != RERR_InfoValueTypeBool) {
        return false;
    }

    *value = found->value.value.boolean;
    return true;
}

bool RERR_InfoMap_GetI64(RERR_InfoMapPtr map, const char *key,
                         int64_t *value) {
    if (!value) {
        return false;
    }
    *value = 0; // Not promised in API, but be deterministic

    if (!map || !key || RERR_InfoMap_IsOutOfMemory(map)) {
        return false;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found || found->value.type != RERR_InfoValueTypeI64) {
        return false;
    }

    *value = found->value.value.i64;
    return true;
}

bool RERR_InfoMap_GetU64(RERR_InfoMapPtr map, const char *key,
                         uint64_t *value) {
    if (!value) {
        return false;
    }
    *value = 0; // Not promised in API, but be deterministic

    if (!map || !key || RERR_InfoMap_IsOutOfMemory(map)) {
        return false;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found || found->value.type != RERR_InfoValueTypeU64) {
        return false;
    }

    *value = found->value.value.u64;
    return true;
}

bool RERR_InfoMap_GetF64(RERR_InfoMapPtr map, const char *key, double *value) {
    if (!value) {
        return false;
    }
    *value = nan(""); // Not promised in API, but be deterministic

    if (!map || !key || RERR_InfoMap_IsOutOfMemory(map)) {
        return false;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found || found->value.type != RERR_InfoValueTypeF64) {
        return false;
    }

    *value = found->value.value.f64;
    return true;
}

RERR_InfoMapIterator RERR_InfoMap_Begin(RERR_InfoMapPtr map) {
    if (!map || RERR_InfoMap_IsOutOfMemory(map)) {
        // begin and end must be equal even if map is "empty"
        return NULL;
    }
    return RERR_DynArray_Begin(map->items);
}

RERR_InfoMapIterator RERR_InfoMap_End(RERR_InfoMapPtr map) {
    if (!map || RERR_InfoMap_IsOutOfMemory(map)) {
        // begin and end must be equal even if map is "empty"
        return NULL;
    }
    return RERR_DynArray_End(map->items);
}

RERR_InfoMapIterator RERR_InfoMap_Advance(RERR_InfoMapPtr map,
                                          RERR_InfoMapIterator it) {
    assert(it && it != RERR_InfoMap_End(map));

    // In the current array-based implementation we could advance the iterator
    // without having a reference to the whole map, but we demand such a
    // reference in case we change the map storage implementation in the
    // future.
    return RERR_DynArray_Advance(map->items, it);
}

const char *RERR_InfoMapIterator_GetKey(RERR_InfoMapIterator it) {
    assert(it);
    if (!it) {
        return NULL;
    }
    return it->key;
}

RERR_InfoValueType RERR_InfoMapIterator_GetType(RERR_InfoMapIterator it) {
    assert(it);
    if (!it) {
        return RERR_InfoValueTypeInvalid;
    }
    return it->value.type;
}

const char *RERR_InfoMapIterator_GetString(RERR_InfoMapIterator it) {
    if (!it || it->value.type != RERR_InfoValueTypeString) {
        assert(false);
        return NULL;
    }
    return it->value.value.string;
}

bool RERR_InfoMapIterator_GetBool(RERR_InfoMapIterator it) {
    if (!it || it->value.type != RERR_InfoValueTypeBool) {
        assert(false);
        return false;
    }
    return it->value.value.boolean;
}

int64_t RERR_InfoMapIterator_GetI64(RERR_InfoMapIterator it) {
    if (!it || it->value.type != RERR_InfoValueTypeI64) {
        assert(false);
        return 0;
    }
    return it->value.value.i64;
}

uint64_t RERR_InfoMapIterator_GetU64(RERR_InfoMapIterator it) {
    if (!it || it->value.type != RERR_InfoValueTypeU64) {
        assert(false);
        return 0;
    }
    return it->value.value.u64;
}

double RERR_InfoMapIterator_GetF64(RERR_InfoMapIterator it) {
    if (!it || it->value.type != RERR_InfoValueTypeF64) {
        assert(false);
        return nan("");
    }
    return it->value.value.f64;
}
