#include "SaveSerializer.h"
#include "FileMan.h"
#include <cstring>

// ---- SaveWriter -----------------------------------------------------------

void SaveWriter::raw(const void* p, UINT32 n)
{
	if (!ok || n == 0) return;
	UINT32 wrote = 0;
	if (!FileWrite(hFile, p, n, &wrote) || wrote != n) ok = false;
}

void SaveWriter::u8(UINT8 v) { raw(&v, 1); }

void SaveWriter::u16(UINT16 v)
{
	UINT8 b[2] = { (UINT8)(v & 0xFF), (UINT8)((v >> 8) & 0xFF) };
	raw(b, 2);
}

void SaveWriter::u32(UINT32 v)
{
	UINT8 b[4] = { (UINT8)(v), (UINT8)(v >> 8), (UINT8)(v >> 16), (UINT8)(v >> 24) };
	raw(b, 4);
}

void SaveWriter::u64(UINT64 v)
{
	UINT8 b[8];
	for (int i = 0; i < 8; ++i) b[i] = (UINT8)((v >> (8 * i)) & 0xFF);
	raw(b, 8);
}

void SaveWriter::f32(float v)
{
	UINT32 bits; std::memcpy(&bits, &v, 4); u32(bits);
}

void SaveWriter::f64(double v)
{
	UINT64 bits; std::memcpy(&bits, &v, 8); u64(bits);
}

void SaveWriter::wstr(const CHAR16* p, UINT32 n)
{
	for (UINT32 i = 0; i < n; ++i) u16((UINT16)p[i]);
}

void SaveWriter::bytes(const void* p, UINT32 n) { raw(p, n); }

void SaveWriter::skip(UINT32 n)
{
	UINT8 z = 0;
	for (UINT32 i = 0; i < n; ++i) raw(&z, 1);
}

// ---- SaveReader -----------------------------------------------------------

void SaveReader::raw(void* p, UINT32 n)
{
	if (!ok || n == 0) return;
	UINT32 got = 0;
	if (!FileRead(hFile, p, n, &got) || got != n) ok = false;
}

UINT8 SaveReader::u8() { UINT8 v = 0; raw(&v, 1); return v; }

UINT16 SaveReader::u16()
{
	UINT8 b[2] = {0,0}; raw(b, 2);
	return (UINT16)(b[0] | (b[1] << 8));
}

UINT32 SaveReader::u32()
{
	UINT8 b[4] = {0,0,0,0}; raw(b, 4);
	return (UINT32)b[0] | ((UINT32)b[1] << 8) | ((UINT32)b[2] << 16) | ((UINT32)b[3] << 24);
}

UINT64 SaveReader::u64()
{
	UINT8 b[8]; raw(b, 8);
	UINT64 v = 0;
	for (int i = 0; i < 8; ++i) v |= (UINT64)b[i] << (8 * i);
	return v;
}

float SaveReader::f32() { UINT32 bits = u32(); float v; std::memcpy(&v, &bits, 4); return v; }

double SaveReader::f64() { UINT64 bits = u64(); double v; std::memcpy(&v, &bits, 8); return v; }

void SaveReader::wstr(CHAR16* p, UINT32 n)
{
	for (UINT32 i = 0; i < n; ++i) p[i] = (CHAR16)u16();
}

void SaveReader::bytes(void* p, UINT32 n) { raw(p, n); }

void SaveReader::skip(UINT32 n)
{
	UINT8 dump;
	for (UINT32 i = 0; i < n; ++i) raw(&dump, 1);
}
