// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

/** \file
 * \brief Header-only C++ interface for error info maps.
 */

#ifndef __cplusplus
#error This header is for C++ only.
#endif

#include "RichErrors/InfoMap.h"

#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace RERR {

/// Error information map.
class InfoMap final {
    RERR_InfoMapPtr ptr;

  public:
    ~InfoMap() { RERR_InfoMap_Destroy(ptr); }

    /// Copy construct.
    InfoMap(InfoMap const &rhs) noexcept : ptr{RERR_InfoMap_Copy(rhs.ptr)} {}

    /// Copy assign.
    InfoMap &operator=(InfoMap const &rhs) noexcept {
        RERR_InfoMap_Destroy(ptr);
        ptr = RERR_InfoMap_Copy(rhs.ptr);
        return *this;
    }

    /// Move construct.
    InfoMap(InfoMap &&rhs) noexcept {
        ptr = rhs.ptr;
        rhs.ptr = nullptr;
    }

    /// Move assign.
    InfoMap &operator=(InfoMap &&rhs) noexcept {
        RERR_InfoMap_Destroy(ptr);
        ptr = rhs.ptr;
        rhs.ptr = nullptr;
        return *this;
    }

    /// Construct empty map.
    InfoMap() noexcept : ptr{RERR_InfoMap_Create()} {}

    explicit InfoMap(RERR_InfoMapPtr const &) = delete;

    /// Construct from a C pointer.
    /**
     * For safety, the C pointer must be an rvalue, so that it is made
     * explicit in user code that the new object is taking ownership of the
     * pointer. Use `std::move()` if necessary.
     */
    explicit InfoMap(RERR_InfoMapPtr &&cInfoMap) noexcept : ptr{cInfoMap} {
        cInfoMap = nullptr;
    }

    /// Return the C pointer, relinquishing ownership.
    RERR_InfoMapPtr ReleaseCPtr() noexcept {
        auto ret = ptr;
        ptr = nullptr;
        return ret;
    }

    /// Make a mutable copy of this info map.
    InfoMap MutableCopy() const noexcept {
        return InfoMap(RERR_InfoMap_MutableCopy(ptr));
    }

    /// Make an immutable copy of this info map.
    InfoMap ImmutableCopy() const noexcept {
        return InfoMap(RERR_InfoMap_ImmutableCopy(ptr));
    }

    /// Forbid further modification of this info map.
    void MakeImmutable() noexcept { RERR_InfoMap_MakeImmutable(ptr); }

    /// Return whether this info map is mutable.
    bool IsMutable() const noexcept { return RERR_InfoMap_IsMutable(ptr); }

    /// Create an info map, simulating an allocation failure.
    static InfoMap OutOfMemory() noexcept {
        return InfoMap(RERR_InfoMap_CreateOutOfMemory());
    }

    /// Return whether this info map is in the out-of-memory state.
    bool IsOutOfMemory() const noexcept {
        return RERR_InfoMap_IsOutOfMemory(ptr);
    }

    /// Return whether this info map has seen incorrect construction.
    bool HasProgrammingErrors() const noexcept {
        return RERR_InfoMap_HasProgrammingErrors(ptr);
    }

    /// Get the error message for incorrect construction.
    std::string GetProgrammingErrors() const {
        size_t strSize = RERR_InfoMap_GetProgrammingErrors(ptr, nullptr, 0);

        auto cstr = std::make_unique<char[]>(strSize);
        if (!cstr) {
            return {};
        }
        return std::string(cstr.get());

        /* C++17:
        std::string ret;
        ret.resize(strSize - 1);
        RERR_InfoMap_GetProgrammingErrors(ptr, ret.data(), ret.size() + 1);
        return ret;
        */
    }

    /// Return the number of items in this info map.
    size_t GetSize() const noexcept { return RERR_InfoMap_GetSize(ptr); }

    /// Return whether this info map is empty.
    bool IsEmpty() const noexcept { return RERR_InfoMap_IsEmpty(ptr); }

