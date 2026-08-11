#include "FileMan.h"
#include "SaveLoadGame.h"
#include "SaveSerializer.h"
#include "Strategic Path Types.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace
{
struct MemoryFile
{
	std::vector<UINT8> bytes;
	std::size_t cursor = 0;
};

HWFILE Handle(MemoryFile& file)
{
	return reinterpret_cast<HWFILE>(&file);
}

int failures = 0;

void Check(bool condition, const char* contract)
{
	if (condition)
		std::printf("ok    %s\n", contract);
	else
	{
		++failures;
		std::printf("FAIL  %s\n", contract);
	}
}

void AppendU16(std::vector<UINT8>& bytes, UINT16 value)
{
	bytes.push_back(static_cast<UINT8>(value));
	bytes.push_back(static_cast<UINT8>(value >> 8));
}

void AppendU32(std::vector<UINT8>& bytes, UINT32 value)
{
	for (unsigned shift = 0; shift != 32; shift += 8)
		bytes.push_back(static_cast<UINT8>(value >> shift));
}

void AppendU64(std::vector<UINT8>& bytes, UINT64 value)
{
	for (unsigned shift = 0; shift != 64; shift += 8)
		bytes.push_back(static_cast<UINT8>(value >> shift));
}

template<class T>
UINT32 Float32Bits(T value)
{
	static_assert(sizeof(T) == sizeof(UINT32), "f32 fixture width drifted");
	UINT32 bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

template<class T>
UINT64 Float64Bits(T value)
{
	static_assert(sizeof(T) == sizeof(UINT64), "f64 fixture width drifted");
	UINT64 bits = 0;
	std::memcpy(&bits, &value, sizeof(bits));
	return bits;
}

void AppendBytes(std::vector<UINT8>& destination, const void* source,
	std::size_t size)
{
	const UINT8* first = static_cast<const UINT8*>(source);
	destination.insert(destination.end(), first, first + size);
}

void WritePrimitiveSection(SaveWriter& writer)
{
	writer.u8(0xA5u);
	writer.i8(std::numeric_limits<INT8>::min());
	writer.i8(-1);
	writer.i8(std::numeric_limits<INT8>::max());
	writer.u16(0x1234u);
	writer.i16(std::numeric_limits<INT16>::min());
	writer.i16(-1);
	writer.i16(std::numeric_limits<INT16>::max());
	writer.u32(0x89ABCDEFu);
	writer.i32(std::numeric_limits<INT32>::min());
	writer.i32(-1);
	writer.i32(std::numeric_limits<INT32>::max());
	writer.u64(UINT64_C(0x0123456789ABCDEF));
	writer.i64(std::numeric_limits<INT64>::min());
	writer.i64(-1);
	writer.i64(std::numeric_limits<INT64>::max());
	writer.f32(-13.25f);
	writer.f32(-0.0f);
	writer.f64(0.125);
	writer.f64(-2.5);
	writer.boolean(FALSE);
	writer.boolean(TRUE);
	const CHAR16 wide[] = {L'\u00E9', L'\u03A9', L'\u4F60', L'\0'};
	writer.wstr(wide, 4);
	const CHAR8 narrow[] = {'J', 'A', '2', '\0', '!'};
	writer.str8(narrow, 5);
	const UINT8 opaque[] = {0x00u, 0x7Fu, 0x80u, 0xFFu};
	writer.bytes(opaque, 4);
	writer.skip(3);
}

const std::vector<UINT8>& PrimitiveGolden()
{
	static const std::vector<UINT8> golden{
		0xA5,
		0x80, 0xFF, 0x7F,
		0x34, 0x12,
		0x00, 0x80, 0xFF, 0xFF, 0xFF, 0x7F,
		0xEF, 0xCD, 0xAB, 0x89,
		0x00, 0x00, 0x00, 0x80,
		0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0x7F,
		0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
		0x00, 0x00, 0x54, 0xC1,
		0x00, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x3F,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xC0,
		0x00, 0x01,
		0xE9, 0x00, 0xA9, 0x03, 0x60, 0x4F, 0x00, 0x00,
		'J', 'A', '2', 0x00, '!',
		0x00, 0x7F, 0x80, 0xFF,
		0x00, 0x00, 0x00
	};
	return golden;
}

bool ReadPrimitiveSection(SaveReader& reader)
{
	bool matches =
		reader.u8() == 0xA5u &&
		reader.i8() == std::numeric_limits<INT8>::min() &&
		reader.i8() == -1 &&
		reader.i8() == std::numeric_limits<INT8>::max() &&
		reader.u16() == 0x1234u &&
		reader.i16() == std::numeric_limits<INT16>::min() &&
		reader.i16() == -1 &&
		reader.i16() == std::numeric_limits<INT16>::max() &&
		reader.u32() == 0x89ABCDEFu &&
		reader.i32() == std::numeric_limits<INT32>::min() &&
		reader.i32() == -1 &&
		reader.i32() == std::numeric_limits<INT32>::max() &&
		reader.u64() == UINT64_C(0x0123456789ABCDEF) &&
		reader.i64() == std::numeric_limits<INT64>::min() &&
		reader.i64() == -1 &&
		reader.i64() == std::numeric_limits<INT64>::max();
	const float firstFloat = reader.f32();
	const float secondFloat = reader.f32();
	const double firstDouble = reader.f64();
	const double secondDouble = reader.f64();
	matches = matches &&
		Float32Bits(firstFloat) == UINT32_C(0xC1540000) &&
		Float32Bits(secondFloat) == UINT32_C(0x80000000) &&
		Float64Bits(firstDouble) == UINT64_C(0x3FC0000000000000) &&
		Float64Bits(secondDouble) == UINT64_C(0xC004000000000000) &&
		reader.boolean() == FALSE && reader.boolean() == TRUE;
	CHAR16 wide[4]{};
	CHAR8 narrow[5]{};
	UINT8 opaque[4]{};
	reader.wstr(wide, 4);
	reader.str8(narrow, 5);
	reader.bytes(opaque, 4);
	reader.skip(3);
	const CHAR16 expectedWide[] = {L'\u00E9', L'\u03A9', L'\u4F60', L'\0'};
	const CHAR8 expectedNarrow[] = {'J', 'A', '2', '\0', '!'};
	const UINT8 expectedOpaque[] = {0x00u, 0x7Fu, 0x80u, 0xFFu};
	return matches &&
		std::equal(std::begin(wide), std::end(wide), std::begin(expectedWide)) &&
		std::equal(std::begin(narrow), std::end(narrow), std::begin(expectedNarrow)) &&
		std::equal(std::begin(opaque), std::end(opaque), std::begin(expectedOpaque)) &&
		reader.good();
}

SAVED_GAME_HEADER MakeHeaderFixture()
{
	SAVED_GAME_HEADER header{};
	header.uiSavedGameVersion = 1003;
	const CHAR8 version[] = {'a', 'l', 'p', 'h', 'a', '-', 'g', 'o', 'l', 'd'};
	std::copy(std::begin(version), std::end(version),
		header.zGameVersionNumber);
	header.sSavedGameDesc[0] = L'\u00E9';
	header.sSavedGameDesc[1] = L'\u4F60';
	header.uiFlags = 0x89ABCDEFu;
#ifdef CRIPPLED_VERSION
	for (std::size_t i = 0; i < sizeof(header.ubCrippleFiller); ++i)
		header.ubCrippleFiller[i] = static_cast<UINT8>(0xE0u + i);
#endif
	header.uiDay = 0x01020304u;
	header.ubHour = 23;
	header.ubMin = 59;
	header.sSectorX = std::numeric_limits<INT16>::min();
	header.sSectorY = std::numeric_limits<INT16>::max();
	header.bSectorZ = std::numeric_limits<INT8>::min();
	header.ubNumOfMercsOnPlayersTeam = 0xBEEFu;
	header.iCurrentBalance = std::numeric_limits<INT32>::min();
	header.uiCurrentScreen = 0x76543210u;
	header.fAlternateSector = TRUE;
	header.fWorldLoaded = FALSE;
	header.ubLoadScreenID = 0xE1u;
	UINT8* options = reinterpret_cast<UINT8*>(&header.sInitialGameOptions);
	for (std::size_t i = 0; i < sizeof(header.sInitialGameOptions); ++i)
		options[i] = static_cast<UINT8>((i * 37u + 11u) & 0xFFu);
	header.uiRandom = 0x0BADF00Du;
	for (std::size_t i = 0; i < sizeof(header.ubFiller); ++i)
		header.ubFiller[i] = static_cast<UINT8>(i ^ 0x5Au);
	return header;
}

std::vector<UINT8> HeaderGolden(const SAVED_GAME_HEADER& header)
{
	std::vector<UINT8> golden;
	AppendU32(golden, header.uiSavedGameVersion);
	AppendBytes(golden, header.zGameVersionNumber,
		sizeof(header.zGameVersionNumber));
	for (CHAR16 character : header.sSavedGameDesc)
		AppendU16(golden, static_cast<UINT16>(character));
	AppendU32(golden, header.uiFlags);
#ifdef CRIPPLED_VERSION
	AppendBytes(golden, header.ubCrippleFiller,
		sizeof(header.ubCrippleFiller));
#endif
	AppendU32(golden, header.uiDay);
	golden.push_back(header.ubHour);
	golden.push_back(header.ubMin);
	AppendU16(golden, static_cast<UINT16>(header.sSectorX));
	AppendU16(golden, static_cast<UINT16>(header.sSectorY));
	golden.push_back(static_cast<UINT8>(header.bSectorZ));
	AppendU16(golden, header.ubNumOfMercsOnPlayersTeam);
	AppendU32(golden, static_cast<UINT32>(header.iCurrentBalance));
	AppendU32(golden, header.uiCurrentScreen);
	golden.push_back(1);
	golden.push_back(0);
	golden.push_back(header.ubLoadScreenID);
	AppendBytes(golden, &header.sInitialGameOptions,
		sizeof(header.sInitialGameOptions));
	AppendU32(golden, header.uiRandom);
	AppendBytes(golden, header.ubFiller, sizeof(header.ubFiller));
	return golden;
}

bool HeaderMatches(const SAVED_GAME_HEADER& left,
	const SAVED_GAME_HEADER& right)
{
	bool matches = left.uiSavedGameVersion == right.uiSavedGameVersion &&
		std::equal(std::begin(left.zGameVersionNumber),
			std::end(left.zGameVersionNumber),
			std::begin(right.zGameVersionNumber)) &&
		std::equal(std::begin(left.sSavedGameDesc),
			std::end(left.sSavedGameDesc),
			std::begin(right.sSavedGameDesc)) &&
		left.uiFlags == right.uiFlags && left.uiDay == right.uiDay &&
		left.ubHour == right.ubHour && left.ubMin == right.ubMin &&
		left.sSectorX == right.sSectorX && left.sSectorY == right.sSectorY &&
		left.bSectorZ == right.bSectorZ &&
		left.ubNumOfMercsOnPlayersTeam == right.ubNumOfMercsOnPlayersTeam &&
		left.iCurrentBalance == right.iCurrentBalance &&
		left.uiCurrentScreen == right.uiCurrentScreen &&
		left.fAlternateSector == right.fAlternateSector &&
		left.fWorldLoaded == right.fWorldLoaded &&
		left.ubLoadScreenID == right.ubLoadScreenID &&
		std::memcmp(&left.sInitialGameOptions, &right.sInitialGameOptions,
			sizeof(left.sInitialGameOptions)) == 0 &&
		left.uiRandom == right.uiRandom &&
		std::equal(std::begin(left.ubFiller), std::end(left.ubFiller),
			std::begin(right.ubFiller));
#ifdef CRIPPLED_VERSION
	matches = matches &&
		std::equal(std::begin(left.ubCrippleFiller),
			std::end(left.ubCrippleFiller),
			std::begin(right.ubCrippleFiller));
#endif
	return matches;
}

struct VisitorFixture
{
	UINT8 u8 = 0x5Au;
	UINT16 u16 = 0x1234u;
	UINT32 u32 = 0x89ABCDEFu;
	UINT64 u64 = UINT64_C(0x0123456789ABCDEF);
	INT8 i8 = -2;
	INT16 i16 = -3;
	INT32 i32 = -4;
	INT64 i64 = -5;
	float f32 = 0.5f;
	double f64 = -1.0;
	BOOLEAN boolean = TRUE;
	signed long legacyLong = -2147483647L - 1L;
	CHAR16 wide[3] = {L'V', L'\u03A9', L'\0'};
	CHAR8 narrow[4] = {'F', 'L', 'D', '\0'};
	UINT8 opaque[3] = {0x10u, 0x80u, 0xFEu};
	int* runtimePointer = nullptr;
};

template<class Ar>
void XferVisitorFixture(Ar& ar, VisitorFixture& fixture)
{
	ar.u8(fixture.u8); ar.u16(fixture.u16);
	ar.u32(fixture.u32); ar.u64(fixture.u64);
	ar.i8(fixture.i8); ar.i16(fixture.i16);
	ar.i32(fixture.i32); ar.i64(fixture.i64);
	ar.f32(fixture.f32); ar.f64(fixture.f64);
	ar.boolean(fixture.boolean); ar.slong(fixture.legacyLong);
	ar.wstr(fixture.wide, 3); ar.str8(fixture.narrow, 4);
	ar.bytes(fixture.opaque, 3);
	ar.ptr(fixture.runtimePointer);
	ar.retiredPtr();
}

std::vector<UINT8> VisitorGolden(const VisitorFixture& fixture)
{
	std::vector<UINT8> golden;
	golden.push_back(fixture.u8);
	AppendU16(golden, fixture.u16);
	AppendU32(golden, fixture.u32);
	AppendU64(golden, fixture.u64);
	golden.push_back(static_cast<UINT8>(fixture.i8));
	AppendU16(golden, static_cast<UINT16>(fixture.i16));
	AppendU32(golden, static_cast<UINT32>(fixture.i32));
	AppendU64(golden, static_cast<UINT64>(fixture.i64));
	AppendU32(golden, Float32Bits(fixture.f32));
	AppendU64(golden, Float64Bits(fixture.f64));
	golden.push_back(1);
	AppendU32(golden, static_cast<UINT32>(static_cast<INT32>(
		fixture.legacyLong)));
	for (CHAR16 character : fixture.wide)
		AppendU16(golden, static_cast<UINT16>(character));
	AppendBytes(golden, fixture.narrow, sizeof(fixture.narrow));
	AppendBytes(golden, fixture.opaque, sizeof(fixture.opaque));
	return golden;
}

bool VisitorMatches(const VisitorFixture& left, const VisitorFixture& right)
{
	return left.u8 == right.u8 && left.u16 == right.u16 &&
		left.u32 == right.u32 && left.u64 == right.u64 &&
		left.i8 == right.i8 && left.i16 == right.i16 &&
		left.i32 == right.i32 && left.i64 == right.i64 &&
		Float32Bits(left.f32) == Float32Bits(right.f32) &&
		Float64Bits(left.f64) == Float64Bits(right.f64) &&
		left.boolean == right.boolean &&
		left.legacyLong == right.legacyLong &&
		std::equal(std::begin(left.wide), std::end(left.wide),
			std::begin(right.wide)) &&
		std::equal(std::begin(left.narrow), std::end(left.narrow),
			std::begin(right.narrow)) &&
		std::equal(std::begin(left.opaque), std::end(left.opaque),
			std::begin(right.opaque));
}

std::vector<UINT8> PathGolden(const std::array<PathSt, 2>& paths)
{
	std::vector<UINT8> golden;
	AppendU16(golden, static_cast<UINT16>(paths.size()));
	for (const PathSt& path : paths)
	{
		AppendU32(golden, path.uiSectorId);
		AppendU32(golden, path.uiEta);
		golden.push_back(path.fSpeed ? 1 : 0);
	}
	return golden;
}

constexpr UINT32 FixtureMagic = UINT32_C(0x3146474A); // "JGF1"
constexpr UINT16 FixtureVersion = 1;
constexpr UINT16 FixtureSectionCount = 4;
constexpr UINT32 PrimitiveSection = UINT32_C(0x4D495250); // "PRIM"
constexpr UINT32 HeaderSection = UINT32_C(0x52444853);    // "SHDR"
constexpr UINT32 PathSection = UINT32_C(0x48544150);      // "PATH"
constexpr UINT32 VisitorSection = UINT32_C(0x44534956);   // "VISD"

void WriteSectionHeader(SaveWriter& writer, UINT32 type, std::size_t size)
{
	writer.u32(type);
	writer.u32(static_cast<UINT32>(size));
}
}

// SaveSerializer.cpp calls the production FileMan surface. This executable
// supplies an in-memory implementation so the golden contract has no VFS or
// installed-game-data dependency.
BOOLEAN FileWrite(HWFILE handle, const void* source, UINT32 size,
	UINT32* written)
{
	if (written) *written = 0;
	MemoryFile* file = reinterpret_cast<MemoryFile*>(handle);
	if (!file || (!source && size != 0)) return FALSE;
	if (file->cursor + size > file->bytes.size())
		file->bytes.resize(file->cursor + size);
	if (size != 0)
		std::memcpy(file->bytes.data() + file->cursor, source, size);
	file->cursor += size;
	if (written) *written = size;
	return TRUE;
}

BOOLEAN FileRead(HWFILE handle, PTR destination, UINT32 size, UINT32* read)
{
	if (read) *read = 0;
	MemoryFile* file = reinterpret_cast<MemoryFile*>(handle);
	if (!file || (!destination && size != 0)) return FALSE;
	const std::size_t available = file->cursor < file->bytes.size()
		? file->bytes.size() - file->cursor : 0;
	const std::size_t copied = std::min<std::size_t>(size, available);
	if (copied != 0)
		std::memcpy(destination, file->bytes.data() + file->cursor, copied);
	file->cursor += copied;
	if (read) *read = static_cast<UINT32>(copied);
	return TRUE;
}

int main()
{
	static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
		"SaveSerializer f32 requires an IEEE-754 binary32 host type");
	static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
		"SaveSerializer f64 requires an IEEE-754 binary64 host type");
	static_assert(sizeof(GAME_OPTIONS) == 515,
		"the scalar-only game-options save block changed width");
	static_assert(GAME_VERSION_LENGTH == 16,
		"the portable save-header version field changed width");
	static_assert(SIZE_OF_SAVE_GAME_DESC == 128,
		"the portable save-header description changed width");

	MemoryFile primitiveFile;
	{
		SaveWriter writer(Handle(primitiveFile));
		WritePrimitiveSection(writer);
		Check(writer.good(), "primitive golden fixture writes completely");
	}
	Check(primitiveFile.bytes == PrimitiveGolden(),
		"every SaveSerializer primitive has exact little-endian golden bytes");
	Check(PrimitiveGolden().size() == 106 &&
		PrimitiveGolden()[86] == 0xE9 && PrimitiveGolden()[90] == 0x60,
		"non-ASCII wstr uses fixed 16-bit code units");
	primitiveFile.cursor = 0;
	{
		SaveReader reader(Handle(primitiveFile));
		Check(ReadPrimitiveSection(reader) &&
			primitiveFile.cursor == primitiveFile.bytes.size(),
			"primitive signed edges, floating bits, strings, bytes, and skip round trip exactly");
	}

	SAVED_GAME_HEADER savedHeader = MakeHeaderFixture();
	const std::vector<UINT8> headerGolden = HeaderGolden(savedHeader);
