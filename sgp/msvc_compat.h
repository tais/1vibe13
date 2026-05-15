// MSVC-only function/macro shims for non-Windows builds.
//
// JA2 source uses several Microsoft-specific spellings that aren't
// available on clang/gcc with POSIX libc. Until each call site is
// ported individually, this header maps the MSVC names onto their
// portable equivalents on non-Windows platforms.
//
// Included via types.h so it propagates everywhere.

#ifndef _SGP_MSVC_COMPAT_H
#define _SGP_MSVC_COMPAT_H

#ifndef _WIN32

#include <climits>    // PATH_MAX
#include <strings.h>  // strcasecmp, strncasecmp
#include <wchar.h>    // wcscasecmp on glibc; <wctype.h> on macOS
#include <cstdint>

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

// Win32 unsized typedefs that JA2 source uses without including
// windows.h (relying on transitive includes that we're trimming).
typedef int            BOOL;
typedef int            INT;
typedef unsigned int   UINT;
typedef unsigned long  ULONG;
typedef long           LONG;
typedef unsigned short WORD;
typedef unsigned short USHORT;
typedef unsigned int   DWORD;
typedef unsigned char  BYTE;
typedef char           CHAR;
typedef wchar_t        WCHAR;
typedef unsigned char  UCHAR;
typedef void           VOID;
typedef void*          HANDLE;
typedef void*          LPVOID;
typedef char*          LPSTR;
typedef const char*    LPCSTR;
typedef wchar_t*       LPWSTR;
typedef const wchar_t* LPCWSTR;
typedef struct { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;

// Win32 string conversion constants and functions, stubbed for
// non-Windows. These code paths need real cross-platform replacements
// (e.g. std::codecvt or iconv) in a later phase; the stubs let the
// code compile so we can find and fix call sites incrementally.
#ifndef CP_ACP
#define CP_ACP   0
#endif
#ifndef CP_UTF8
#define CP_UTF8  65001
#endif

#ifdef __cplusplus
inline int MultiByteToWideChar(UINT, DWORD, const char*, int,
                               wchar_t*, int) { return 0; }
inline int WideCharToMultiByte(UINT, DWORD, const wchar_t*, int,
                               char*, int, const char*, BOOL*) { return 0; }

// Monotonic milliseconds counter. Win32 GetTickCount returns msecs
// since boot; the standard-library steady_clock has an unspecified
// origin which is fine for the relative-interval use cases in JA2.
#include <chrono>
#include <thread>
inline DWORD GetTickCount() {
    auto t = std::chrono::steady_clock::now().time_since_epoch();
    return (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

// Win32 Sleep takes milliseconds.
inline void Sleep(DWORD ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Win32 ZeroMemory is a thin memset wrapper.
#include <cstring>
inline void ZeroMemory(void* dst, size_t len) { std::memset(dst, 0, len); }

// MSVC's _abs64 is the standard llabs (absolute value of long long).
#include <cstdlib>
inline long long _abs64(long long v) { return llabs(v); }

// Win32 GetLastError. We have no per-thread error state to surface,
// so return 0 ("success" / "no error info") and let callers fall
// through to their generic error path.
inline DWORD GetLastError() { return 0; }
#endif

// A handful of Win32 ERROR_* values used at call sites that compare
// against GetLastError(). The values match the official Win32 SDK so
// any code conditioning on them stays semantically equivalent.
#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS              0L
#endif
#ifndef ERROR_NOT_READY
#define ERROR_NOT_READY            21L
#endif
#ifndef ERROR_INSUFFICIENT_BUFFER
#define ERROR_INSUFFICIENT_BUFFER  122L
#endif

#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef __min
#define __min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef __max
#define __max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define _stricmp   strcasecmp
#define _strnicmp  strncasecmp
#define _wcsicmp   wcscasecmp
#define sprintf_s  snprintf

#ifndef MAXUINT8
#define MAXUINT8  ((uint8_t)0xff)
#endif
#ifndef MAXUINT16
#define MAXUINT16 ((uint16_t)0xffff)
#endif
#ifndef MAXUINT32
#define MAXUINT32 ((uint32_t)0xffffffff)
#endif

// In-place uppercase/lowercase conversions for ASCII-range only;
// matches MSVC behavior for the JA2 call sites that don't deal with
// locale-sensitive characters.
#ifdef __cplusplus
#include <cctype>
#include <cwctype>
inline char* _strupr(char* s) {
    for (char* p = s; *p; ++p) *p = (char)toupper((unsigned char)*p);
    return s;
}
inline char* _strlwr(char* s) {
    for (char* p = s; *p; ++p) *p = (char)tolower((unsigned char)*p);
    return s;
}
inline wchar_t* _wcsupr(wchar_t* s) {
    for (wchar_t* p = s; *p; ++p) *p = (wchar_t)towupper(*p);
    return s;
}
inline wchar_t* _wcslwr(wchar_t* s) {
    for (wchar_t* p = s; *p; ++p) *p = (wchar_t)towlower(*p);
    return s;
}

// Integer to wide-string conversion. JA2 only uses radix 10/16.
// Implemented by hand so we don't have to wrestle with the swprintf
// macro (whose array-deduction template can't accept a pointer here).
inline wchar_t* _itow_impl(long value, wchar_t* buf, int radix) {
    if (radix != 10 && radix != 16) radix = 10;
    wchar_t tmp[34];
    int idx = 0;
    bool negative = (radix == 10 && value < 0);
    unsigned long uv = negative ? (unsigned long)(-(value + 1)) + 1 : (unsigned long)value;
    if (uv == 0) tmp[idx++] = L'0';
    while (uv) {
        int d = uv % (unsigned)radix;
        tmp[idx++] = (wchar_t)(d < 10 ? (L'0' + d) : (L'a' + d - 10));
        uv /= (unsigned)radix;
    }
    if (negative) tmp[idx++] = L'-';
    for (int i = 0; i < idx; ++i) buf[i] = tmp[idx - 1 - i];
    buf[idx] = L'\0';
    return buf;
}
inline wchar_t* _itow(int value, wchar_t* buf, int radix) { return _itow_impl(value, buf, radix); }
inline wchar_t* _ltow(long value, wchar_t* buf, int radix) { return _itow_impl(value, buf, radix); }

// Wide-string to int. MSVC's _wtoi is wcstol with radix 10.
inline int _wtoi(const wchar_t* s) { return (int)wcstol(s, nullptr, 10); }

// MSVC's 2-arg wcstok keeps state in a thread-unsafe global. Provide
// the same shape on top of POSIX's reentrant 3-arg wcstok.
inline wchar_t* wcstok_2arg(wchar_t* str, const wchar_t* delim) {
    static wchar_t* state = nullptr;
    return ::wcstok(str, delim, &state);
}
#define wcstok(str, delim) ::wcstok_2arg((str), (delim))
#endif

// MSVC's legacy swprintf signature is swprintf(buf, fmt, ...). The
// POSIX/standard signature is swprintf(buf, count, fmt, ...). JA2's
// 3000+ call sites all pass fixed-size CHAR16 arrays as the buffer,
// so we route them through a template that recovers the array extent
// at compile time. Call sites that pass a pointer (instead of an
// array) fail to compile here intentionally -- the original code in
// those spots silently writes at most sizeof(ptr)/sizeof(CHAR16)
// characters and needs an explicit size argument.
#ifdef __cplusplus
#include <cstdarg>
namespace sgp_compat {
    template<size_t N>
    constexpr size_t arrsize(const wchar_t (&)[N]) { return N; }
}
#define swprintf(buf, ...) ::swprintf((buf), ::sgp_compat::arrsize(buf), __VA_ARGS__)

// vswprintf has the same MSVC 3-arg / POSIX 4-arg split. Solved as a
// template overload at global scope so call sites with array buffers
// pick this up automatically. Pointer-buffer sites fail to compile,
// same as the swprintf macro.
template<size_t N>
inline int vswprintf(wchar_t (&buf)[N], const wchar_t* fmt, va_list args) {
    return ::vswprintf(buf, N, fmt, args);
}
#endif

#endif // !_WIN32

// Portable swprintf wrapper for sites that hold a buffer as a pointer
// (and thus can't ride the macro that infers the array extent). The
// caller must pass the buffer's element count explicitly. Available on
// every platform; on Windows it routes through vswprintf which has
// taken the standard 3-arg signature for many MSVC releases.
#ifdef __cplusplus
#include <cstdarg>
#include <wchar.h>
inline int sgp_swprintf(wchar_t* buf, size_t count, const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = vswprintf(buf, count, fmt, args);
    va_end(args);
    return r;
}
#endif

#endif // _SGP_MSVC_COMPAT_H
