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

#include <exception>
#include <string>
#include <utility>
#include <vector>


/// C++ interface for RichErrors.
namespace RERR {

    /// Return the error code domain for errors arising in RichErrors.
    inline std::string const& RichErrorsDomain() {
        static std::string const s = RERR_DOMAIN_RICHERRORS;
        return s;
    }

    /// Return the error code domain for out-of-memory errors.
    inline std::string const& OutOfMemoryDomain() {
        static std::string const s = RERR_DOMAIN_CRITICAL;
        return s;
    }


    /// An error.
    /**
     * This is the primary error object.
     */
    class Error final {
        /// The C pointer.
        RERR_ErrorPtr ptr;

    public:
        ~Error() {
            RERR_Error_Destroy(ptr);
        }

        /// Copy construct.
        Error(Error const& rhs) noexcept {
            RERR_Error_Copy(rhs.ptr, &ptr);
        }

        /// Copy assign.
        Error& operator=(Error const& rhs) noexcept {
            RERR_Error_Destroy(ptr);
            RERR_Error_Copy(rhs.ptr, &ptr);
            return *this;
        }

        /// Move construct.
        Error(Error&& rhs) noexcept {
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
        }

        /// Move assign.
        Error& operator=(Error&& rhs) noexcept {
            RERR_Error_Destroy(ptr);
            ptr = rhs.ptr;
            rhs.ptr = nullptr;
            return *this;
        }

        /// Construct a non-error.
        Error() noexcept :
            ptr{ RERR_NO_ERROR }
        {}

        /// Construct without error code.
        explicit Error(std::string const& message) noexcept :
            ptr{ RERR_Error_Create(message.c_str()) }
        {}

        /// Construct with error code.
        Error(std::string const& domain, int32_t code, std::string const& message) noexcept :
            ptr{ RERR_Error_CreateWithCode(domain.c_str(), code, message.c_str()) }
        {}

        /// Construct with cause, without error code.
        /**
         * Because the new error takes ownership of the cause, the cause must
         * be an rvalue (use `std::move()` if necessary).
         */
        Error(Error&& cause, std::string const& message) noexcept :
            ptr{ RERR_Error_Wrap(cause.ptr, message.c_str()) }
        {
            cause.ptr = nullptr;
        }

        /// Construct with cause and error code.
        /**
         * Because the new error takes ownership of the cause, the cause must
         * be an rvalue (use `std::move()` if necessary).
         */
        Error(Error&& cause, std::string const& domain, int32_t code, std::string const& message) noexcept :
            ptr{ RERR_Error_WrapWithCode(cause.ptr, domain.c_str(), code, message.c_str()) }
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
            ptr{ cError }
        {
            cError = nullptr;
        }

        /// Return the C pointer, relinquishing ownership.
        RERR_ErrorPtr ReleaseCPtr() noexcept {
            auto ret = ptr;
            ptr = nullptr;
            return ret;
        }

        /// Throw an exception containing this error.
        /**
         * The thrown exception is of type Exception. If this error represents
         * no-error, no exception is thrown.
         */
        void ThrowIfError();

        /// Create an out-of-memory error.
        static Error OutOfMemory() noexcept {
            return Error(RERR_Error_CreateOutOfMemory());
        }

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
         *
         * May throw `std::bad_alloc` if the return value could not be
         * allocated.
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
         *
         * May throw `std::bad_alloc` if the return value could not be
         * allocated.
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
         */
        Error GetCause() const noexcept {
            RERR_ErrorPtr cause = RERR_Error_GetCause(ptr);
            RERR_ErrorPtr causeCopy;
            RERR_Error_Copy(cause, &causeCopy);
            return Error(std::move(causeCopy));
        }

        /// Return references to all errors in the chain of causes.
        /**
         * The returned vector includes this error (unless this object
         * represents no error). If this object represents no error, the
         * returned vector is empty.
         *
         * May throw `std::bad_alloc` if the return value could not be
         * allocated.
         */
        std::vector<Error> GetCauseChain() const {
            std::vector<Error> ret;
            // Copying is cheap enough
            for (auto err = *this; err.IsError(); err = err.GetCause()) {
                ret.push_back(err);
            }
            return ret;
        }

        /// Return whether this error is an out-of-memory error.
        bool IsOutOfMemory() const noexcept {
            return RERR_Error_IsOutOfMemory(ptr);
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

    /// An exception that wraps Error.
    class Exception : public virtual std::exception {
        Error error;

    public:
        ~Exception() = default;

        /// Copy construct.
        Exception(Exception const& rhs) noexcept = default;

        /// Copy assign.
        Exception& operator=(Exception const& rhs) noexcept = default;

        /// Move construct.
        Exception(Exception&& rhs) noexcept = default;

        /// Move assign.
        Exception& operator=(Exception&& rhs) noexcept = default;

        /// Move construct from an error.
        explicit Exception(Error&& error) noexcept :
            error{ std::move(error) }
        {}

        /// Return an explanatory string.
        /**
         * Like `std::exception::what()`, this is not intended to be used for
         * displaying to the user. Rather, it is bear-minimum information for
         * e.g. logging an unrecoverable failure. As such, this method only
         * returns the message of the directly wrapped error, ignoring any
         * cause chain.
         */
        const char* what() const noexcept override {
            return error.GetMessageCStr();
        }

        /// Access the wrapped error.
        Error const& Error() const noexcept {
            return error;
        }
    };

    inline void Error::ThrowIfError() {
        if (IsSuccess()) {
            return;
        }
        throw Exception(std::move(*this));
    }

    /// Convenience function to throw RERR_ErrorPtr as exception.
    /**
     * The C error must be an rvalue, so this will work in
     *
     *     ThrowIfError(c_func_returning_error_ptr());
     *
     * but will require `std::move()` if throwing a C error held in a variable.
     * If storing the error temporarily, it is better to wrap in Error:
     *
     *     auto err = Error(c_func_returning_error_ptr());
     *     // ...
     *     err.ThrowIfError();
     */
    inline void ThrowIfError(RERR_ErrorPtr&& error) {
        Error(std::forward<RERR_ErrorPtr>(error)).ThrowIfError();
    }

} // namespace RERR
