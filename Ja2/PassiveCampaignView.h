#ifndef JA2_PASSIVE_CAMPAIGN_VIEW_H
#define JA2_PASSIVE_CAMPAIGN_VIEW_H

#include <array>
#include <cstdint>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

constexpr std::uint32_t PassiveCampaignViewSection = 0x57564350u; // "PCVW"
constexpr std::size_t PassiveCampaignViewHeaderBytes = 20;
constexpr std::size_t PassiveCampaignProfileBytes = 130;
constexpr std::size_t PassiveCampaignMaximumProfiles = 255;
enum class PassiveCampaignWorldKind : std::uint8_t { Strategic = 1, Tactical = 2 };

struct PassiveCampaignProfile
{
	std::uint16_t id = 0;
	// Unicode scalar values, zero terminated and zero padded; no native wchar ABI.
	std::array<std::uint32_t, 32> nickname{};
	friend bool operator==(const PassiveCampaignProfile& a, const PassiveCampaignProfile& b)
	{ return a.id == b.id && a.nickname == b.nickname; }
};

struct PassiveCampaignView
{
	PassiveCampaignWorldKind worldKind = PassiveCampaignWorldKind::Strategic;
	std::uint8_t sectorX = 0, sectorY = 0, sectorZ = 0;
	std::uint16_t rows = 0, columns = 0;
	std::uint32_t worldMinutes = 0;
	std::vector<PassiveCampaignProfile> profiles;
	friend bool operator==(const PassiveCampaignView& a, const PassiveCampaignView& b)
	{
		return a.worldKind == b.worldKind && a.sectorX == b.sectorX && a.sectorY == b.sectorY &&
			a.sectorZ == b.sectorZ && a.rows == b.rows && a.columns == b.columns &&
			a.worldMinutes == b.worldMinutes && a.profiles == b.profiles;
	}
};

inline bool IsPassiveCampaignDisplayScalar(std::uint32_t value) noexcept
{
	return value >= 32 && value != 127 && value <= 0x10ffff &&
		!(value >= 0xd800 && value <= 0xdfff);
}

inline bool ValidatePassiveCampaignView(const PassiveCampaignView& view) noexcept
{
	if (view.profiles.size() > PassiveCampaignMaximumProfiles) return false;
	if (view.worldKind == PassiveCampaignWorldKind::Strategic)
	{
		if (view.sectorX || view.sectorY || view.sectorZ || view.rows || view.columns) return false;
	}
	else if (view.worldKind != PassiveCampaignWorldKind::Tactical ||
		!view.sectorX || view.sectorX > 16 || !view.sectorY || view.sectorY > 16 || view.sectorZ > 3 ||
		!view.rows || view.rows > 2000 || !view.columns || view.columns > 2000) return false;
	for (std::size_t index = 0; index < view.profiles.size(); ++index)
	{
		const auto& profile = view.profiles[index];
		if (profile.id >= PassiveCampaignMaximumProfiles ||
			(index && view.profiles[index - 1].id >= profile.id) || !profile.nickname[0]) return false;
		bool ended = false;
		std::size_t utf16Units = 0;
		for (auto scalar : profile.nickname)
		{
			if (!scalar) ended = true;
			else
			{
				if (ended || !IsPassiveCampaignDisplayScalar(scalar)) return false;
				utf16Units += scalar > 0xffff ? 2 : 1;
				if (utf16Units >= profile.nickname.size()) return false;
			}
		}
		if (!ended) return false;
	}
	return true;
}

inline bool EncodePassiveCampaignView(const PassiveCampaignView& view,
	std::vector<std::uint8_t>& output) noexcept
{
	if (!ValidatePassiveCampaignView(view)) return false;
	try
	{
		std::vector<std::uint8_t> bytes;
		bytes.reserve(PassiveCampaignViewHeaderBytes + view.profiles.size() * PassiveCampaignProfileBytes);
		const auto put = [&](std::uint32_t value, unsigned width) {
			for (unsigned i = 0; i < width; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
		};
		put(1, 4); put(static_cast<std::uint8_t>(view.worldKind), 1);
		put(view.sectorX, 1); put(view.sectorY, 1); put(view.sectorZ, 1);
		put(view.rows, 2); put(view.columns, 2); put(view.worldMinutes, 4);
		put(view.profiles.size(), 2); put(0, 2);
		for (const auto& profile : view.profiles)
		{
			put(profile.id, 2);
			for (auto scalar : profile.nickname) put(scalar, 4);
		}
		output = std::move(bytes);
		return true;
	}
	catch (...) { return false; }
}

inline bool DecodePassiveCampaignView(const std::vector<std::uint8_t>& bytes,
	PassiveCampaignView& output) noexcept
{
	if (bytes.size() < PassiveCampaignViewHeaderBytes || bytes.size() >
		PassiveCampaignViewHeaderBytes + PassiveCampaignMaximumProfiles * PassiveCampaignProfileBytes) return false;
	std::size_t offset = 0;
	const auto get = [&](unsigned width) {
		std::uint32_t value = 0;
		for (unsigned i = 0; i < width; ++i) value |= std::uint32_t(bytes[offset++]) << (8 * i);
		return value;
	};
	if (get(4) != 1) return false;
	try
	{
		PassiveCampaignView view;
		view.worldKind = static_cast<PassiveCampaignWorldKind>(get(1));
		view.sectorX = get(1); view.sectorY = get(1); view.sectorZ = get(1);
		view.rows = get(2); view.columns = get(2); view.worldMinutes = get(4);
		const auto count = get(2);
		if (get(2) != 0 || count > PassiveCampaignMaximumProfiles ||
			bytes.size() != PassiveCampaignViewHeaderBytes + count * PassiveCampaignProfileBytes) return false;
		view.profiles.resize(count);
		for (auto& profile : view.profiles)
		{
			profile.id = get(2);
			for (auto& scalar : profile.nickname) scalar = get(4);
		}
		if (!ValidatePassiveCampaignView(view)) return false;
		output = std::move(view);
		return true;
	}
	catch (...) { return false; }
}

inline bool PassiveCampaignNickname(const PassiveCampaignView& view, std::uint16_t id,
	std::array<wchar_t, 32>& output) noexcept
{
	output = {};
	for (const auto& profile : view.profiles)
	{
		if (profile.id != id) continue;
		std::size_t offset = 0;
		for (auto scalar : profile.nickname)
		{
			if (!scalar) return offset != 0;
			if (!IsPassiveCampaignDisplayScalar(scalar)) break;
			if constexpr (sizeof(wchar_t) == 2)
			{
				if (scalar > 0xffff)
				{
					if (offset + 2 >= output.size()) break;
					scalar -= 0x10000;
					output[offset++] = static_cast<wchar_t>(0xd800 + (scalar >> 10));
					output[offset++] = static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff));
					continue;
				}
			}
			if (offset + 1 >= output.size()) break;
			output[offset++] = static_cast<wchar_t>(scalar);
		}
		break;
	}
	output = {};
	return false;
}

enum class PassiveCampaignPreparationResult : std::uint8_t
{ Ready, MissingView, InvalidCheckpoint, WorldActive, WorldMinutesMismatch, RollbackFailure };

class GameContext;
bool CapturePassiveCampaignView(PassiveCampaignView& output) noexcept;
// Strict compatibility validation only. Never loads the domain or restores RNG,
// packages or frame state. Output changes only after the guard rolls back cleanly.
PassiveCampaignPreparationResult PreparePassiveCampaignCheckpoint(GameContext& context, const std::string& path,
	std::uint32_t expectedWorldMinutes, PassiveCampaignView& output) noexcept;

#endif