#ifdef CRIPPLED_VERSION
	constexpr std::size_t ExpectedHeaderBytes = 1337;
#else
	constexpr std::size_t ExpectedHeaderBytes = 1317;
#endif
	Check(headerGolden.size() == ExpectedHeaderBytes,
		"portable save header retains its exact schema width");
	std::array<PathSt, 2> savedPaths{{
		{0x01020304u, 0xA0B0C0D0u, FALSE, nullptr, nullptr},
		{0xFFFFFFFFu, 0u, TRUE, nullptr, nullptr}
	}};
	savedPaths[0].pNext = &savedPaths[1];
	savedPaths[1].pPrev = &savedPaths[0];
	const std::vector<UINT8> pathGolden = PathGolden(savedPaths);
	int runtimeObject = 7;
	VisitorFixture savedVisitor;
	savedVisitor.runtimePointer = &runtimeObject;
	const std::vector<UINT8> visitorGolden = VisitorGolden(savedVisitor);
	Check(pathGolden.size() == 20 && visitorGolden.size() == 60,
		"representative path and field-visitor records retain exact widths");

	MemoryFile fixtureFile;
	{
		SaveWriter writer(Handle(fixtureFile));
		writer.u32(FixtureMagic);
		writer.u16(FixtureVersion);
		writer.u16(FixtureSectionCount);
		WriteSectionHeader(writer, PrimitiveSection, PrimitiveGolden().size());
		WritePrimitiveSection(writer);
		WriteSectionHeader(writer, HeaderSection, headerGolden.size());
		SaveFieldWriter fields(writer);
		XferSaveGameHeaderFields(fields, savedHeader);
		WriteSectionHeader(writer, PathSection, pathGolden.size());
		writer.u16(static_cast<UINT16>(savedPaths.size()));
		for (PathSt& path : savedPaths) XferPathNodeFields(fields, path);
		WriteSectionHeader(writer, VisitorSection, visitorGolden.size());
		XferVisitorFixture(fields, savedVisitor);
		Check(writer.good(), "canonical multi-section fixture writes completely");
	}

	std::vector<UINT8> completeGolden;
	AppendU32(completeGolden, FixtureMagic);
	AppendU16(completeGolden, FixtureVersion);
	AppendU16(completeGolden, FixtureSectionCount);
	for (const auto& section : std::array<std::pair<UINT32,
		const std::vector<UINT8>*>, 4>{{
		{PrimitiveSection, &PrimitiveGolden()},
		{HeaderSection, &headerGolden},
		{PathSection, &pathGolden},
		{VisitorSection, &visitorGolden}
	}})
	{
		AppendU32(completeGolden, section.first);
		AppendU32(completeGolden,
			static_cast<UINT32>(section.second->size()));
		completeGolden.insert(completeGolden.end(),
			section.second->begin(), section.second->end());
	}
	Check(fixtureFile.bytes == completeGolden,
		"production save-header and path-node visitors match canonical bytes");
	Check(completeGolden.size() == ExpectedHeaderBytes + 226,
		"canonical four-section save fixture retains its exact total width");

	fixtureFile.cursor = 0;
	bool fixtureMatches = false;
	{
		SaveReader reader(Handle(fixtureFile));
		fixtureMatches = reader.u32() == FixtureMagic &&
			reader.u16() == FixtureVersion &&
			reader.u16() == FixtureSectionCount;
		fixtureMatches = fixtureMatches &&
			reader.u32() == PrimitiveSection &&
			reader.u32() == PrimitiveGolden().size() &&
			ReadPrimitiveSection(reader);
		SAVED_GAME_HEADER loadedHeader{};
		SaveFieldReader fields(reader);
		fixtureMatches = fixtureMatches &&
			reader.u32() == HeaderSection &&
			reader.u32() == headerGolden.size();
		XferSaveGameHeaderFields(fields, loadedHeader);
		fixtureMatches = fixtureMatches &&
			HeaderMatches(savedHeader, loadedHeader) &&
			reader.u32() == PathSection &&
			reader.u32() == pathGolden.size() && reader.u16() == 2;
		std::array<PathSt, 2> loadedPaths{{
			{0, 0, FALSE, &savedPaths[0], &savedPaths[0]},
			{0, 0, FALSE, &savedPaths[0], &savedPaths[0]}
		}};
		for (PathSt& path : loadedPaths) XferPathNodeFields(fields, path);
		fixtureMatches = fixtureMatches &&
			loadedPaths[0].uiSectorId == savedPaths[0].uiSectorId &&
			loadedPaths[0].uiEta == savedPaths[0].uiEta &&
			loadedPaths[0].fSpeed == savedPaths[0].fSpeed &&
			loadedPaths[1].uiSectorId == savedPaths[1].uiSectorId &&
			loadedPaths[1].uiEta == savedPaths[1].uiEta &&
			loadedPaths[1].fSpeed == savedPaths[1].fSpeed &&
			loadedPaths[0].pNext == &savedPaths[0] &&
			loadedPaths[1].pPrev == &savedPaths[0] &&
			reader.u32() == VisitorSection &&
			reader.u32() == visitorGolden.size();
		VisitorFixture loadedVisitor;
		loadedVisitor.runtimePointer = &runtimeObject;
		XferVisitorFixture(fields, loadedVisitor);
		fixtureMatches = fixtureMatches &&
			VisitorMatches(savedVisitor, loadedVisitor) &&
			loadedVisitor.runtimePointer == nullptr && reader.good() &&
			fixtureFile.cursor == fixtureFile.bytes.size();
	}
	Check(fixtureMatches,
		"canonical save fixture is emitted and consumed without installed game data");

	if (failures == 0)
		std::printf("all save serializer golden tests passed\n");
	return failures == 0 ? 0 : 1;
}
