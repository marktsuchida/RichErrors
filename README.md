RichErrors - Error handling for C
=================================

RichErrors is a small C library for doing a little better than returning
integer codes for errors. It does not do anything akin to exception handling
(such as in C++), but makes it easy to pass detailed error information up the
call chain.


RichErrors Core (`RichErrors.h`)
--------------------------------

An error in RichErrors is represented with the opaque type `RERR_ErrorPtr`.
This type behaves similarly to a pointer, but is not always a pointer, so only
RichErrors API functions should be used to access its values.

An error consists of the following:
- A string message
- An optional 32-bit integer code, together with an error "domain" indicating
  different subsystems to which the code may belong. The domain is required if
  a code is included.
- An optional "info map", which is an arbitrary string-to-value map. Values can
  be string, boolean, 64-bit signed integer, 64-bit unsigned integer, or 64-bit
  floating point. RichErrors does not provide any rules for info map contents.
- An optional "cause", which is a nested error. This is used to wrap low-level
  errors inside higher-level errors, to provide a causal chain.

Error objects are immutable: once created, none of their contents may change.

Basic Usage
-----------

To return a basic error:
```C
RERR_ErrorPtr foo(...)
{
    // ...
    if (something_went_wrong()) {
        return RERR_Error_Create("Something went wrong");
    }
}
```
This is the minimal usage, and calling code will not be able to
programmatically determine the nature of the error. The advantage is that it is
very easy to write (potentially dynamically generated) error messages, so that
programmers will (hopefully) be motivated to do proper error handling.

To return an error with an error code from some subsystem "Bar":
```C
#define BAR_ERROR_DOMAIN "BarSystem"

// Call e.g. RERR_Domain_Register(BAR_ERROR_DOMAIN, RERR_CodeFormat_I32)
// during initialization.

RERR_ErrorPtr foo2(...)
{
    // ...
    BarErrorCode err = BarSystemFunction(...);
    if (err != BarError_OK) {
        return RERR_Error_CreateWithCode(BAR_ERROR_DOMAIN, err,
                "Something went wrong in Bar");
    }
}
```
If, as is often the case, the subsystem Bar offers a function to fetch a string
message for each error code (similar to `strerror()`), then one mighe write a
small wrapper function that takes a Bar error code and returns an
`RERR_ErrorPtr` with the corresponding message.

To return an out-of-memory error:
```C
// ...
void *buffer = malloc(1000000000);
if (buffer == NULL) {
    return RERR_Error_CreateOutOfMemory();
}
```
The out-of-memory error behaves just like any other error, except that it is
implemented as a special pointer value with no associated dynamically allocated
memory, so that it is guaranteed to work even when heap memory is unavailable.
All RichErrors functions return an out-of-memory error when internal memory
allocation fails.

To check an error return value:
```C
// ...
RERR_ErrorPtr err = foo(...);
if (err != RERR_NO_ERROR) {
    MyErrorHandler(RERR_Error_GetMessage(err));
    RERR_Error_Destroy(err);
}
// ...
```
Note that all errors (including out-of-memory errors) must be destroyed. The
only exception is `RERR_NO_ERROR`.

To ignore an error return value:
```C
// ...
RERR_Error_Destroy(foo(...));
// ...
```
It is safe to destroy an `RERR_NO_ERROR`. This makes it explicit that errors
are being ignored. If you simply ignore the returned error, you will leak
memory every time an error occurs. (And a leak checker could be used to detect
unhandled errors--perhaps a future version of RichErrors itself will provide
such instrumentation.)


Error Code Domains
------------------

Many complex systems consist of multiple subsystems, each of which may define
their own error codes. For this reason, RichErrors requires all errors with
codes to indicate which "domain" they belong to. Domains are essentially
namespaces for error codes.

Domains are C strings, and it is assumed that static string constants will be
used.

Domains need to be registered before use, to prevent clashing domains (this may
become configurable in future versions).


RichErrors Design Notes
-----------------------

If the system or application consists of multiple DLLs, a single, central DLL
should link to RichErrors. All other DLLs can then call API methods of the
central DLL that wrap RichErrors functions. In this way, all resource
allocation and deallocation for RichErrors occurs in the central DLL, meaning
that it is safe to pass errors (`RERR_Error_Ptr`) across DLL boundaries, even
if the DLLs each use a different C runtime. (This matters mostly on Windows
where different versions and configurations of MSVC use different runtimes.)


Err2Code (`Err2Code.h`)
-----------------------

Err2Code is an add-on feature based on RichErrors, which helps with
retrofitting rich error handling on top of a legacy API (ABI) that uses integer
error codes (so long as the error code type is at least 32 bits wide).

Imagine that you have a service-provider interface in some system. A large
number of modules (possibly developed by different parties) implements this
interface. The interface consists of functions that return an integer error
code. Now you want to let the implementations return more detailed information
with errors, but you cannot convert all implementations at the same time. You
also don't want to maintain multiple versions of the interface.

This is where Err2Code comes in. You provide a way for individual modules to
declare that they return rich error information (this needs to happen before
any other module functions are called, for obvious reasons). You also expose to
the modules an API function that converts an `RERR_Error_Ptr` into an `int32_t`
(or whatever integer type the interface uses for error codes). This function
would internally call `RERR_ErrorMap_RegisterThreadLocal()`.

