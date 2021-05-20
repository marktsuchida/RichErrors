// RichErrors: A C library for rich (nested) error information.
// Author: Mark A. Tsuchida
//
// Copyright 2019-2021 Board of Regents of the University of Wisconsin System
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
 * \brief Header-only C++ interface for Err2Code.
 */

#ifndef __cplusplus
#error This header is for C++ only.
#endif

#include "RichErrors/RichErrors.hpp"
#include "RichErrors/Err2Code.h"

#include <cstring>


namespace RERR {

    /// Map from thread-local integer codes to errors.
    class ErrorMap final {
        RERR_ErrorMapPtr ptr;

    public:
        ~ErrorMap() {
            RERR_ErrorMap_Destroy(ptr);
        }

        ErrorMap(ErrorMap const&) = delete;
        ErrorMap& operator=(ErrorMap const&) = delete;

        /// Move construct.
        ErrorMap(ErrorMap&& rhs) noexcept {
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
        }

        /// Move assign.
        ErrorMap& operator=(ErrorMap&& rhs) noexcept {
            RERR_ErrorMap_Destroy(ptr);
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
            return *this;
        }

        /// Configuration builder for error map.
        class Config final {
            friend class ErrorMap;
            RERR_ErrorMapConfig c;
            RERR_ErrorMapConfig ok; // Record set fields

            bool IsComplete() const noexcept {
                return ok.minMappedCode && ok.maxMappedCode &&
                    ok.noErrorCode && ok.outOfMemoryCode && ok.mapFailureCode;
            }

        public:
            /// Construct.
            Config() noexcept {
                memset(&c, 0, sizeof(c));
                memset(&ok, 0, sizeof(ok));
            }

            /// Set the code range for mapping errors.
            Config& SetMappedRange(int32_t minCode, int32_t maxCode) noexcept {
                c.minMappedCode = minCode;
                c.maxMappedCode = maxCode;
                ok.minMappedCode = 1;
                ok.maxMappedCode = 1;
                return *this;
            }

            /// Set the code to use for no-error.
            Config& SetNoErrorCode(int32_t noErrCode) noexcept {
                c.noErrorCode = noErrCode;
                ok.noErrorCode = 1;
                return *this;
            }

            /// Set the code to use for out-of-memory errors.
            Config& SetOutOfMemoryCode(int32_t outOfMemoryCode) noexcept {
                c.outOfMemoryCode = outOfMemoryCode;
                ok.outOfMemoryCode = 1;
                return *this;
            }

            /// Set the code to use when failed to assign a code.
            Config& SetMapFailureCode(int32_t failureCode) noexcept {
                c.mapFailureCode = failureCode;
                ok.mapFailureCode = 1;
                return *this;
            }
        };

        /// Construct new.
        /**
         * May throw Exception if configuration is invalid or allocation failed.
         */
        explicit ErrorMap(Config const& config) {
            if (!config.IsComplete()) {
                Error(RichErrorsDomain(), RERR_ECODE_MAP_INVALID_CONFIG,
                    RERR_CodeFormat_I32,
                    "Incomplete error map configuration (programming error)").
                    ThrowIfError();
            }
            ThrowIfError(RERR_ErrorMap_Create(&ptr, &config.c));
        }

        explicit ErrorMap(RERR_ErrorMapPtr const&) = delete;

        /// Construct from a C poiner.
        /**
         * For safety, the C pointer must be an rvalue, so that it is made
         * explicit in user code that the new object is taking ownership of the
         * pointer. Use `std::move()` if necessary.
         */
        explicit ErrorMap(RERR_ErrorMapPtr&& cMap) noexcept :
            ptr{ cMap }
        {
            cMap = nullptr;
        }

        /// Return the C pointer, relinquishing ownership.
        RERR_ErrorMapPtr ReleaseCPtr() noexcept {
            auto ret = ptr;
            ptr = nullptr;
            return ret;
        }

        /// Register an error and assign an integer code.
        int32_t RegisterThreadLocal(Error&& error) noexcept {
            auto cError = error.ReleaseCPtr();
            return RERR_ErrorMap_RegisterThreadLocal(ptr, cError);
        }

        /// Return whether an error is registered under an integer code.
        bool IsRegisteredThreadLocal(int32_t code) noexcept {
            return RERR_ErrorMap_IsRegisteredThreadLocal(ptr, code);
        }

        /// Retrieve an error by its assigned integer code.
        Error RetrieveThreadLocal(int32_t mappedCode) {
            return Error(RERR_ErrorMap_RetrieveThreadLocal(ptr, mappedCode));
        }

        /// Clear integer code assignments for the current thread.
        void ClearThreadLocal() noexcept {
            RERR_ErrorMap_ClearThreadLocal(ptr);
        }
    };

} // namespace RERR
