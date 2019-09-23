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
#include "InfoMap.h"

#include "DynArray.h"

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
        const char* string; // Uniquely owned
        bool boolean;
        int64_t i64;
        uint64_t u64;
        double f64;
    } value;
};


struct RERR_InfoMapItem {
    const char* key; // Uniquely owned
    struct Value value;
};


struct RERR_InfoMap {
    RERR_DynArrayPtr items; // Sorted by key strcmp
    bool frozen;
    size_t refCount; // Always 1 unless frozen
};


static inline void FreeConst(const void* m)
{
    free((void*)m);
}


// Precondition: it != NULL
static inline void ClearItem(RERR_InfoMapIterator it)
{
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
static bool DeepCopyItem(struct RERR_InfoMapItem* restrict dst, struct RERR_InfoMapItem* restrict src)
{
    char* keyCopy = NULL;
    char* strCopy = NULL;

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
static void Clear(RERR_InfoMapPtr map)
{
    RERR_InfoMapIterator begin = RERR_DynArray_Begin(map->items);
    RERR_InfoMapIterator end = RERR_DynArray_End(map->items);
    for (RERR_InfoMapIterator it = begin; it != end; ++it) {
        ClearItem(it);
    }
    RERR_DynArray_Clear(map->items);
}


// Precondition: map != NULL
static RERR_InfoMapPtr MutableCopy(RERR_InfoMapPtr source)
{
    RERR_InfoMapPtr ret = RERR_InfoMap_Create();
    if (!ret) {
        goto error;
    }

    RERR_InfoMapIterator begin = RERR_DynArray_Begin(source->items);
    RERR_InfoMapIterator end = RERR_DynArray_End(source->items);
    for (RERR_InfoMapIterator it = begin; it != end; ++it) {
        RERR_InfoMapIterator ins = RERR_DynArray_Insert(ret->items, RERR_DynArray_End(ret->items));
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


static int ItemKeyCompare(const struct RERR_InfoMapItem* item, const char* key)
{
    return strcmp(item->key, key);
}


static inline RERR_InfoMapIterator Find(RERR_InfoMapPtr map, const char* key)
{
    return RERR_DynArray_BSearch(map->items, key, ItemKeyCompare);
}


// Ensures capacity and finds insertion point (or NULL on error)
// Precondition: map != NULL
// Precondition: key != NULL
// Postcondition: *it == NULL || (*it)->key contains copy of key
// Postcondition: (*it)->value is invalid
// Returns RERR_InfoMapErrorOutOfMemory if allocation of capacity or key copy failed
static RERR_InfoMapError SetKey(RERR_InfoMapPtr map, const char* key, RERR_InfoMapIterator* it)
{
    *it = RERR_DynArray_BSearchInsertionPoint(map->items, key, ItemKeyCompare);

    // Found exact, so need to overwrite
    if (*it != RERR_DynArray_End(map->items) && strcmp((*it)->key, key) == 0) {
        // Overwriting value in place.
        // Evacuate key while we clear the item.
        const char* saveKey = (*it)->key;
        (*it)->key = NULL;
        ClearItem(*it);
        (*it)->key = saveKey;
        return RERR_InfoMapNoError;
    }

    RERR_InfoMapError ret = RERR_InfoMapNoError;

    size_t keyLen = strlen(key);
    char* keyCopy = malloc(keyLen + 1);
    if (!keyCopy) {
        ret = RERR_InfoMapErrorOutOfMemory;
        goto exit;
    }
    strncpy(keyCopy, key, keyLen + 1);

    *it = RERR_DynArray_Insert(map->items, *it);
    if (!*it) {
        ret = RERR_InfoMapErrorOutOfMemory;
        goto exit;
    }

    (*it)->key = keyCopy;
    keyCopy = NULL;

exit:
    free(keyCopy);
    return ret;
}


RERR_InfoMapPtr RERR_InfoMap_Create(void)
{
    RERR_InfoMapPtr ret = calloc(1, sizeof(struct RERR_InfoMap));
    if (!ret) {
        return NULL;
    }
    ret->refCount = 1;

    ret->items = RERR_DynArray_Create(sizeof(struct RERR_InfoMapItem));
    if (!ret->items) {
        free(ret);
        return NULL;
    }

    return ret;
}


void RERR_InfoMap_Destroy(RERR_InfoMapPtr map)
{
    if (!map) {
        return;
    }

    if (--map->refCount > 0) {
        return;
    }

    // TODO Clear() may realloc to shrink, which is a waste
    Clear(map);
    RERR_DynArray_Destroy(map->items);
    free(map);
}


RERR_InfoMapPtr RERR_InfoMap_Copy(RERR_InfoMapPtr map)
{
    if (!map) {
        return NULL;
    }

    if (map->frozen) {
        ++map->refCount;
        return map;
    }

    return MutableCopy(map);
}


RERR_InfoMapPtr RERR_InfoMap_MutableCopy(RERR_InfoMapPtr map)
{
    if (!map) {
        return NULL;
    }
    return MutableCopy(map);
}


RERR_InfoMapPtr RERR_InfoMap_ImmutableCopy(RERR_InfoMapPtr map)
{
    RERR_InfoMapPtr ret = RERR_InfoMap_Copy(map);
    RERR_InfoMap_MakeImmutable(ret);
    return ret;
}


void RERR_InfoMap_MakeImmutable(RERR_InfoMapPtr map)
{
    if (!map) {
        return;
    }
    map->frozen = true;
    // TODO Shrink if there is excess capacity
}


bool RERR_InfoMap_IsImmutable(RERR_InfoMapPtr map)
{
    if (!map) {
        return true;
    }
    return map->frozen;
}


size_t RERR_InfoMap_GetSize(RERR_InfoMapPtr map)
{
    if (!map) {
        return 0;
    }
    return RERR_DynArray_GetSize(map->items);
}


bool RERR_InfoMap_IsEmpty(RERR_InfoMapPtr map)
{
    if (!map) {
        return true;
    }
    return RERR_DynArray_IsEmpty(map->items);
}


void RERR_InfoMap_ReserveCapacity(RERR_InfoMapPtr map, size_t capacity)
{
    if (!map || map->frozen) {
        return;
    }

    RERR_DynArray_ReserveCapacity(map->items, capacity);
}


RERR_InfoMapError RERR_InfoMap_SetString(RERR_InfoMapPtr map, const char* key, const char* value)
{
    if (!map || !key || !value) {
        return RERR_InfoMapErrorNullArg;
    }
    if (map->frozen) {
        return RERR_InfoMapErrorMapImmutable;
    }

    RERR_InfoMapError ret = RERR_InfoMapNoError;

    size_t strLen = strlen(value);
    char* strCopy = malloc(strLen + 1);
    if (!strCopy) {
        ret = RERR_InfoMapErrorOutOfMemory;
        goto exit;
    }
    strncpy(strCopy, value, strLen + 1);

    RERR_InfoMapIterator it;
    ret = SetKey(map, key, &it);
    if (ret) {
        goto exit;
    }

    it->value.type = RERR_InfoValueTypeString;
    it->value.value.string = strCopy;
    strCopy = NULL;

exit:
    free(strCopy);
    return ret;
}


RERR_InfoMapError RERR_InfoMap_SetBool(RERR_InfoMapPtr map, const char* key, bool value)
{
    if (!map || !key) {
        return RERR_InfoMapErrorNullArg;
    }
    if (map->frozen) {
        return RERR_InfoMapErrorMapImmutable;
    }

    RERR_InfoMapIterator it;
    RERR_InfoMapError ret = SetKey(map, key, &it);
    if (ret) {
        return ret;
    }

    it->value.type = RERR_InfoValueTypeBool;
    it->value.value.boolean = value;
    return RERR_InfoMapNoError;
}


RERR_InfoMapError RERR_InfoMap_SetI64(RERR_InfoMapPtr map, const char* key, int64_t value)
{
    if (!map || !key) {
        return RERR_InfoMapErrorNullArg;
    }
    if (map->frozen) {
        return RERR_InfoMapErrorMapImmutable;
    }

    RERR_InfoMapIterator it;
    RERR_InfoMapError ret = SetKey(map, key, &it);
    if (ret) {
        return ret;
    }

    it->value.type = RERR_InfoValueTypeI64;
    it->value.value.i64 = value;
    return RERR_InfoMapNoError;
}


RERR_InfoMapError RERR_InfoMap_SetU64(RERR_InfoMapPtr map, const char* key, uint64_t value)
{
    if (!map || !key) {
        return RERR_InfoMapErrorNullArg;
    }
    if (map->frozen) {
        return RERR_InfoMapErrorMapImmutable;
    }

    RERR_InfoMapIterator it;
    RERR_InfoMapError ret = SetKey(map, key, &it);
    if (ret) {
        return ret;
    }

    it->value.type = RERR_InfoValueTypeU64;
    it->value.value.u64 = value;
    return RERR_InfoMapNoError;
}


RERR_InfoMapError RERR_InfoMap_SetF64(RERR_InfoMapPtr map, const char* key, double value)
{
    if (!map || !key) {
        return RERR_InfoMapErrorNullArg;
    }
    if (map->frozen) {
        return RERR_InfoMapErrorMapImmutable;
    }

    RERR_InfoMapIterator it;
    RERR_InfoMapError ret = SetKey(map, key, &it);
    if (ret) {
        return ret;
    }

    it->value.type = RERR_InfoValueTypeF64;
    it->value.value.f64 = value;
    return RERR_InfoMapNoError;
}


bool RERR_InfoMap_Remove(RERR_InfoMapPtr map, const char* key)
{
    if (!map || map->frozen || !key) {
        return false;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return false;
    }

    ClearItem(found);
    RERR_DynArray_Erase(map->items, found);

    return true;
}


void RERR_InfoMap_Clear(RERR_InfoMapPtr map)
{
    if (!map || map->frozen) {
        return;
    }

    Clear(map);
}


bool RERR_InfoMap_HasKey(RERR_InfoMapPtr map, const char* key)
{
    if (!map || !key) {
        return false;
    }

    RERR_InfoMapIterator found = Find(map, key);
    return found != NULL;
}


RERR_InfoMapError RERR_InfoMap_GetType(RERR_InfoMapPtr map, const char* key, RERR_InfoValueType* type)
{
    if (!map || !key || !type) {
        return RERR_InfoMapErrorNullArg;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return RERR_InfoMapErrorKeyNotFound;
    }

    *type = found->value.type;
    return RERR_InfoMapNoError;
}


RERR_InfoMapError RERR_InfoMap_GetString(RERR_InfoMapPtr map, const char* key, const char** value)
{
    if (!map || !key || !value) {
        return RERR_InfoMapErrorNullArg;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return RERR_InfoMapErrorKeyNotFound;
    }
    if (found->value.type != RERR_InfoValueTypeString) {
        return RERR_InfoMapErrorWrongType;
    }

    *value = found->value.value.string;

    return RERR_InfoMapNoError;
}


RERR_InfoMapError RERR_InfoMap_GetBool(RERR_InfoMapPtr map, const char* key, bool* value)
{
    if (!map || !key) {
        return RERR_InfoMapErrorNullArg;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return RERR_InfoMapErrorKeyNotFound;
    }
    if (found->value.type != RERR_InfoValueTypeBool) {
        return RERR_InfoMapErrorWrongType;
    }

    *value = found->value.value.boolean;

    return RERR_InfoMapNoError;
}


RERR_InfoMapError RERR_InfoMap_GetI64(RERR_InfoMapPtr map, const char* key, int64_t* value)
{
    if (!map || !key) {
        return RERR_InfoMapErrorNullArg;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return RERR_InfoMapErrorKeyNotFound;
    }
    if (found->value.type != RERR_InfoValueTypeI64) {
        return RERR_InfoMapErrorWrongType;
    }

    *value = found->value.value.i64;

    return RERR_InfoMapNoError;
}


RERR_InfoMapError RERR_InfoMap_GetU64(RERR_InfoMapPtr map, const char* key, uint64_t* value)
{
    if (!map || !key) {
        return RERR_InfoMapErrorNullArg;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return RERR_InfoMapErrorKeyNotFound;
    }
    if (found->value.type != RERR_InfoValueTypeU64) {
        return RERR_InfoMapErrorWrongType;
    }

    *value = found->value.value.u64;

    return RERR_InfoMapNoError;
}


RERR_InfoMapError RERR_InfoMap_GetF64(RERR_InfoMapPtr map, const char* key, double* value)
{
    if (!map || !key) {
        return RERR_InfoMapErrorNullArg;
    }

    RERR_InfoMapIterator found = Find(map, key);
    if (!found) {
        return RERR_InfoMapErrorKeyNotFound;
    }
    if (found->value.type != RERR_InfoValueTypeF64) {
        return RERR_InfoMapErrorWrongType;
    }

    *value = found->value.value.f64;

    return RERR_InfoMapNoError;
}
