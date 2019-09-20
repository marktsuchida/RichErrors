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
#include "SmallMap.h"

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
 * implementation and of code using SmallMap).
 *
 * An alternative possibility would be to use separate arrays for the keys and
 * values (and possibly for the value type and actual value). In this case it
 * would be preferable to allocate a single block containing all of the arrays,
 * to minimize the number of allocations and reallocations. However, in the
 * absence of comparative benchmarking, I am choosing the simpler
 * implementation.
 */


struct Value {
    SmallMapValueType type;
    union {
        const char* string; // Uniquely owned
        bool boolean;
        int64_t i64;
        uint64_t u64;
        double f64;
    } value;
};


struct SmallMapItem {
    const char* key; // Uniquely owned
    struct Value value;
};


struct SmallMap {
    DynArrayPtr items; // Sorted by key strcmp
    bool frozen;
    size_t refCount; // Always 1 unless frozen
};


static inline void FreeConst(const void* m)
{
    free((void*)m);
}


// Precondition: it != NULL
static inline void ClearItem(SmallMapIterator it)
{
    FreeConst(it->key);

    switch (it->value.type) {
        case SmallMapValueTypeString:
            FreeConst(it->value.value.string);
            break;
        default:
            break;
    }
}


// Precondition: src != NULL
// Precondition: dst != NULL
// Precondition: src != dst
static bool DeepCopyItem(struct SmallMapItem* restrict dst, struct SmallMapItem* restrict src)
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
        case SmallMapValueTypeString:
            dst->value.type = SmallMapValueTypeString;
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
static void Clear(SmallMapPtr map)
{
    SmallMapIterator begin = DynArray_Begin(map->items);
    SmallMapIterator end = DynArray_End(map->items);
    for (SmallMapIterator it = begin; it != end; ++it) {
        ClearItem(it);
    }
    DynArray_Clear(map->items);
}


