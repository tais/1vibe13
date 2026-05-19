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
#include <algorithm>     // std::min/std::max
#include <type_traits>   // std::common_type for global ::min/::max overloads
#include <math.h>        // bare log/sqrt/sin/cos/pow/floor/ceil/etc.
                         // libstdc++'s <cmath> exposes them in std:: only;
                         // JA2 calls them unqualified everywhere.

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
typedef long long      LONGLONG;
typedef unsigned long long ULONGLONG;
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
typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow;
    DWORD dwReserved0, dwReserved1;
    char cFileName[260];
    char cAlternateFileName[14];
} WIN32_FIND_DATA;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

// Win32 file flag constants used by LibraryDataBase.cpp et al.
typedef DWORD* LPDWORD;
#ifndef GENERIC_READ
#define GENERIC_READ              0x80000000L
#define GENERIC_WRITE             0x40000000L
#define OPEN_EXISTING             3
#define CREATE_ALWAYS             2
#define FILE_FLAG_RANDOM_ACCESS   0x10000000L
#define FILE_BEGIN                0
#define FILE_CURRENT              1
#define FILE_END                  2
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000L
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100L
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200L
#define LANG_NEUTRAL              0
#define SUBLANG_DEFAULT           1
#endif
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;
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
// Real UTF-8 -> wchar_t conversion. Win32's MultiByteToWideChar lets
// the caller pass the source byte count (cbMultiByte) or -1 to mean
// "null-terminated". The destination size (cchWideChar) being 0 means
// "return the required wide-char count (including terminator)".
//
// JA2 only ever invokes this with CP_UTF8 (the XML loader path); the
// Code Page argument is therefore ignored. JA2's XML strings are
// short and ASCII-heavy in practice, but we decode full UTF-8 so any
// non-ASCII glyph that does make it through ends up as the correct
// Unicode codepoint (and the Font.cpp GetIndex translation table can
// then decide whether to render it).
inline int MultiByteToWideChar(UINT /*CodePage*/, DWORD /*dwFlags*/,
                               const char* lpMultiByteStr, int cbMultiByte,
                               wchar_t* lpWideCharStr, int cchWideChar)
{
    if (!lpMultiByteStr) return 0;
    // cbMultiByte = -1 means treat lpMultiByteStr as a null-terminated
    // C string and INCLUDE the terminator in the wide output.
    const bool includeTerminator = (cbMultiByte < 0);
    int srcLen = cbMultiByte;
    if (cbMultiByte < 0) {
        srcLen = 0;
        while (lpMultiByteStr[srcLen]) ++srcLen;
        ++srcLen;  // include trailing NUL
    }

    int outCount = 0;
    int i = 0;
    while (i < srcLen) {
        unsigned char c0 = (unsigned char)lpMultiByteStr[i];
        unsigned int cp;
        int extra;
        if (c0 < 0x80)        { cp = c0;          extra = 0; }
        else if ((c0 & 0xE0) == 0xC0) { cp = c0 & 0x1F; extra = 1; }
        else if ((c0 & 0xF0) == 0xE0) { cp = c0 & 0x0F; extra = 2; }
        else if ((c0 & 0xF8) == 0xF0) { cp = c0 & 0x07; extra = 3; }
        else                   { cp = '?';        extra = 0; }

        // Bail if a multibyte sequence would run past the input.
        if (i + 1 + extra > srcLen) { cp = '?'; extra = 0; }
        for (int j = 0; j < extra; ++j) {
            unsigned char cn = (unsigned char)lpMultiByteStr[i + 1 + j];
            if ((cn & 0xC0) != 0x80) { cp = '?'; extra = 0; break; }
            cp = (cp << 6) | (cn & 0x3F);
        }
        i += 1 + extra;

        if (cchWideChar > 0) {
            if (outCount >= cchWideChar) {
                // truncated -- ensure final wchar is NUL when the source was
                // a C string (matches Win32 MultiByteToWideChar behavior).
                if (includeTerminator && cchWideChar > 0) {
                    lpWideCharStr[cchWideChar - 1] = 0;
                }
                return 0;  // Win32 returns 0 on insufficient buffer
            }
            lpWideCharStr[outCount] = (wchar_t)cp;
        }
        ++outCount;
        if (cp == 0 && includeTerminator) break;
    }
    return outCount;
}

