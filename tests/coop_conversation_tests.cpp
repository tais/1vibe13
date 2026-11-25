#include "CoopConversation.h"
#include <cstdio>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

CoopConversation Open()
{
	CoopConversation value;
	value.sessionEpoch = 7; value.revision = 3; value.conversation = 2;
	value.npcProfile = 101; value.mercProfile = 239;
	value.npc = {40, 9}; value.merc = {1, 2};
	value.initiator[0] = 5; value.speaking = true;
	const char16_t text[] = u"Why are you here?";
	for (std::size_t i = 0; text[i]; ++i) value.quote[value.quoteLength++] = text[i];
	return value;
}

void Codec()
{
	const auto open = Open();
	CoopConversationBytes bytes{};
	CHECK(EncodeCoopConversation(open, bytes) && bytes[0] == 'J' && bytes[3] == 'V' && bytes[7] == 1 &&
		bytes[24] == 2 && bytes[28] == 101 && bytes[29] == 239 && bytes[30] == 17 && bytes[44] == 5 &&
		bytes[64] == 'W' && bytes[65] == 0, "fixed little-endian conversation layout");
	CoopConversation decoded;
	CHECK(DecodeCoopConversation(bytes.data(), bytes.size(), decoded) && SameCoopConversationContent(decoded, open) &&
		decoded.revision == 3 && decoded.sessionEpoch == 7, "conversation roundtrips exactly");
	for (std::size_t size : {std::size_t(0), CoopConversationWireSize - 1, CoopConversationWireSize + 1})
		CHECK(!DecodeCoopConversation(bytes.data(), size, decoded), "only the exact width decodes");
	for (std::size_t at : {std::size_t(3), std::size_t(6), std::size_t(60), std::size_t(64 + 2 * 17)})
	{
		auto bad = bytes; bad[at] ^= 0x5a;
		CHECK(!DecodeCoopConversation(bad.data(), bad.size(), decoded), "magic, kind, reserved and quote tail are checked");
	}
	CoopConversation none; none.sessionEpoch = 7; none.revision = 1;
	CHECK(EncodeCoopConversation(none, bytes) && DecodeCoopConversation(bytes.data(), bytes.size(), decoded) &&
		!decoded.conversation, "no conversation encodes as an empty record");
	auto stray = none; stray.npcProfile = 101;
	CHECK(!EncodeCoopConversation(stray, bytes), "an empty record cannot carry participants");
	auto self = open; self.merc = self.npc;
	CHECK(!EncodeCoopConversation(self, bytes), "an NPC cannot talk to itself");
	auto hole = open; hole.quote[3] = 0;
	CHECK(!EncodeCoopConversation(hole, bytes), "no embedded terminator inside the quote length");
}

void Ledger()
{
	CoopConversationLedger ledger;
	CHECK(!ledger.observe({}), "no observation before a session");
	CHECK(ledger.beginSession(7) && !ledger.beginSession(8), "one session at a time");
	CHECK(ledger.observe({}) && ledger.value().revision == 1 && !ledger.value().conversation,
		"an empty observation keeps the first revision");
	auto open = Open(); open.sessionEpoch = 0; open.revision = 0;
	CHECK(ledger.observe(open) && ledger.value().revision == 2 && ledger.value().sessionEpoch == 7,
		"opening a conversation advances the revision and binds the session");
	CHECK(ledger.observe(open) && ledger.value().revision == 2, "unchanged content keeps the revision");
	open.speaking = false;
	CHECK(ledger.observe(open) && ledger.value().revision == 3, "speech ending is a change");
	CHECK(ledger.observe({}) && ledger.value().revision == 4 && !ledger.value().conversation, "closing is a change");
	auto invalid = Open(); invalid.npcProfile = 0;
	CHECK(!ledger.observe(invalid) && ledger.value().revision == 4, "invalid captures never replace the observation");
}
}

int main()
{
	Codec(); Ledger();
	if (!failures) std::puts("co-op conversation tests passed");
	return failures ? 1 : 0;
}