// Precondition: map != NULL
static SmallMapPtr MutableCopy(SmallMapPtr source)
{
    SmallMapPtr ret = SmallMap_Create();
    if (!ret) {
        goto error;
    }

    SmallMapIterator begin = DynArray_Begin(source->items);
    SmallMapIterator end = DynArray_End(source->items);
    for (SmallMapIterator it = begin; it != end; ++it) {
        SmallMapIterator ins = DynArray_Insert(ret->items, DynArray_End(ret->items));
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
    SmallMap_Destroy(ret);
    return NULL;
}


static int ItemKeyCompare(const struct SmallMapItem* item, const char* key)
{
    return strcmp(item->key, key);
}


static inline SmallMapIterator Find(SmallMapPtr map, const char* key)
{
    return DynArray_BSearchExact(map->items, key, ItemKeyCompare);
}


// Ensures capacity and finds insertion point (or NULL on error)
// Precondition: map != NULL
// Precondition: key != NULL
// Postcondition: *it == NULL || (*it)->key contains copy of key
// Postcondition: (*it)->value is invalid
// Returns SmallMapErrorOutOfMemory if allocation of capacity or key copy failed
// Returns SmallMapErrorKeyExists if unique && key exists
static SmallMapError SetKey(SmallMapPtr map, const char* key, bool unique, SmallMapIterator* it)
{
    *it = DynArray_BSearchInsertionPoint(map->items, key, ItemKeyCompare);

    // Found exact, so need to overwrite
    if (*it != DynArray_End(map->items) && strcmp((*it)->key, key) == 0) {
        if (unique) {
            return SmallMapErrorKeyExists;
        }

        // Overwriting value in place.
        // Evacuate key while we clear the item.
        const char* saveKey = (*it)->key;
        (*it)->key = NULL;
        ClearItem(*it);
        (*it)->key = saveKey;
        return SmallMapNoError;
    }

    SmallMapError ret = SmallMapNoError;

    size_t keyLen = strlen(key);
    char* keyCopy = malloc(keyLen + 1);
    if (!keyCopy) {
        ret = SmallMapErrorOutOfMemory;
        goto exit;
    }
    strncpy(keyCopy, key, keyLen + 1);

    *it = DynArray_Insert(map->items, *it);
    if (!*it) {
        ret = SmallMapErrorOutOfMemory;
        goto exit;
    }

    (*it)->key = keyCopy;
    keyCopy = NULL;

exit:
    free(keyCopy);
    return ret;
}


SmallMapPtr SmallMap_Create(void)
{
    SmallMapPtr ret = calloc(1, sizeof(struct SmallMap));
    if (!ret) {
        return NULL;
    }
    ret->refCount = 1;

    ret->items = DynArray_Create(sizeof(struct SmallMapItem));
    if (!ret->items) {
        free(ret);
        return NULL;
    }

    return ret;
}


void SmallMap_Destroy(SmallMapPtr map)
{
    if (--map->refCount > 0) {
        return;
    }

    // TODO Clear() may realloc to shrink, which is a waste
    Clear(map);
    DynArray_Destroy(map->items);
    free(map);
}


SmallMapPtr SmallMap_Copy(SmallMapPtr map)
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


SmallMapPtr SmallMap_UnfrozenCopy(SmallMapPtr map)
{
    if (!map) {
        return NULL;
    }
    return MutableCopy(map);
}


void SmallMap_Freeze(SmallMapPtr map)
{
    if (!map) {
        return;
    }
    map->frozen = true;
}


bool SmallMap_IsFrozen(SmallMapPtr map)
{
    if (!map) {
        return false;
    }
    return map->frozen;
}


size_t SmallMap_GetSize(SmallMapPtr map)
{
    if (!map) {
        return 0;
    }
    return DynArray_Size(map->items);
}


void SmallMap_ReserveCapacity(SmallMapPtr map, size_t capacity)
{
    if (!map) {
        return;
    }

    DynArray_ReserveCapacity(map->items, capacity);
}


static SmallMapError SetString(SmallMapPtr map, const char* key, const char* value, bool unique)
{
    if (!map || !key || !value) {
        return SmallMapErrorNullArg;
    }

    SmallMapError ret = SmallMapNoError;

    size_t strLen = strlen(value);
    char* strCopy = malloc(strLen + 1);
    if (!strCopy) {
        ret = SmallMapErrorOutOfMemory;
        goto exit;
    }
    strncpy(strCopy, value, strLen + 1);

    SmallMapIterator it;
    ret = SetKey(map, key, unique, &it);
    if (ret) {
        goto exit;
    }

    it->value.type = SmallMapValueTypeString;
    it->value.value.string = strCopy;
    strCopy = NULL;

exit:
    free(strCopy);
    return ret;
}


SmallMapError SmallMap_SetString(SmallMapPtr map, const char* key, const char* value)
{
    return SetString(map, key, value, false);
}


SmallMapError SmallMap_SetUniqueString(SmallMapPtr map, const char* key, const char* value)
{
    return SetString(map, key, value, true);
}


static SmallMapError SetBool(SmallMapPtr map, const char* key, bool value, bool unique)
{
    if (!map || !key) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator it;
    SmallMapError ret = SetKey(map, key, unique, &it);
    if (ret) {
        return ret;
    }

    it->value.type = SmallMapValueTypeBool;
    it->value.value.boolean = value;
    return SmallMapNoError;
}


SmallMapError SmallMap_SetBool(SmallMapPtr map, const char* key, bool value)
{
    return SetBool(map, key, value, false);
}


SmallMapError SmallMap_SetUniqueBool(SmallMapPtr map, const char* key, bool value)
{
    return SetBool(map, key, value, true);
}


static SmallMapError SetI64(SmallMapPtr map, const char* key, int64_t value, bool unique)
{
    if (!map || !key) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator it;
    SmallMapError ret = SetKey(map, key, unique, &it);
    if (ret) {
        return ret;
    }

    it->value.type = SmallMapValueTypeI64;
    it->value.value.i64 = value;
    return SmallMapNoError;
}


SmallMapError SmallMap_SetI64(SmallMapPtr map, const char* key, int64_t value)
{
    return SetI64(map, key, value, false);
}


SmallMapError SmallMap_SetUniqueI64(SmallMapPtr map, const char* key, int64_t value)
{
    return SetI64(map, key, value, true);
}


static SmallMapError SetU64(SmallMapPtr map, const char* key, uint64_t value, bool unique)
{
    if (!map || !key) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator it;
    SmallMapError ret = SetKey(map, key, unique, &it);
    if (ret) {
        return ret;
    }

    it->value.type = SmallMapValueTypeU64;
    it->value.value.u64 = value;
    return SmallMapNoError;
}


SmallMapError SmallMap_SetU64(SmallMapPtr map, const char* key, uint64_t value)
{
    return SetU64(map, key, value, false);
}


SmallMapError SmallMap_SetUniqueU64(SmallMapPtr map, const char* key, uint64_t value)
{
    return SetU64(map, key, value, true);
}


static SmallMapError SetF64(SmallMapPtr map, const char* key, double value, bool unique)
{
    if (!map || !key) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator it;
    SmallMapError ret = SetKey(map, key, unique, &it);
    if (ret) {
        return ret;
    }

    it->value.type = SmallMapValueTypeF64;
    it->value.value.f64 = value;
    return SmallMapNoError;
}


SmallMapError SmallMap_SetF64(SmallMapPtr map, const char* key, double value)
{
    return SetF64(map, key, value, false);
}


SmallMapError SmallMap_SetUniqueF64(SmallMapPtr map, const char* key, double value)
{
    return SetF64(map, key, value, true);
}


bool SmallMap_Remove(SmallMapPtr map, const char* key)
{
    if (!map || !key) {
        return false;
    }

    SmallMapIterator found = Find(map, key);
    if (!found) {
        return false;
    }

    ClearItem(found);
    DynArray_Erase(map->items, found);

    return true;
}


void SmallMap_Clear(SmallMapPtr map)
{
    if (!map) {
        return;
    }

    Clear(map);
}


bool SmallMap_HasKey(SmallMapPtr map, const char* key)
{
    if (!map || !key) {
        return false;
    }

    SmallMapIterator found = Find(map, key);
    return found != NULL;
}


SmallMapError SmallMap_GetType(SmallMapPtr map, const char* key, SmallMapValueType* type)
{
    if (!map || !key || !type) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator found = Find(map, key);
    if (!found) {
        return SmallMapErrorKeyNotFound;
    }

    *type = found->value.type;
    return SmallMapNoError;
}


size_t SmallMap_GetStringSize(SmallMapPtr map, const char* key)
{
    if (!map || !key) {
        return 0;
    }

    SmallMapIterator found = Find(map, key);
    if (!found || found->value.type != SmallMapValueTypeString) {
        return 0;
    }

    return strlen(found->value.value.string);
}


static SmallMapError GetString(SmallMapPtr map, const char* key, char* dest, size_t destSize, bool remove, bool allowPartial)
{
    if (!map || !key || !dest) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator found = Find(map, key);
    if (!found) {
        return SmallMapErrorKeyNotFound;
    }
    if (found->value.type != SmallMapValueTypeString) {
        return SmallMapErrorWrongType;
    }

    SmallMapError ret = SmallMapNoError;

    size_t len = strlen(found->value.value.string);
    if (destSize < len + 1) {
        ret = SmallMapErrorDestSizeTooSmall;
        if (!allowPartial) {
            return ret;
        }
    }
    strncpy(dest, found->value.value.string, destSize);
    dest[destSize - 1] = '\0'; // Terminate if truncated

    if (remove) {
        ClearItem(found);
        DynArray_Erase(map->items, found);
    }

    return ret;
}


SmallMapError SmallMap_GetString(SmallMapPtr map, const char* key, char* dest, size_t destSize)
{
    return GetString(map, key, dest, destSize, false, false);
}


SmallMapError SmallMap_GetTruncatedString(SmallMapPtr map, const char* key, char* dest, size_t destSize)
{
    return GetString(map, key, dest, destSize, false, true);
}

SmallMapError SmallMap_PopString(SmallMapPtr map, const char* key, char* dest, size_t destSize)
{
    return GetString(map, key, dest, destSize, true, false);
}


static SmallMapError GetBool(SmallMapPtr map, const char* key, bool* value, bool remove)
{
    if (!map || !key) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator found = Find(map, key);
    if (!found) {
        return SmallMapErrorKeyNotFound;
    }
    if (found->value.type != SmallMapValueTypeBool) {
        return SmallMapErrorWrongType;
    }

    *value = found->value.value.boolean;

    if (remove) {
        ClearItem(found);
        DynArray_Erase(map->items, found);
    }

    return SmallMapNoError;
}


SmallMapError SmallMap_GetBool(SmallMapPtr map, const char* key, bool* value)
{
    return GetBool(map, key, value, false);
}


SmallMapError SmallMap_PopBool(SmallMapPtr map, const char* key, bool* value)
{
    return GetBool(map, key, value, true);
}


static SmallMapError GetI64(SmallMapPtr map, const char* key, int64_t* value, bool remove)
{
    if (!map || !key) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator found = Find(map, key);
    if (!found) {
        return SmallMapErrorKeyNotFound;
    }
    if (found->value.type != SmallMapValueTypeI64) {
        return SmallMapErrorWrongType;
    }

    *value = found->value.value.i64;

    if (remove) {
        ClearItem(found);
        DynArray_Erase(map->items, found);
    }

    return SmallMapNoError;
}


SmallMapError SmallMap_GetI64(SmallMapPtr map, const char* key, int64_t* value)
{
    return GetI64(map, key, value, false);
}


SmallMapError SmallMap_PopI64(SmallMapPtr map, const char* key, int64_t* value)
{
    return GetI64(map, key, value, true);
}


static SmallMapError GetU64(SmallMapPtr map, const char* key, uint64_t* value, bool remove)
{
    if (!map || !key) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator found = Find(map, key);
    if (!found) {
        return SmallMapErrorKeyNotFound;
    }
    if (found->value.type != SmallMapValueTypeU64) {
        return SmallMapErrorWrongType;
    }

    *value = found->value.value.u64;

    if (remove) {
        ClearItem(found);
        DynArray_Erase(map->items, found);
    }

    return SmallMapNoError;
}


SmallMapError SmallMap_GetU64(SmallMapPtr map, const char* key, uint64_t* value)
{
    return GetU64(map, key, value, false);
}


SmallMapError SmallMap_PopU64(SmallMapPtr map, const char* key, uint64_t* value)
{
    return GetU64(map, key, value, true);
}


static SmallMapError GetF64(SmallMapPtr map, const char* key, double* value, bool remove)
{
    if (!map || !key) {
        return SmallMapErrorNullArg;
    }

    SmallMapIterator found = Find(map, key);
    if (!found) {
        return SmallMapErrorKeyNotFound;
    }
    if (found->value.type != SmallMapValueTypeF64) {
        return SmallMapErrorWrongType;
    }

    *value = found->value.value.f64;

    if (remove) {
        ClearItem(found);
        DynArray_Erase(map->items, found);
    }

    return SmallMapNoError;
}


SmallMapError SmallMap_GetF64(SmallMapPtr map, const char* key, double* value)
{
    return GetF64(map, key, value, false);
}


SmallMapError SmallMap_PopF64(SmallMapPtr map, const char* key, double* value)
{
    return GetF64(map, key, value, true);
}
