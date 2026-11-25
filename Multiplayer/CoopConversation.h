#ifndef MULTIPLAYER_COOP_CONVERSATION_H
#define MULTIPLAYER_COOP_CONVERSATION_H

#include "CoopSessionProtocol.h"
#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
// The native talk panel on the authority, replicated to every campaign-ready
// peer: who talks with whom and the current subtitle. One conversation exists
// at a time, exactly like the stock single talk panel.
inline constexpr const char* CoopConversationMessageName = "coop.conversation";
inline constexpr std::size_t CoopConversationQuoteUnits = 240;
inline constexpr std::size_t CoopConversationWireSize = 64 + CoopConversationQuoteUnits * 2;

struct CoopConversation
{
	std::uint64_t sessionEpoch = 0;
	std::uint64_t revision = 0;
	// Runtime-scoped identity of the open talk panel; zero means none.
	std::uint32_t conversation = 0;
	std::uint8_t npcProfile = 0;
	std::uint8_t mercProfile = 0;
	TacticalEntityId npc{};
	TacticalEntityId merc{};
	// The peer whose request opened it; zero when native gameplay opened it.
	PeerIdentity initiator{};
	// The NPC (or merc) is still speaking or more lines are queued.
	bool speaking = false;
	// The panel's other party is itself an NPC (a scripted NPC-to-NPC scene,
	// e.g. Fatima handing Miguel the letter): any player may only close it.
	bool npcSource = false;
	std::uint16_t quoteLength = 0;
	std::array<std::uint16_t, CoopConversationQuoteUnits> quote{};
};
using CoopConversationBytes = std::array<std::uint8_t, CoopConversationWireSize>;

inline bool ValidCoopConversation(const CoopConversation& value) noexcept
{
	if (!value.sessionEpoch || !value.revision || value.quoteLength > CoopConversationQuoteUnits) return false;
	for (std::size_t i = value.quoteLength; i < value.quote.size(); ++i) if (value.quote[i]) return false;
	for (std::size_t i = 0; i < value.quoteLength; ++i) if (!value.quote[i]) return false;
	if (!value.conversation)
		return !value.npcProfile && !value.mercProfile && value.npc == TacticalEntityId{} &&
			value.merc == TacticalEntityId{} && IsZero(value.initiator) && !value.speaking && !value.npcSource &&
			!value.quoteLength;
	return value.npcProfile && value.npc.valid() && value.merc.valid() && value.npc != value.merc;
}

inline bool SameCoopConversationContent(const CoopConversation& a, const CoopConversation& b) noexcept
{
	return a.conversation == b.conversation && a.npcProfile == b.npcProfile && a.mercProfile == b.mercProfile &&
		a.npc == b.npc && a.merc == b.merc && a.initiator == b.initiator && a.speaking == b.speaking &&
		a.npcSource == b.npcSource &&
		a.quoteLength == b.quoteLength && a.quote == b.quote;
}

inline bool EncodeCoopConversation(const CoopConversation& value, CoopConversationBytes& output) noexcept
{
	if (!ValidCoopConversation(value)) return false;
	CoopConversationBytes bytes{};
	const auto put = [&](std::size_t at, std::uint64_t number, unsigned count) {
		for (unsigned i = 0; i < count; ++i) bytes[at + i] = static_cast<std::uint8_t>(number >> (8 * i));
	};
	bytes[0] = 'J'; bytes[1] = '2'; bytes[2] = 'C'; bytes[3] = 'V';
	put(4, CurrentProtocolVersion, 2);
	bytes[6] = 1; bytes[7] = value.speaking ? 1 : 0;
	put(8, value.sessionEpoch, 8); put(16, value.revision, 8); put(24, value.conversation, 4);
	bytes[28] = value.npcProfile; bytes[29] = value.mercProfile; put(30, value.quoteLength, 2);
	put(32, value.npc.slot, 2); put(34, value.merc.slot, 2);
	put(36, value.npc.incarnation, 4); put(40, value.merc.incarnation, 4);
	for (std::size_t i = 0; i < value.initiator.size(); ++i) bytes[44 + i] = value.initiator[i];
	bytes[60] = value.npcSource ? 1 : 0;
	for (std::size_t i = 0; i < value.quoteLength; ++i) put(64 + 2 * i, value.quote[i], 2);
	output = bytes;
	return true;
}

inline bool DecodeCoopConversation(const std::uint8_t* bytes, std::size_t size, CoopConversation& output) noexcept
{
	if (!bytes || size != CoopConversationWireSize || bytes[0] != 'J' || bytes[1] != '2' || bytes[2] != 'C' ||
		bytes[3] != 'V' || bytes[6] != 1 || bytes[7] > 1) return false;
	if (bytes[60] > 1) return false;
	for (std::size_t i = 61; i < 64; ++i) if (bytes[i]) return false;
	const auto get = [&](std::size_t at, unsigned count) {
		std::uint64_t number = 0;
		for (unsigned i = 0; i < count; ++i) number |= static_cast<std::uint64_t>(bytes[at + i]) << (8 * i);
		return number;
	};
	if (get(4, 2) != CurrentProtocolVersion) return false;
	CoopConversation value;
	value.speaking = bytes[7] != 0;
	value.npcSource = bytes[60] != 0;
	value.sessionEpoch = get(8, 8); value.revision = get(16, 8);
	value.conversation = static_cast<std::uint32_t>(get(24, 4));
	value.npcProfile = bytes[28]; value.mercProfile = bytes[29];
	value.quoteLength = static_cast<std::uint16_t>(get(30, 2));
	value.npc = {static_cast<std::uint16_t>(get(32, 2)), static_cast<std::uint32_t>(get(36, 4))};
	value.merc = {static_cast<std::uint16_t>(get(34, 2)), static_cast<std::uint32_t>(get(40, 4))};
	for (std::size_t i = 0; i < value.initiator.size(); ++i) value.initiator[i] = bytes[44 + i];
	if (value.quoteLength > CoopConversationQuoteUnits) return false;
	for (std::size_t i = 0; i < CoopConversationQuoteUnits; ++i)
	{
		const auto unit = static_cast<std::uint16_t>(get(64 + 2 * i, 2));
		if (i < value.quoteLength) value.quote[i] = unit;
		else if (unit) return false;
	}
	if (!ValidCoopConversation(value)) return false;
	output = value;
	return true;
}

// Authority-side revisioned observation, like the economy ledgers.
class CoopConversationLedger
{
public:
	bool beginSession(std::uint64_t epoch) noexcept
	{
		if (!epoch || value_.sessionEpoch) return false;
		value_ = {}; value_.sessionEpoch = epoch; value_.revision = 1;
		return true;
	}
	void clear() noexcept { value_ = {}; }
	const CoopConversation& value() const noexcept { return value_; }
	bool observe(CoopConversation captured) noexcept
	{
		if (!value_.sessionEpoch) return false;
		captured.sessionEpoch = value_.sessionEpoch; captured.revision = value_.revision;
		if (!ValidCoopConversation(captured)) return false;
		if (!SameCoopConversationContent(value_, captured))
		{
			if (captured.revision == UINT64_MAX) return false;
			++captured.revision;
		}
		value_ = captured;
		return true;
	}
private:
	CoopConversation value_;
};
}
#endif
