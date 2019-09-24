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

    class InfoMap final {
        RERR_InfoMapPtr ptr;

    public:
        ~InfoMap() {
            RERR_InfoMap_Destroy(ptr);
        }

        /// Copy construct.
        InfoMap(InfoMap const& rhs) noexcept :
            ptr{ RERR_InfoMap_Copy(rhs.ptr) }
        {}

        /// Copy assign.
        InfoMap& operator=(InfoMap const& rhs) noexcept {
            RERR_InfoMap_Destroy(ptr);
            ptr = RERR_InfoMap_Copy(rhs.ptr);
            return *this;
        }

        /// Move construct.
        InfoMap(InfoMap&& rhs) noexcept {
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
        }

        /// Move assign.
        InfoMap& operator=(InfoMap&& rhs) noexcept {
            RERR_InfoMap_Destroy(ptr);
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
            return *this;
        }

        /// Construct empty map.
        InfoMap() noexcept :
            ptr{ RERR_InfoMap_Create() }
        {}

        explicit InfoMap(RERR_InfoMapPtr const&) = delete;

        /// Construct from a C pointer.
        /**
         * For safety, the C pointer must be an rvalue, so that it is made
         * explicit in user code that the new object is taking ownership of the
         * pointer. Use `std::move()` if necessary.
         */
        explicit InfoMap(RERR_InfoMapPtr&& cInfoMap) noexcept :
            ptr{ cInfoMap }
        {
            cInfoMap = nullptr;
        }

        /// Return the C pointer, relinquishing ownership.
        RERR_InfoMapPtr ReleaseCPtr() noexcept {
            auto ret = ptr;
            ptr = nullptr;
            return ret;
        }

        InfoMap MutableCopy() const noexcept {
            return InfoMap(RERR_InfoMap_MutableCopy(ptr));
        }

        InfoMap ImmutableCopy() const noexcept {
            return InfoMap(RERR_InfoMap_ImmutableCopy(ptr));
        }

        void MakeImmutable() noexcept {
            RERR_InfoMap_MakeImmutable(ptr);
        }

        bool IsMutable() const noexcept {
            return RERR_InfoMap_IsMutable(ptr);
        }

        static InfoMap OutOfMemory() noexcept {
            return InfoMap(RERR_InfoMap_CreateOutOfMemory());
        }

        bool IsOutOfMemory() const noexcept {
            return RERR_InfoMap_IsOutOfMemory(ptr);
        }

        bool HasProgrammingErrors() const noexcept {
            return RERR_InfoMap_HasProgrammingErrors(ptr);
        }

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

        size_t GetSize() const noexcept {
            return RERR_InfoMap_GetSize(ptr);
        }

        bool IsEmpty() const noexcept {
            return RERR_InfoMap_IsEmpty(ptr);
        }

        void ReserveCapacity(size_t capacity) noexcept {
            RERR_InfoMap_ReserveCapacity(ptr, capacity);
        }

        /*
         * Note: The Set methods are not a single overloaded method Set()
         * because the type to use for a given key is part of an API contract
         * between the error producer and error consumer.
         */

        void SetString(std::string const& key, std::string const& value) noexcept {
            RERR_InfoMap_SetString(ptr, key.c_str(), value.c_str());
        }

        void SetBool(std::string const& key, bool value) noexcept {
            RERR_InfoMap_SetBool(ptr, key.c_str(), value);
        }

        void SetI64(std::string const& key, int64_t value) noexcept {
            RERR_InfoMap_SetI64(ptr, key.c_str(), value);
        }

        void SetU64(std::string const& key, uint64_t value) noexcept {
            RERR_InfoMap_SetU64(ptr, key.c_str(), value);
        }

        void SetF64(std::string const& key, double value) noexcept {
            RERR_InfoMap_SetF64(ptr, key.c_str(), value);
        }

        void Remove(std::string const& key) noexcept {
            RERR_InfoMap_Remove(ptr, key.c_str());
        }

        void Clear() noexcept {
            RERR_InfoMap_Clear(ptr);
        }

        bool HasKey(std::string const& key) const noexcept {
            return RERR_InfoMap_HasKey(ptr, key.c_str());
        }

        RERR_InfoValueType GetType(std::string const& key) const noexcept {
            return RERR_InfoMap_GetType(ptr, key.c_str());
        }

        bool GetString(std::string const& key, std::string& value) const {
            const char* v;
            bool ok = RERR_InfoMap_GetString(ptr, key.c_str(), &v);
            if (ok) {
                value = v;
            }
            else {
                value.clear();
            }
            return ok;
        }

        bool GetBool(std::string const& key, bool& value) const {
            return RERR_InfoMap_GetBool(ptr, key.c_str(), &value);
        }

        bool GetI64(std::string const& key, int64_t& value) const {
            return RERR_InfoMap_GetI64(ptr, key.c_str(), &value);
        }

        bool GetU64(std::string const& key, uint64_t& value) const {
            return RERR_InfoMap_GetU64(ptr, key.c_str(), &value);
        }

        bool GetF64(std::string const& key, double& value) const {
            return RERR_InfoMap_GetF64(ptr, key.c_str(), &value);
        }

        std::vector<std::string> Keys() const {
            std::vector<std::string> ret;
            ret.reserve(RERR_InfoMap_GetSize(ptr));
            RERR_InfoMapIterator begin = RERR_InfoMap_Begin(ptr);
            RERR_InfoMapIterator end = RERR_InfoMap_End(ptr);
            for (RERR_InfoMapIterator it = begin; it != end; it = RERR_InfoMap_Advance(ptr, it)) {
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
            Item(RERR_InfoMapIterator it) : it{ it } {}
            Item() : it{ nullptr } {}
            // Default copy and move

            std::string GetKey() const {
                return RERR_InfoMapIterator_GetKey(it);
            }

            RERR_InfoValueType GetType() const noexcept {
                return RERR_InfoMapIterator_GetType(it);
            }

            std::string GetString() const {
                auto s = RERR_InfoMapIterator_GetString(it);
                return s ? s : "";
            }

            bool GetBool() const {
                return RERR_InfoMapIterator_GetBool(it);
            }

            int64_t GetI64() const {
                return RERR_InfoMapIterator_GetI64(it);
            }

            uint64_t GetU64() const {
                return RERR_InfoMapIterator_GetU64(it);
            }

            double GetF64() const {
                return RERR_InfoMapIterator_GetF64(it);
            }
        };

        /// Iterator.
        class const_iterator final {
            RERR_InfoMapPtr ptr; // non-owning
            RERR_InfoMapIterator it;

        public:
            using value_type = const Item;
            using difference_type = std::ptrdiff_t;
            using reference = Item const&;
            using pointer = Item const*;
            using iterator_category = std::forward_iterator_tag;

            const_iterator(RERR_InfoMapPtr ptr, RERR_InfoMapIterator it) :
                ptr{ ptr },
                it{ it }
            {}

            const_iterator() :
                ptr{ nullptr },
                it{ nullptr }
            {}
            // Default copy and move

            value_type operator*() {
                return { it };
            }

            const_iterator& operator++() {
                it = RERR_InfoMap_Advance(ptr, it);
                return *this;
            }

            const_iterator operator++(int) {
                return { ptr, RERR_InfoMap_Advance(ptr, it) };
            }

            bool operator==(const_iterator const& rhs) {
                return ptr == rhs.ptr && it == rhs.it;
            }

            bool operator!=(const_iterator const& rhs) {
                return ptr != rhs.ptr || it != rhs.it;
            }
        };

        const_iterator cbegin() const noexcept {
            return { ptr, RERR_InfoMap_Begin(ptr) };
        }

        const_iterator begin() const noexcept {
            return cbegin();
        }

        const_iterator cend() const noexcept {
            return { ptr, RERR_InfoMap_End(ptr) };
        }

        const_iterator end() const noexcept {
            return cend();
        }
    };

} // namespace RERR
