#ifndef _SGP_BOUNDED_STRING_H
#define _SGP_BOUNDED_STRING_H

// Bounded string-copy helpers for the untrusted-input boundary (filenames, player
// /IMP names, multiplayer wire fields, screen-message text). Each truncates to fit
// and ALWAYS NUL-terminates, replacing the fragile manual "strncpy then [n]=0"
// pattern (and the unbounded strcpy/sprintf sinks that the MP revival made
// remotely reachable). Semantics mirror BSD strlcpy: copy up to dstSize-1 chars,
// always terminate; returns the source length (so truncation is detectable).
//
// Use sgp_strlcpy/sgp_wcslcpy at the receive-side sink, NOT for the ~hundreds of
// internal trusted copies.

#include <cstddef>
#include <cstring>
#include <cwchar>

inline size_t sgp_strlcpy(char* dst, const char* src, size_t dstSize)
{
	size_t srcLen = src ? std::strlen(src) : 0;
	if (dst && dstSize > 0)
	{
		size_t n = (srcLen < dstSize - 1) ? srcLen : dstSize - 1;
		if (src && n) std::memcpy(dst, src, n);
		dst[n] = '\0';
	}
	return srcLen;
}

inline size_t sgp_wcslcpy(wchar_t* dst, const wchar_t* src, size_t dstCount)
{
	size_t srcLen = src ? std::wcslen(src) : 0;
	if (dst && dstCount > 0)
	{
		size_t n = (srcLen < dstCount - 1) ? srcLen : dstCount - 1;
		if (src && n) std::wmemcpy(dst, src, n);
		dst[n] = L'\0';
	}
	return srcLen;
}

// Convenience for fixed-size arrays: deduces the element count so callers can't
// pass a wrong size. (Pointer-typed destinations must use the count form above.)
template <size_t N> inline size_t sgp_strlcpy(char (&dst)[N], const char* src)  { return sgp_strlcpy(dst, src, N); }
template <size_t N> inline size_t sgp_wcslcpy(wchar_t (&dst)[N], const wchar_t* src) { return sgp_wcslcpy(dst, src, N); }

#endif
