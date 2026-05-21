#ifndef SGP_SAVE_SERIALIZER_H
#define SGP_SAVE_SERIALIZER_H

#include "types.h"      // UINT*/INT*/CHAR*/BOOLEAN/HWFILE

// Portable, explicit, little-endian save serialization (save-format v2).
//
// Replaces the legacy "dump the struct's bytes" save format, which is not
// portable: wchar_t/CHAR16 is 2 bytes on Win32 but 4 on macOS/Linux, struct
// padding is ABI-dependent, and `long`/`enum` sizes vary. SaveWriter/SaveReader
// define the on-disk byte layout explicitly and identically on every platform:
//
//   * every multi-byte integer is written little-endian, byte by byte (so the
//     result is independent of host endianness);
//   * wide strings are 16-bit on disk and widened to wchar_t in memory (`wstr`);
//   * floats/doubles go through their bit pattern as u32/u64.
//
// See docs/SAVE_FORMAT.md. Each saved struct gets an explicit
// Serialize(SaveWriter&) / Deserialize(SaveReader&) listing its fields, so the
// format never depends on in-memory layout.

class SaveWriter
{
public:
	explicit SaveWriter(HWFILE f) : hFile(f), ok(true) {}
	bool good() const { return ok; }

	void u8 (UINT8  v);
	void u16(UINT16 v);
	void u32(UINT32 v);
	void u64(UINT64 v);
	void i8 (INT8   v) { u8 ((UINT8 )v); }
	void i16(INT16  v) { u16((UINT16)v); }
	void i32(INT32  v) { u32((UINT32)v); }
	void i64(INT64  v) { u64((UINT64)v); }
	void f32(float  v);
	void f64(double v);
	void boolean(BOOLEAN v) { u8(v ? 1 : 0); }

	// n wide chars -> n * 2 bytes (each char narrowed to 16-bit, LE).
	void wstr(const CHAR16* p, UINT32 n);
	// fixed-size byte field (CHAR8[]/UINT8[]); written verbatim.
	void str8(const CHAR8* p, UINT32 n) { bytes(p, n); }
	void bytes(const void* p, UINT32 n);
	// emit n zero bytes (reserved space / dropped-field placeholders).
	void skip(UINT32 n);

private:
	HWFILE hFile;
	bool   ok;
	void raw(const void* p, UINT32 n);
};

class SaveReader
{
public:
	explicit SaveReader(HWFILE f) : hFile(f), ok(true) {}
	bool good() const { return ok; }

	UINT8  u8 ();
	UINT16 u16();
	UINT32 u32();
	UINT64 u64();
	INT8   i8 () { return (INT8 )u8 (); }
	INT16  i16() { return (INT16)u16(); }
	INT32  i32() { return (INT32)u32(); }
	INT64  i64() { return (INT64)u64(); }
	float  f32();
	double f64();
	BOOLEAN boolean() { return u8() ? TRUE : FALSE; }

	// reads n * 2 bytes from disk, widening each 16-bit char into p[0..n).
	void wstr(CHAR16* p, UINT32 n);
	void str8(CHAR8* p, UINT32 n) { bytes(p, n); }
	void bytes(void* p, UINT32 n);
	void skip(UINT32 n);

private:
	HWFILE hFile;
	bool   ok;
	void raw(void* p, UINT32 n);
};

#endif // SGP_SAVE_SERIALIZER_H
