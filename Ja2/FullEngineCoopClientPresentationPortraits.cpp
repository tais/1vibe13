#include "FullEngineCoopClientPresentationPortraits.h"

#include "FullEngineCoopClientPresentationPassiveUi.h"

#include <array>
#include <cstdio>
#include <cstring>

bool BuildFullEngineCoopClientPortraitAssetPath(
	const TacticalPortraitSnapshot& portrait, bool baseVariant,
	char* output, std::size_t capacity) noexcept
{
	if (!output || !IsCanonicalTacticalPortrait(portrait) ||
		portrait.family == TacticalPortraitFamily::Absent) return false;
	const char* family = portrait.family == TacticalPortraitFamily::Faces ? "FACES" : "IMPFACES";
	const char* camouflage = "";
	if (!baseVariant)
		switch (portrait.camouflage)
		{
			case TacticalPortraitCamouflage::None: break;
			case TacticalPortraitCamouflage::Wood: camouflage = "WoodCamo\\"; break;
			case TacticalPortraitCamouflage::Urban: camouflage = "UrbanCamo\\"; break;
			case TacticalPortraitCamouflage::Desert: camouflage = "DesertCamo\\"; break;
			case TacticalPortraitCamouflage::Snow: camouflage = "SnowCamo\\"; break;
		}
	std::array<char, FullEngineCoopClientPortraitPathBytes> path{};
	const int length = std::snprintf(path.data(), path.size(), "%s\\%s%02u.sti",
		family, camouflage, unsigned(portrait.faceIndex));
	if (length < 0 || static_cast<std::size_t>(length) >= path.size() ||
		static_cast<std::size_t>(length) >= capacity) return false;
	std::memcpy(output, path.data(), static_cast<std::size_t>(length) + 1);
	return true;
}

bool IsValidFullEngineCoopClientPortraitFrame(
	const std::uint8_t* bytes, std::size_t size,
	std::uint16_t width, std::uint16_t height) noexcept
{
	if (!bytes || !width || width > 48 || !height || height > 43 ||
		!size || size > 48u * 43u * 2u + 43u) return false;
	std::size_t cursor = 0;
	for (std::uint16_t row = 0; row < height; ++row)
	{
		std::uint16_t column = 0;
		bool ended = false;
		while (cursor < size)
		{
			const std::uint8_t header = bytes[cursor++];
			if (!header) { ended = true; break; }
			const std::uint16_t run = header & 0x7fu;
			if (!run || run > width - column) return false;
			if (!(header & 0x80u))
			{
				if (run > size - cursor) return false;
				cursor += run;
			}
			column += run;
		}
		if (!ended || column != width) return false;
	}
	return cursor == size;
}

FullEngineCoopClientPresentationPortraits::FullEngineCoopClientPresentationPortraits(
	FullEngineCoopClientPortraitServices services) noexcept : services_(services) {}

FullEngineCoopClientPresentationPortraits::~FullEngineCoopClientPresentationPortraits()
{
	teardown();
}

void FullEngineCoopClientPresentationPortraits::release(Entry& entry) noexcept
{
	if (entry.loaded && services_.release) services_.release(entry.handle, services_.context);
	entry = {};
}

void FullEngineCoopClientPresentationPortraits::teardown() noexcept
{
	for (auto& entry : entries_) release(entry);
	worldGeneration_ = 0;
}

bool FullEngineCoopClientPresentationPortraits::synchronize(
	std::uint64_t worldGeneration, const FullEngineCoopClientVisiblePortrait* visible,
	std::size_t count) noexcept
{
	if (!worldGeneration || count > entries_.size() || (count && !visible))
	{ teardown(); return false; }
	for (std::size_t index = 0; index < count; ++index)
	{
		if (!visible[index].actor.valid() || !IsCanonicalTacticalPortrait(visible[index].portrait))
		{ teardown(); return false; }
		for (std::size_t prior = 0; prior < index; ++prior)
			if (visible[prior].actor == visible[index].actor) { teardown(); return false; }
	}
	if (worldGeneration_ != worldGeneration) teardown();
	worldGeneration_ = worldGeneration;
	for (std::size_t index = 0; index < entries_.size(); ++index)
	{
		auto& entry = entries_[index];
		if (index >= count) { release(entry); continue; }
		const auto& wanted = visible[index];
		if (entry.identity.actor == wanted.actor && entry.identity.portrait == wanted.portrait)
			continue;
		release(entry);
		entry.identity = wanted; // Also records absence/load failure for this identity.
		if (!services_.load || !services_.release || !services_.blit ||
			wanted.portrait.family == TacticalPortraitFamily::Absent) continue;
		std::array<char, FullEngineCoopClientPortraitPathBytes> path{};
		if (!BuildFullEngineCoopClientPortraitAssetPath(wanted.portrait, false,
			path.data(), path.size())) continue;
		entry.loaded = services_.load(path.data(), entry.handle, services_.context);
		if (!entry.loaded && wanted.portrait.camouflage != TacticalPortraitCamouflage::None &&
			BuildFullEngineCoopClientPortraitAssetPath(wanted.portrait, true, path.data(), path.size()))
			entry.loaded = services_.load(path.data(), entry.handle, services_.context);
	}
	return true;
}

bool FullEngineCoopClientPresentationPortraits::render(
	std::size_t visibleSlot, const FullEngineCoopClientPassiveUiRect& face) noexcept
{
	if (!worldGeneration_ || visibleSlot >= entries_.size() || !face.valid() ||
		face.right - face.left != 48 || face.bottom - face.top != 43) return false;
	auto& entry = entries_[visibleSlot];
	if (!entry.loaded || !services_.blit) return false;
	if (services_.blit(entry.handle, face, services_.context)) return true;
	// Cache a post-load validation/blit failure without retaining an unusable
	// native handle or retrying the same failed resource every frame.
	const auto identity = entry.identity;
	release(entry);
	entry.identity = identity;
	return false;
}