    /// Pre-allocate space for the given number of items.
    void ReserveCapacity(size_t capacity) noexcept {
        RERR_InfoMap_ReserveCapacity(ptr, capacity);
    }

    /*
     * Note: The Set methods are not a single overloaded method Set()
     * because the type to use for a given key is part of an API contract
     * between the error producer and error consumer.
     */

    /// Add or replace a string value in this info map.
    void SetString(std::string const &key, std::string const &value) noexcept {
        RERR_InfoMap_SetString(ptr, key.c_str(), value.c_str());
    }

    /// Add or replace a boolean value in this info map.
    void SetBool(std::string const &key, bool value) noexcept {
        RERR_InfoMap_SetBool(ptr, key.c_str(), value);
    }

    /// Add or replace a signed integer value in this info map.
    void SetI64(std::string const &key, int64_t value) noexcept {
        RERR_InfoMap_SetI64(ptr, key.c_str(), value);
    }

    /// Add or replace an unsigned integer value in this info map.
    void SetU64(std::string const &key, uint64_t value) noexcept {
        RERR_InfoMap_SetU64(ptr, key.c_str(), value);
    }

    /// Add or replace a floating point value in this info map.
    void SetF64(std::string const &key, double value) noexcept {
        RERR_InfoMap_SetF64(ptr, key.c_str(), value);
    }

    /// Remove a key from this info map.
    void Remove(std::string const &key) noexcept {
        RERR_InfoMap_Remove(ptr, key.c_str());
    }

    /// Remove all items from this info map.
    void Clear() noexcept { RERR_InfoMap_Clear(ptr); }

    /// Return whether this info map contains the given key.
    bool HasKey(std::string const &key) const noexcept {
        return RERR_InfoMap_HasKey(ptr, key.c_str());
    }

    /// Get the value type for the given key.
    RERR_InfoValueType GetType(std::string const &key) const noexcept {
        return RERR_InfoMap_GetType(ptr, key.c_str());
    }

    /// Retrieve a string value from this info map.
    bool GetString(std::string const &key, std::string &value) const {
        const char *v;
        bool ok = RERR_InfoMap_GetString(ptr, key.c_str(), &v);
        if (ok) {
            value = v;
        } else {
            value.clear();
        }
        return ok;
    }

    /// Retrieve a boolean value from this info map.
    bool GetBool(std::string const &key, bool &value) const {
        return RERR_InfoMap_GetBool(ptr, key.c_str(), &value);
    }

    /// Retrieve a signed integer value from this info map.
    bool GetI64(std::string const &key, int64_t &value) const {
        return RERR_InfoMap_GetI64(ptr, key.c_str(), &value);
    }

    /// Retrieve an unsigned integer value from this info map.
    bool GetU64(std::string const &key, uint64_t &value) const {
        return RERR_InfoMap_GetU64(ptr, key.c_str(), &value);
    }

    /// Retrieve a floaing point value from this info map.
    bool GetF64(std::string const &key, double &value) const {
        return RERR_InfoMap_GetF64(ptr, key.c_str(), &value);
    }

    /// Get all keys in this info map as a vector of strings.
    std::vector<std::string> Keys() const {
        std::vector<std::string> ret;
        ret.reserve(RERR_InfoMap_GetSize(ptr));
        RERR_InfoMapIterator begin = RERR_InfoMap_Begin(ptr);
        RERR_InfoMapIterator end = RERR_InfoMap_End(ptr);
        for (RERR_InfoMapIterator it = begin; it != end;
             it = RERR_InfoMap_Advance(ptr, it)) {
            ret.push_back(RERR_InfoMapIterator_GetKey(it));
        }
        return ret;
    }

    /*
     * TODO With C++17, we could have a function to convert the whole map
     * to std::map<std::string, std::variant<std::string, bool, int64_t,
     * uint64_t, double>>, which might be a nice way to write idiomatic C++
     * code.
     */

    /// Wrapper of a key-value item, used as the value type for iterators.
    class Item final {
        RERR_InfoMapIterator it;

