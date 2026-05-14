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
typedef unsigned int   UINT;
typedef unsigned long  ULONG;
typedef long           LONG;
typedef unsigned short WORD;
typedef unsigned int   DWORD;
typedef unsigned char  BYTE;
typedef char           CHAR;
typedef unsigned char  UCHAR;
typedef void*          HANDLE;
typedef void*          LPVOID;
typedef struct { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;

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
