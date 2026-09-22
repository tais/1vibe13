#include "Multiplayer/CoopCampaignGroups.h"
#include <cstdio>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
CoopCampaignGroups Fixture()
{
	CoopCampaignGroups value;
	value.sessionEpoch = 0x0102030405060708ull; value.revision = 9; value.available = true;
	value.groupCount = 2; value.memberCount = 3;
	value.groups[0] = {{2, 0x11223344}, 9, 1, 0, false, false, 0, 0, 1, 0, 0, 0, 0, 0, 2};
	value.groups[1] = {{255, 77}, 9, 2, 1, true, true, 10, 2, 4, 12, 2, 12345, 89, 2, 1};
	value.members[0] = {{0, 12}, 255, -128}; value.members[1] = {{263, 13}, 3, 127};
	value.members[2] = {{65534, 99}, 246, 0};
	return value;
}
void Protocol()
{
	auto original = Fixture(); CoopCampaignGroupsBytes bytes; std::size_t size = 0;
	CHECK(EncodeCoopCampaignGroups(original, bytes, size) && size == 144, "bounded group fixture encodes");
	CHECK(bytes[0] == 'J' && bytes[3] == 'G' && bytes[4] == static_cast<std::uint8_t>(CurrentProtocolVersion) && bytes[5] == static_cast<std::uint8_t>(CurrentProtocolVersion >> 8) &&
		bytes[8] == 8 && bytes[15] == 1 && bytes[32] == 2 && bytes[36] == 0x44 && bytes[39] == 0x11 &&
		bytes[104] == 0 && bytes[120] == 255 && bytes[136] == 128, "explicit little-endian vector and signed assignment endpoints");
	CoopCampaignGroups decoded;
	CHECK(DecodeCoopCampaignGroups(bytes.data(), size, decoded) && SameCoopCampaignGroups(original, decoded), "round trip exact identities and all fields");
	for (std::size_t length = 0; length < size; ++length)
		CHECK(!DecodeCoopCampaignGroups(bytes.data(), length, decoded) && SameCoopCampaignGroups(original, decoded), "every truncation preserves output");
	CHECK(!DecodeCoopCampaignGroups(nullptr, size, decoded) && !DecodeCoopCampaignGroups(bytes.data(), size + 1, decoded) &&
		!DecodeCoopCampaignGroups(bytes.data(), bytes.size() + 1, decoded), "null, trailing data and oversized wire rejected");
	for (const std::size_t offset : {0u, 4u, 5u, 6u, 28u, 29u, 30u, 31u, 33u, 34u, 35u, 47u, 50u, 51u, 105u, 111u})
	{
		auto bad = bytes; bad[offset] ^= 0x80;
		CHECK(!DecodeCoopCampaignGroups(bad.data(), size, decoded) && SameCoopCampaignGroups(original, decoded), "wrong header/version or nonzero reserved bytes rejected transactionally");
	}
	for (unsigned fault = 0; fault < 19; ++fault)
	{
		auto bad = original;
		switch (fault)
		{
		case 0: bad.sessionEpoch = 0; break;
		case 1: bad.revision = 0; break;
		case 2: bad.available = false; break;
		case 3: bad.groupCount = 256; break;
		case 4: bad.memberCount = 257; break;
		case 5: bad.groups[0].id = {}; break;
		case 6: bad.groups[1].id.slot = 2; break;
		case 7: bad.groups[0].x = 0; break;
		case 8: bad.groups[0].y = 17; break;
		case 9: bad.groups[0].z = 4; break;
		case 10: bad.groups[0].arrivalMinutes = 1; break;
		case 11: bad.groups[1].nextY = 3; break;
		case 12: bad.groups[1].destinationY = 0; break;
		case 13: bad.groups[1].firstMember = 1; break;
		case 14: bad.groups[1].memberCount = 0; break;
		case 15: bad.members[0].actor = {}; break;
		case 16: bad.members[1].actor.slot = 0; break;
		case 17: bad.members[2].actor.slot = 0; break;
		case 18: --bad.memberCount; break;
		}
		auto untouched = bytes; std::size_t untouchedSize = 999;
		CHECK(!EncodeCoopCampaignGroups(bad, untouched, untouchedSize) && untouched == bytes && untouchedSize == 999,
			"invalid native projections cannot alter encoded output");
	}
	for (const auto offset : {24u, 26u})
	{
		auto bad = bytes; bad[offset] = 255; bad[offset + 1] = 255;
		CHECK(!DecodeCoopCampaignGroups(bad.data(), size, decoded), "wire counts checked before record access");
	}
	for (std::size_t at = 0; at < size; ++at)
		for (unsigned bit = 0; bit < 8; ++bit)
		{
			auto changed = bytes; changed[at] ^= static_cast<std::uint8_t>(1u << bit);
			auto output = original;
			if (DecodeCoopCampaignGroups(changed.data(), size, output))
			{
				CoopCampaignGroupsBytes canonical; std::size_t canonicalSize = 0;
				CHECK(EncodeCoopCampaignGroups(output, canonical, canonicalSize) && canonicalSize == size && canonical == changed,
					"every accepted single-bit mutation has one canonical representation");
			}
			else CHECK(SameCoopCampaignGroups(output, original), "every rejected single-bit mutation preserves output");
		}
	CoopCampaignGroups maximum; maximum.sessionEpoch = 1; maximum.revision = 1; maximum.available = true;
	maximum.groupCount = 255; maximum.memberCount = 256;
	for (unsigned i = 0; i < 255; ++i)
		maximum.groups[i] = {{static_cast<std::uint8_t>(i + 1), i + 1}, 1, 16, 3, false, false, 0, 0, 0, 0, 0, 0, 0,
			static_cast<std::uint16_t>(i), static_cast<std::uint16_t>(i == 254 ? 2 : 1)};
	for (unsigned i = 0; i < 256; ++i) maximum.members[i].actor = {static_cast<std::uint16_t>(i), i + 1};
	CHECK(EncodeCoopCampaignGroups(maximum, bytes, size) && size == MaximumCoopCampaignGroupsWireSize && size == 12288 &&
		DecodeCoopCampaignGroups(bytes.data(), size, decoded) && SameCoopCampaignGroups(maximum, decoded), "maximum native group/member populations fit fixed callback capacity");
	maximum = {}; maximum.sessionEpoch = 1; maximum.revision = 2;
	CHECK(EncodeCoopCampaignGroups(maximum, bytes, size) && size == 32 && DecodeCoopCampaignGroups(bytes.data(), size, decoded) &&
		!decoded.available && !decoded.groupCount && !decoded.memberCount, "unavailable explicitly clears every old record");
}
void Ledger()
{
	CoopCampaignGroupsLedger ledger;
	CHECK(!ledger.observe(Fixture()) && !ledger.beginSession(0) && ledger.beginSession(77) && !ledger.beginSession(88), "session lifecycle is explicit");
	auto value = Fixture();
	CHECK(ledger.observe(value) && ledger.value().sessionEpoch == 77 && ledger.value().revision == 1, "ledger owns stamps");
	CHECK(ledger.observe(value) && ledger.value().revision == 1, "unchanged captures coalesce");
	++value.members[0].actor.incarnation;
	CHECK(ledger.observe(value) && ledger.value().revision == 2, "slot reuse changes identity and revision");
	auto invalid = value; invalid.groups[0].memberCount = 0;
	CHECK(!ledger.observe(invalid) && ledger.value().revision == 2 && ledger.value().available, "invalid capture cannot replace coherent data");
	CHECK(ledger.observe({}) && !ledger.value().available && ledger.value().revision == 3 && !ledger.value().groupCount,
		"explicit transition/unavailable is a new full replacement");
	CHECK(ledger.observe(value) && ledger.value().revision == 4, "recovery cannot reuse pre-transition revision");
	ledger.clear(); CHECK(ledger.beginSession(99) && ledger.observe(value) && ledger.value().revision == 1, "new session starts a fresh lineage");
}
}
int main() { Protocol(); Ledger(); return failures ? 1 : 0; }
