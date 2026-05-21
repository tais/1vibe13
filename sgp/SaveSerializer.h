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

// ---------------------------------------------------------------------------
// Field-visitor adapters.
//
// For very large structs (MERCPROFILESTRUCT, SOLDIERTYPE) writing separate
// Save and Load field lists invites drift: one field added/reordered on only
// one side silently corrupts saves. These adapters expose the same by-
// reference method names over a SaveWriter or a SaveReader, so a single
// templated field list (`template<class Ar> void Xfer(Ar&, T&)`) serves both
// directions. Call Xfer with a SaveFieldWriter to save, a SaveFieldReader to
// load. `isLoading` lets a field list special-case the rare asymmetric spot.
class SaveFieldWriter
{
public:
	explicit SaveFieldWriter(SaveWriter& w_) : w(w_) {}
	void u8 (UINT8&  v) { w.u8 (v); }   void u16(UINT16& v) { w.u16(v); }
	void u32(UINT32& v) { w.u32(v); }   void u64(UINT64& v) { w.u64(v); }
	void i8 (INT8&   v) { w.i8 (v); }   void i16(INT16&  v) { w.i16(v); }
	void i32(INT32&  v) { w.i32(v); }   void i64(INT64&  v) { w.i64(v); }
	void f32(float&  v) { w.f32(v); }   void f64(double& v) { w.f64(v); }
	void boolean(BOOLEAN& v) { w.boolean(v); }
	// `long` is 32-bit on Win32 but 64-bit on macOS/Linux: pin it to 32 bits.
	void slong(signed long& v) { w.i32((INT32)v); }
	void wstr(CHAR16* p, UINT32 n) { w.wstr(p, n); }
	void str8(CHAR8*  p, UINT32 n) { w.str8(p, n); }
	void bytes(void*  p, UINT32 n) { w.bytes(p, n); }
	// runtime pointer: not persisted (writes nothing; reader clears it).
	template<class T> void ptr(T*& ) {}
	bool good() const { return w.good(); }
	static const bool isLoading = false;
private:
	SaveWriter& w;
};

class SaveFieldReader
{
public:
	explicit SaveFieldReader(SaveReader& r_) : r(r_) {}
	void u8 (UINT8&  v) { v = r.u8 (); }   void u16(UINT16& v) { v = r.u16(); }
	void u32(UINT32& v) { v = r.u32(); }   void u64(UINT64& v) { v = r.u64(); }
	void i8 (INT8&   v) { v = r.i8 (); }   void i16(INT16&  v) { v = r.i16(); }
	void i32(INT32&  v) { v = r.i32(); }   void i64(INT64&  v) { v = r.i64(); }
	void f32(float&  v) { v = r.f32(); }   void f64(double& v) { v = r.f64(); }
	void boolean(BOOLEAN& v) { v = r.boolean(); }
	void slong(signed long& v) { v = (signed long)(INT32)r.i32(); }
	void wstr(CHAR16* p, UINT32 n) { r.wstr(p, n); }
	void str8(CHAR8*  p, UINT32 n) { r.str8(p, n); }
	void bytes(void*  p, UINT32 n) { r.bytes(p, n); }
	// runtime pointer: was never meaningfully persisted; clear it on load.
	template<class T> void ptr(T*& p) { p = NULL; }
	bool good() const { return r.good(); }
	static const bool isLoading = true;
private:
	SaveReader& r;
};

#endif // SGP_SAVE_SERIALIZER_H
