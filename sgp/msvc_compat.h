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

#include <strings.h>  // strcasecmp, strncasecmp
#include <wchar.h>    // wcscasecmp on glibc; <wctype.h> on macOS
#include <cstdint>

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

#endif // !_WIN32

#endif // _SGP_MSVC_COMPAT_H