New or updated modules can now use RichErrors for internal error handling. At
the interface boundary, they can call the API method to obtain an integer error
code, which they return to the system. Older modules continue to use plain
integer error codes, through the same ABI.

```C
// In module implementing service provider API:
int Foo(...)
{
    RERR_ErrorPtr err = Bar();
    if (err != RERR_NO_ERROR) {
        // ErrorAsReturnCode() is a system-provided API function that
        // internally calls RERR_ErrorMap_RegisterThreadLocal().
        return ErrorAsReturnCode(err);
    }
    // ...
}
```

```C
// In system code calling the modules:
// ...
int err = Foo(...); // In module DLL
if (err != 0) {
    RERR_ErrorPtr richErr;
    if (the_module_uses_rich_errors()) {
        richErr = RERR_ErrorMap_RetrieveThreadLocal(codeMap, err);
    }
    else {
        richErr = RERR_Error_CreateWithCode(LEGACY_CODE_DOMAIN, err,
                "An error occurred");
    }
    return richErr;
}
// ...
```
(In real code, the call to `Foo()` might occur through a function pointer.)


Err2Code Design Notes
---------------------

Err2Code uses a map from the integer error codes to registered error objects.
The map is (at least conceptually) per-thread.

Why not change the error code type to `intptr_t` and return the pointer
directly?

- Because that would break the API on 64-bit systems (assuming the original
  error code type is 32-bit). Note that more than one module may participate in
  a call chain (through callbacks), and these may be a mixture of updated and
  legacy modules. In such cases, error codes originating from an updated module
  (with rich error information) may need to pass through legacy modules that
  still consider errors to be plain integer codes. If we changed the size of
  the error code type, all modules would need to be modified to use the new
  type.

- Using a 64-bit type can also cause ambiguity if negative error codes are in
  use. Sign-extending the code would probably work most of the time, but some
  error codes out there are not supposed to be interpreted as signed integers
  (e.g. Windows `HRESULT` codes).

Why use a per-thread map for assigned error codes?

- Primarily for resource-management reasons. Again note that the call chain may
  bounce between multiple modules, some of which may be legacy modules that
  treat error codes as plain integers. Because we cannot count on such modules
  notifying the system of "discarded" codes, the only way we can determine when
  it is safe to discard the error information mapped to a code is when the call
  stack pops to the top-level system. At that time, we know that all error
  codes assigned on the particular thread can be released. Further assumptions
  may be possible depending on system behavior.

- Because of this design, all modules need to observe the rule that error codes
  mapped on one thread must not be shared with other threads. This can be
  enforced in new code that uses rich error information. In theory, legacy
  modules may be sharing error codes between threads, and this would break our
  design. We assume that such cases are rare enough to be dealt with
  individually as long as we can detect them. By using globally sequential
  error codes, codes returned (or pushed) on the wrong thread can be flagged
  with very high probability.

How do we deal with a call stack mixing modules with modernized and legacy
error handling?

- For example, assume a Core DLL uses Err2Code to assign codes to errors.
  Modules A and B implement an interface provided by Core, but only Module B
  uses rich error information; module A treats error codes as plain integers.

- Things get complicated when Core calls Module A, which calls a callback into
  Core, which in turn calls into Module B. Supppose an error occurs in Module
  B. Module B produces rich error information and calls the Core API to assign
  an integer code to it. It then returns the code to Core. The callback returns
  the code to Module A, which passes it straight through back to Core as the
  return value of the first call out of Core. Now Core has the problem that it
  cannot determine for sure whether the returned error code is a plain integer
  code from Module A, or one that is mapped to rich error information from
  Module B.

- However, it should be noted that Core has the knowledge that Module A uses
  legacy error handling and Module B uses rich error handling. Therefore, upon
  Module B returning a code mapped to a rich error, it can substitute a fixed
  code that does not clash with any of the plain error codes used or handled by
  Module A. When Module A returns, Core can now unambiguously determine whether
  any returned code came from Module A or was the substitute code for the
  pending rich error from Module B.

- A few assumptions must be met for all of this to work perfectly, and the
  details will likely vary from system to system. Most importantly, there needs
  to be at least one error code that is not used by legacy modules. If modules
  communicate with each other directly, there had better be a reasonably large
  range of common unused error codes that can be used as the range for Err2Code
  mapping.

In general the fidelity of error handling via Err2Code mirrors the quality of
the original code. If error codes are already clearly defined and used
consistently throughout the system, then Err2Code can map a range of codes that
were never in use, so that no clashes occur. If no policy on error codes was
enforced to begin with in a system with many modules, then in all likelyhood
problems with clashing error codes are already present, and Err2Code will
probably not make the situation much worse if used carefully (and in the long
run will help eliminate any clashes).


Performance
-----------

RichErrors will not add much overhead when no errors occur, because functions
will simply be returning null. In the case that errors do occur, we assume that
efficiency is not a primary concern, and that errors do not contain large
amounts of data.

Don't use errors as a substitute for control flow.


C++ API
-------

It makes sense to have a C API (and ABI) between DLLs, yet use C++ for internal
implementation of each DLL. To support error handling in such cases, RichErrors
has a C++ API that is a wrapper around the C objects. The C++ API is a
header-only library that depens on the C API.


Building RichErrors
-------------------

Meson is used for building.

```sh
meson build/
cd build
ninja
ninja test
ninja install
```

To build with Visual C++ on Windows, `meson` and `ninja` need to be run from
the Visual Studio Tools Command Prompt.