inline int WideCharToMultiByte(UINT /*CodePage*/, DWORD /*dwFlags*/,
                               const wchar_t* lpWideCharStr, int cchWideChar,
                               char* lpMultiByteStr, int cbMultiByte,
                               const char* /*lpDefaultChar*/, BOOL* /*lpUsedDefaultChar*/)
{
    if (!lpWideCharStr) return 0;
    const bool includeTerminator = (cchWideChar < 0);
    int srcLen = cchWideChar;
    if (cchWideChar < 0) {
        srcLen = 0;
        while (lpWideCharStr[srcLen]) ++srcLen;
        ++srcLen;
    }
    int outCount = 0;
    for (int i = 0; i < srcLen; ++i) {
        unsigned int cp = (unsigned int)lpWideCharStr[i];
        unsigned char buf[4];
        int n;
        if (cp < 0x80) { buf[0] = (unsigned char)cp; n = 1; }
        else if (cp < 0x800) { buf[0] = (unsigned char)(0xC0 | (cp >> 6));
                               buf[1] = (unsigned char)(0x80 | (cp & 0x3F)); n = 2; }
        else if (cp < 0x10000) { buf[0] = (unsigned char)(0xE0 | (cp >> 12));
                                 buf[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
                                 buf[2] = (unsigned char)(0x80 | (cp & 0x3F)); n = 3; }
        else { buf[0] = (unsigned char)(0xF0 | (cp >> 18));
               buf[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
               buf[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
               buf[3] = (unsigned char)(0x80 | (cp & 0x3F)); n = 4; }
        if (cbMultiByte > 0) {
            if (outCount + n > cbMultiByte) return 0;
            for (int j = 0; j < n; ++j) lpMultiByteStr[outCount + j] = (char)buf[j];
        }
        outCount += n;
        if (cp == 0 && includeTerminator) break;
    }
    return outCount;
}

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

// Win32 OutputDebugString -- emit to stderr on non-Windows.
#include <cstdio>
inline void OutputDebugStringA(const char* s) { if (s) std::fputs(s, stderr); }
inline void OutputDebugStringW(const wchar_t* s) { if (s) std::fputws(s, stderr); }
#define OutputDebugString OutputDebugStringA

// MSVC's _abs64 is the standard llabs (absolute value of long long).
#include <cstdlib>
inline long long _abs64(long long v) { return llabs(v); }

// Win32 GetLastError. We have no per-thread error state to surface,
// so return 0 ("success" / "no error info") and let callers fall
// through to their generic error path.
inline DWORD GetLastError() { return 0; }

// MSVC __debugbreak intrinsic -> compiler trap on clang/gcc.
inline void __debugbreak() { __builtin_trap(); }

// (Win32 mmsystem timer stubs -- timeBeginPeriod / timeEndPeriod /
// timeGetDevCaps / timeSetEvent / timeKillEvent / TIMECAPS / MMRESULT /
// LPTIMECALLBACK -- were removed once Timer Control was rewritten on
// std::thread + std::chrono. Add them back if a Windows-only header
// pulled into a portable TU starts referencing them.)

// Win32 CRITICAL_SECTION / Event stubs. These are still referenced
// from a handful of non-portable translation units that haven't been
// SDL3-ified yet (notably input.cpp's Win32 hook path); once those
// land they can go too.
struct CRITICAL_SECTION { int _stub; };
inline void InitializeCriticalSection(CRITICAL_SECTION*) {}
inline void DeleteCriticalSection(CRITICAL_SECTION*) {}
inline void EnterCriticalSection(CRITICAL_SECTION*) {}
inline void LeaveCriticalSection(CRITICAL_SECTION*) {}
inline BOOL TryEnterCriticalSection(CRITICAL_SECTION*) { return 1; }
#ifndef WAIT_OBJECT_0
#define WAIT_OBJECT_0     0x00000000L
#define WAIT_ABANDONED    0x00000080L
#define WAIT_ABANDONED_0  0x00000080L
#define WAIT_TIMEOUT      0x00000102L
#define WAIT_FAILED       0xFFFFFFFFL
#define INFINITE          0xFFFFFFFFL
#endif
inline HANDLE CreateEvent(void*, BOOL, BOOL, const char*) { return (HANDLE)1; }
inline BOOL   SetEvent(HANDLE)   { return 1; }
inline BOOL   ResetEvent(HANDLE) { return 1; }
inline DWORD  WaitForSingleObject(HANDLE, DWORD) { return WAIT_TIMEOUT; }
inline DWORD  WaitForMultipleObjectsEx(DWORD, const HANDLE*, BOOL, DWORD, BOOL) { return WAIT_TIMEOUT; }
inline DWORD  WaitForMultipleObjects(DWORD, const HANDLE*, BOOL, DWORD) { return WAIT_TIMEOUT; }
inline DWORD  GetCurrentThreadId() { return 1; }
typedef union { LONGLONG QuadPart; struct { DWORD LowPart; LONG HighPart; }; } LARGE_INTEGER;
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* freq) {
    if (freq) freq->QuadPart = 1000000;
    return 1;
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER* count) {
    auto t = std::chrono::steady_clock::now().time_since_epoch();
    if (count) count->QuadPart = (LONGLONG)std::chrono::duration_cast<std::chrono::microseconds>(t).count();
    return 1;
}
inline BOOL   CloseHandle(HANDLE) { return 1; }
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);
inline HANDLE CreateThread(void*, size_t, LPTHREAD_START_ROUTINE, LPVOID, DWORD, DWORD*) { return (HANDLE)0; }
inline void   YieldProcessor() { /* no-op */ }
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef WINAPI
#define WINAPI
#endif

// Win32 process heap stubs -- route through malloc/calloc/free.
#ifndef HEAP_ZERO_MEMORY
#define HEAP_ZERO_MEMORY 0x00000008
#endif
inline HANDLE GetProcessHeap() { return (HANDLE)1; }
inline LPVOID HeapAlloc(HANDLE, DWORD flags, size_t size) {
    return (flags & HEAP_ZERO_MEMORY) ? std::calloc(1, size) : std::malloc(size);
}
inline BOOL HeapFree(HANDLE, DWORD, LPVOID ptr) { std::free(ptr); return 1; }

// (Win32 file I/O stubs CreateFile / ReadFile / WriteFile /
// SetFilePointer / GetFileSize / GetFileAttributes were removed in
// Phase 2 -- the legacy LibraryDataBase.cpp that needed them is now
// a stub, since bfVFS handles SLF reading on every platform.)

typedef int HFILE;
#ifndef HFILE_ERROR
#define HFILE_ERROR ((HFILE)-1)
#endif
#ifndef SEM_FAILCRITICALERRORS
#define SEM_FAILCRITICALERRORS 0x0001
#define SEM_NOGPFAULTERRORBOX  0x0002
#define SEM_NOOPENFILEERRORBOX 0x8000
#endif
inline UINT SetErrorMode(UINT) { return 0; }

// Win32 wsprintf/wsprintfA are narrow printf-into-buffer with no
// length check. JA2 only uses the narrow form for debug strings, so
// alias to sprintf. (The wide form wsprintfW is unused.)
#define wsprintfA sprintf
#define wsprintf  sprintf

// (Win32 virtual-memory + MEMORYSTATUS stubs were removed -- MemMan
// no longer routes locked allocations through VirtualAlloc/VirtualLock
// and MemGetFree returns 0 directly without GlobalMemoryStatus.)

// MSVC _splitpath -- gate at call site, not in compat header (cleaner
// than wrapping POSIX dirname/basename which have different semantics).
// Callers wrap in #ifdef _WIN32.
inline DWORD FormatMessageA(DWORD, const void*, DWORD, DWORD, char* buf, DWORD bufSize, void*) {
    if (buf && bufSize > 0) buf[0] = '\0';
    return 0;
}
#define FormatMessage FormatMessageA

// Win32 GetPrivateProfileString stub. Returns 0 (no value) and
// writes the default string into the buffer. Real INI handling is
// done by INIReader; the legacy Win32 path is only used in a few
// places that fall back to defaults when the call returns nothing.
inline DWORD GetPrivateProfileStringA(const char*, const char*,
                                      const char* def, char* out,
                                      DWORD outSize, const char*) {
    if (out && outSize > 0) {
        size_t n = def ? std::strlen(def) : 0;
        if (n >= outSize) n = outSize - 1;
        if (n > 0) std::memcpy(out, def, n);
        out[n] = '\0';
        return (DWORD)n;
    }
    return 0;
}
#define GetPrivateProfileString GetPrivateProfileStringA
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

// MSVC-specific double-underscore synonyms still used in a couple of
// places (e.g. TileEngine/Smell.cpp). These don't collide with stdlib
// names, so the macro form is fine.
#ifndef __min
#define __min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef __max
#define __max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifdef __cplusplus
// Global `::min` / `::max` template overloads that handle mixed integer
// types via std::common_type. JA2 source has thousands of bare
// `min(int_literal, sized_int)` calls that won't compile against
// std::min<T>(T const&, T const&) because deduction fails on
// dissimilar argument types. These overloads pick up those cases via
// ADL/unqualified lookup; for same-type calls in code that does
// `using namespace std;`, std::min<T> is more specialized and wins
// overload resolution, so it remains the preferred path.
//
// Function-style `#define min/max` macros are intentionally NOT defined
// because they corrupt `min(...)` / `max(...)` inside
// libstdc++'s own template code (sstream/istream/hashtable/specfun/...)
// on Linux clang.
template <typename A, typename B>
constexpr auto min(A const& a, B const& b)
    -> typename std::common_type<A, B>::type
{
    using T = typename std::common_type<A, B>::type;
    return T(a) < T(b) ? T(a) : T(b);
}
template <typename A, typename B>
constexpr auto max(A const& a, B const& b)
    -> typename std::common_type<A, B>::type
{
    using T = typename std::common_type<A, B>::type;
    return T(a) > T(b) ? T(a) : T(b);
}
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

    // MSVC vs POSIX wide-printf swap '%s' and '%S' semantics:
    //   MSVC:   %s = wchar_t*, %S = char*
    //   POSIX:  %s = char*,    %S = wchar_t*
    // Legacy JA2 was written under MSVC conventions; ~all format
    // strings use %s for wchar_t args. Translate to the unambiguous
    // %ls (wide) / %hs (narrow) forms which mean the same on both
    // platforms so the legacy call sites keep working:
    //   %s -> %ls (force wide)
    //   %S -> %hs (force narrow)
    // Returns true if the format was rewritten, false otherwise.
    // 'out' must have at least (wcslen(fmt) + count_of_%s%S + 1) wide
    // chars of space; for safety the call sites use a 1024-wchar buffer.
    inline void msvc_wfmt_to_posix(const wchar_t* fmt, wchar_t* out, size_t outN)
    {
        size_t o = 0;
        for (size_t i = 0; fmt[i] && o + 2 < outN; ++i) {
            if (fmt[i] == L'%') {
                // copy run of flags/width up to the conversion specifier
                size_t j = i;
                out[o++] = L'%';
                ++j;
                // skip flags / width / precision / length modifiers
                while (fmt[j] && !((fmt[j] >= L'a' && fmt[j] <= L'z') ||
                                   (fmt[j] >= L'A' && fmt[j] <= L'Z')) &&
                       o + 1 < outN) {
                    out[o++] = fmt[j++];
                }
                if (!fmt[j]) { i = j - 1; continue; }
                wchar_t conv = fmt[j];
                if (conv == L's' && o + 1 < outN) {
                    out[o++] = L'l'; out[o++] = L's';   // %s -> %ls (wide)
                } else if (conv == L'S' && o + 1 < outN) {
                    out[o++] = L'h'; out[o++] = L's';   // %S -> %hs (narrow)
                } else {
                    out[o++] = conv;
                }
                i = j;
            } else {
                out[o++] = fmt[i];
            }
        }
        out[(o < outN) ? o : outN - 1] = 0;
    }
}
// Portable swprintf that pre-translates MSVC %s/%S to POSIX-
// unambiguous %ls/%hs before calling ::swprintf.
namespace sgp_compat {
    inline int swprintf_msvc(wchar_t* buf, size_t n, const wchar_t* fmt, ...) {
        wchar_t fixed[1024];
        msvc_wfmt_to_posix(fmt, fixed, 1024);
        va_list args;
        va_start(args, fmt);
        int r = ::vswprintf(buf, n, fixed, args);
        va_end(args);
        return r;
    }
}
#define swprintf(buf, ...) ::sgp_compat::swprintf_msvc((buf), ::sgp_compat::arrsize(buf), __VA_ARGS__)

// vswprintf has the same MSVC 3-arg / POSIX 4-arg split. Solved as a
// template overload at global scope so call sites with array buffers
// pick this up automatically. Pointer-buffer sites fail to compile,
// same as the swprintf macro. Also pre-translates the MSVC %s/%S
// convention to POSIX-unambiguous %ls/%hs so legacy format strings
// (authored under MSVC) keep their semantics.
template<size_t N>
inline int vswprintf(wchar_t (&buf)[N], const wchar_t* fmt, va_list args) {
    wchar_t fixed[1024];
    ::sgp_compat::msvc_wfmt_to_posix(fmt, fixed, 1024);
    return ::vswprintf(buf, N, fixed, args);
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
