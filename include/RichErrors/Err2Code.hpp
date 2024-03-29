// This file is part of RichErrors.
// Copyright 2019-2022 Board of Regents of the University of Wisconsin System
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

/** \file
 * \brief Header-only C++ interface for Err2Code.
 */

#ifndef __cplusplus
#error This header is for C++ only.
#endif

#include "RichErrors/Err2Code.h"
#include "RichErrors/RichErrors.hpp"

#include <cstring>

namespace RERR {

/// Map from thread-local integer codes to errors.
class ErrorMap final {
    RERR_ErrorMapPtr ptr;

  public:
    ~ErrorMap() { RERR_ErrorMap_Destroy(ptr); }

    ErrorMap(ErrorMap const &) = delete;
    ErrorMap &operator=(ErrorMap const &) = delete;

    /// Move construct.
    ErrorMap(ErrorMap &&rhs) noexcept {
        ptr = rhs.ptr;
        rhs.ptr = nullptr;
    }

    /// Move assign.
    ErrorMap &operator=(ErrorMap &&rhs) noexcept {
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
            return ok.minMappedCode && ok.maxMappedCode && ok.noErrorCode &&
                   ok.outOfMemoryCode && ok.mapFailureCode;
        }

      public:
        /// Construct.
        Config() noexcept {
            memset(&c, 0, sizeof(c));
            memset(&ok, 0, sizeof(ok));
        }

        /// Set the code range for mapping errors.
        Config &SetMappedRange(int32_t minCode, int32_t maxCode) noexcept {
            c.minMappedCode = minCode;
            c.maxMappedCode = maxCode;
            ok.minMappedCode = 1;
            ok.maxMappedCode = 1;
            return *this;
        }

        /// Set the code to use for no-error.
        Config &SetNoErrorCode(int32_t noErrCode) noexcept {
            c.noErrorCode = noErrCode;
            ok.noErrorCode = 1;
            return *this;
        }

        /// Set the code to use for out-of-memory errors.
        Config &SetOutOfMemoryCode(int32_t outOfMemoryCode) noexcept {
            c.outOfMemoryCode = outOfMemoryCode;
            ok.outOfMemoryCode = 1;
            return *this;
        }

        /// Set the code to use when failed to assign a code.
        Config &SetMapFailureCode(int32_t failureCode) noexcept {
            c.mapFailureCode = failureCode;
            ok.mapFailureCode = 1;
            return *this;
        }
    };

    /// Construct new.
    /**
     * May throw Exception if configuration is invalid or allocation failed.
     */
    explicit ErrorMap(Config const &config) {
        if (!config.IsComplete()) {
            Error(RichErrorsDomain(), RERR_ECODE_MAP_INVALID_CONFIG,
                  "Incomplete error map configuration (programming error)")
                .ThrowIfError();
        }
        ThrowIfError(RERR_ErrorMap_Create(&ptr, &config.c));
    }

    explicit ErrorMap(RERR_ErrorMapPtr const &) = delete;

    /// Construct from a C poiner.
    /**
     * For safety, the C pointer must be an rvalue, so that it is made
     * explicit in user code that the new object is taking ownership of the
     * pointer. Use `std::move()` if necessary.
     */
    explicit ErrorMap(RERR_ErrorMapPtr &&cMap) noexcept : ptr{cMap} {
        cMap = nullptr;
    }

    /// Return the C pointer, relinquishing ownership.
    RERR_ErrorMapPtr ReleaseCPtr() noexcept {
        auto ret = ptr;
        ptr = nullptr;
        return ret;
    }

    /// Register an error and assign an integer code.
    int32_t RegisterThreadLocal(Error &&error) noexcept {
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
    void ClearThreadLocal() noexcept { RERR_ErrorMap_ClearThreadLocal(ptr); }
};

} // namespace RERR
