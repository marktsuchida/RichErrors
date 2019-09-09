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
 * \brief Header-only C++ interface for the RichErrors library.
 */

#ifndef __cplusplus
#error This header is for C++ only.
#endif

#include "RichErrors/RichErrors.h"

#include <string>
#include <vector>


/// C++ interface for RichErrors.
namespace RERR {

#if __cplusplus >= 201703L // C++17 has inline variables
    namespace Domains {
        inline std::string RichErrors(RERR_DOMAIN_RICHERRORS);
    }

    [[deprecated("Use RERR::Domains::RichErrors")]]
#endif
    /// Return the error code domain for errors arising in RichErrors.
    inline std::string RichErrorsDomain() {
        return RERR_DOMAIN_RICHERRORS;
    }


    /// A reference to a non-owned error.
    class WeakError {
    protected:
        // ptr is non-owning in WeakError and owning in Error

        /// The C pointer.
        RERR_ErrorPtr ptr;

        // No construction by user other than copy/move.

        /// Construct a non-error.
        WeakError() noexcept :
            ptr{ RERR_NO_ERROR }
        {}

        /// Construct from non-owned C pointer.
        explicit WeakError(RERR_ErrorPtr ptr) noexcept :
            ptr{ ptr }
        {}

    public:
        ~WeakError() = default;

        /// Copy construct.
        WeakError(WeakError const&) noexcept = default;
        /// Copy assign.
        WeakError& operator=(WeakError const&) noexcept = default;

        /// Move construct.
        WeakError(WeakError&&) noexcept = default;
        /// Move assign.
        WeakError& operator=(WeakError&&) noexcept = default;

        /// Return whether this instance represents an error.
        bool IsError() const noexcept {
            return ptr != RERR_NO_ERROR;
        }

        /// Return whether this instance represents no error.
        bool IsSuccess() const noexcept {
            return ptr == RERR_NO_ERROR;
        }

        /// Return whether this error has a code and domain.
        bool HasCode() const noexcept {
            return RERR_Error_HasCode(ptr);
        }

        /// Return the error code domain.
        /**
         * If this error does not have a code, an empty string is returned.
         */
        std::string GetDomain() const {
            return RERR_Error_GetDomain(ptr);
        }

        /// Return a pointer to the internal error code domain.
        /**
         * The returned C string is valid for the lifetime of this error.
         */
        char const* GetDomainCStr() const noexcept {
            return RERR_Error_GetDomain(ptr);
        }

        /// Return the error code.
        /**
         * If this error does not have a code, zero is returned.
         */
        int32_t GetCode() const noexcept {
            return RERR_Error_GetCode(ptr);
        }

        /// Return the error message.
        /**
         * This function returns a copy of the message.
         */
        std::string GetMessage() const {
            return RERR_Error_GetMessage(ptr);
        }

        /// Return a pointer to the internal error message.
        /**
         * The returned C string is valid for the lifetime of this error.
         */
        char const* GetMessageCStr() const noexcept {
            return RERR_Error_GetMessage(ptr);
        }

        /// Return whether this error has a cause, or original error.
        bool HasCause() const noexcept {
            return RERR_Error_HasCause(ptr);
        }

        /// Return a reference to the cause, or original error.
        /**
         * If this error has no cause, the returned object represents no error.
         *
         * The returned error is valid for the lifetime of this error.
         */
        WeakError GetCause() const noexcept {
            return WeakError{ RERR_Error_GetCause(ptr) };
        }

        /// Return references to all errors in the chain of causes.
        /**
         * If this object represents no error, the returned vector is empty.
         *
         * The returned errors are valid for the lifetime of this error.
         */
        std::vector<WeakError> GetCauseChain() const {
            std::vector<WeakError> ret;
            for (auto err = *this; err.IsError(); err = err.GetCause()) {
                ret.push_back(err);
            }
            return ret;
        }
    };


    /// An error.
    /**
     * This is the primary error object. It is movable but not copyable.
     */
    class Error final : public WeakError {
        // WeakPtr::ptr is managed similarly to unique_ptr

    public:
        ~Error() noexcept {
            RERR_Error_Destroy(ptr);
        }

        Error(Error const&) = delete;
        Error& operator=(Error const&) = delete;

        /// Move construct.
        Error(Error&& rhs) noexcept {
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
        }

        /// Move assign.
        Error& operator=(Error&& rhs) noexcept {
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
            return *this;
        }

        /// Construct a non-error.
        Error() noexcept : WeakError{} {}

        /// Construct without error code.
        explicit Error(std::string const& message) noexcept :
            WeakError{ RERR_Error_Create(message.c_str()) }
        {}

        /// Construct with error code.
        Error(std::string const& domain, int32_t code, std::string const& message) noexcept :
            WeakError{ RERR_Error_CreateWithCode(domain.c_str(), code, message.c_str()) }
        {}

        /// Construct with cause, without error code.
        /**
         * Because the new error takes ownership of the cause, the cause must
         * be an rvalue (use `std::move()` if necessary).
         */
        Error(Error&& cause, std::string const& message) noexcept :
            WeakError{ RERR_Error_Wrap(cause.ptr, message.c_str()) }
        {
            cause.ptr = nullptr;
        }

        /// Construct with cause and error code.
        /**
         * Because the new error takes ownership of the cause, the cause must
         * be an rvalue (use `std::move()` if necessary).
         */
        Error(Error&& cause, std::string const& domain, int32_t code, std::string const& message) noexcept :
            WeakError{ RERR_Error_WrapWithCode(cause.ptr, domain.c_str(), code, message.c_str()) }
        {
            cause.ptr = nullptr;
        }

        explicit Error(RERR_ErrorPtr const&) = delete;

        /// Construct from a C pointer.
        /**
         * For safety, the C pointer must be an rvalue, so that it is made
         * explicit in user code that the new object is taking ownership of the
         * pointer. Use `std::move()` if necessary.
         */
        explicit Error(RERR_ErrorPtr&& cError) noexcept :
            WeakError{ cError }
        {}

        /// Return the C pointer, relinquishing ownership.
        RERR_ErrorPtr ReleaseCPtr() noexcept {
            auto ret = ptr;
            ptr = nullptr;
            return ret;
        }

        /// Create an out-of-memory error.
        static Error OutOfMemory() noexcept {
            return Error(RERR_Error_OutOfMemory());
        }
    };

    /// Unregister all domains (for testing).
    inline void UnregisterAllDomains() noexcept {
        RERR_Domain_UnregisterAll();
    }

    /// Register an error code domain.
    inline Error RegisterDomain(std::string const& domain) noexcept {
        return Error(RERR_Domain_Register(domain.c_str()));
    }

} // namespace RERR