      public:
        /// Construct.
        /**
         * User code usually obtains Item objects from an iterator, rather
         * than directly constructing them.
         */
        Item(RERR_InfoMapIterator it) : it{it} {}

        /// Construct the no-item value.
        Item() : it{nullptr} {}

        // Default copy and move

        /// Return the key for this item.
        std::string GetKey() const { return RERR_InfoMapIterator_GetKey(it); }

        /// Return the value type for this item.
        RERR_InfoValueType GetType() const noexcept {
            return RERR_InfoMapIterator_GetType(it);
        }

        /// Return the string value for this item.
        /**
         * If this item's value is not a string, an empty string is
         * returned.
         */
        std::string GetString() const {
            auto s = RERR_InfoMapIterator_GetString(it);
            return s ? s : "";
        }

        /// Return the boolean value for this item.
        /**
         * If this item's value is not a boolean, the returned value is
         * undefined.
         */
        bool GetBool() const { return RERR_InfoMapIterator_GetBool(it); }

        /// Return the signed integer value for this item.
        /**
         * If this item's value is not a signed integer, the returned value
         * is undefined.
         */
        int64_t GetI64() const { return RERR_InfoMapIterator_GetI64(it); }

        /// Return the unsigned integer value for this item.
        /**
         * If this item's value is not an unsigned integer, the returned
         * value is undefined.
         */
        uint64_t GetU64() const { return RERR_InfoMapIterator_GetU64(it); }

        /// Return the floating point value for this item.
        /**
         * If this item's value is not a floating point number, the
         * returned value is undefined.
         */
        double GetF64() const { return RERR_InfoMapIterator_GetF64(it); }
    };

    /// Iterator.
    class const_iterator final {
        RERR_InfoMapPtr ptr; // non-owning
        RERR_InfoMapIterator it;

      public:
        /// The value type of the iterator.
        using value_type = const Item;

        /// The type for difference between two iterators.
        using difference_type = std::ptrdiff_t;

        /// The type for a reference to a value.
        using reference = Item const &;

        /// The type for a pointer to a value.
        using pointer = Item const *;

        /// The iterator category.
        using iterator_category = std::forward_iterator_tag;

        /// Construct.
        /**
         * User code usually obtains an iterator from InfoMap member
         * functions.
         *
         * \sa InfoMap::begin()
         * \sa InfoMap::cbegin()
         * \sa InfoMap::end()
         * \sa InfoMap::cend()
         */
        const_iterator(RERR_InfoMapPtr ptr, RERR_InfoMapIterator it)
            : ptr{ptr}, it{it} {}

        /// Construct the past-end value.
        const_iterator() : ptr{nullptr}, it{nullptr} {}

        // Default copy and move

        /// Access the value of this iterator, namely the item.
        value_type operator*() { return {it}; }

        /// Advance this iterator by one, returning the new iterator.
        const_iterator &operator++() {
            it = RERR_InfoMap_Advance(ptr, it);
            return *this;
        }

        /// Advance this iterator by one, returning the original iterator.
        const_iterator operator++(int) {
            return {ptr, RERR_InfoMap_Advance(ptr, it)};
        }

        /// Return whether two iterators point to the same item.
        bool operator==(const_iterator const &rhs) {
            return ptr == rhs.ptr && it == rhs.it;
        }

        /// Return whether two iterators point to different items.
        bool operator!=(const_iterator const &rhs) {
            return ptr != rhs.ptr || it != rhs.it;
        }
    };

    /// Return an iterator pointing at the first item of this info map.
    const_iterator cbegin() const noexcept {
        return {ptr, RERR_InfoMap_Begin(ptr)};
    }

    /// Return an iterator pointing at the first item of this info map.
    const_iterator begin() const noexcept { return cbegin(); }

    /// Return an iterator pointing past the last item of this info map.
    const_iterator cend() const noexcept {
        return {ptr, RERR_InfoMap_End(ptr)};
    }

    /// Return an iterator pointing past the last item of this info map.
    const_iterator end() const noexcept { return cend(); }
};

} // namespace RERR
