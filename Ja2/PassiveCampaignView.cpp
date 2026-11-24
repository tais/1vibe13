#include "PassiveCampaignView.h"

#include "DedicatedCampaignSaveBridge.h"
#include "GameContext.h"
#include "Game Clock.h"
#include "RuntimeSaveState.h"
#include "Soldier Profile.h"
#include "TacticalWorldAdapter.h"
#include "worlddef.h"
#include "strategicmap.h"
#include <iterator>

bool CapturePassiveCampaignView(PassiveCampaignView& output) noexcept
{
	static_assert(NUM_PROFILES == PassiveCampaignMaximumProfiles, "profile domain changed");
	static_assert(WORLD_ROWS_MAX == 2000 && WORLD_COLS_MAX == 2000, "map dimension domain changed");
	try
	{
		PassiveCampaignView view;
		view.worldMinutes = GetWorldTotalMin();
		if (IsJa2TacticalWorldLoaded())
		{
			if (gWorldSectorX < 1 || gWorldSectorX > 16 || gWorldSectorY < 1 || gWorldSectorY > 16 ||
				gbWorldSectorZ < 0 || gbWorldSectorZ > 3 || WORLD_ROWS < 1 || WORLD_ROWS > WORLD_ROWS_MAX ||
				WORLD_COLS < 1 || WORLD_COLS > WORLD_COLS_MAX) return false;
			view.worldKind = PassiveCampaignWorldKind::Tactical;
			view.sectorX = gWorldSectorX; view.sectorY = gWorldSectorY; view.sectorZ = gbWorldSectorZ;
			view.rows = WORLD_ROWS; view.columns = WORLD_COLS;
		}
		for (std::uint16_t id = 0; id < NUM_PROFILES; ++id)
		{
			const auto& name = gMercProfiles[id].zNickname;
			if (!name[0]) continue;
			PassiveCampaignProfile profile;
			profile.id = id;
			std::size_t write = 0;
			bool ended = false;
			for (std::size_t read = 0; read < std::size(name); ++read)
			{
				std::uint32_t scalar = static_cast<std::uint32_t>(name[read]);
				if (!scalar) { ended = true; break; }
				if constexpr (sizeof(CHAR16) == 2)
				{
					if (scalar >= 0xd800 && scalar <= 0xdbff)
					{
						if (++read >= std::size(name)) return false;
						const auto second = static_cast<std::uint32_t>(name[read]);
						if (second < 0xdc00 || second > 0xdfff) return false;
						scalar = 0x10000 + ((scalar - 0xd800) << 10) + second - 0xdc00;
					}
				}
				if (!IsPassiveCampaignDisplayScalar(scalar) || write + 1 >= profile.nickname.size()) return false;
				profile.nickname[write++] = scalar;
			}
			if (!ended) return false;
			view.profiles.push_back(profile);
		}
		if (!ValidatePassiveCampaignView(view)) return false;
		output = std::move(view);
		return true;
	}
	catch (...) { return false; }
}

PassiveCampaignPreparationResult PreparePassiveCampaignCheckpoint(GameContext& context, const std::string& path,
	std::uint32_t expectedWorldMinutes, PassiveCampaignView& output) noexcept
{
	using Result = PassiveCampaignPreparationResult;
	if (IsJa2TacticalWorldLoaded()) return Result::WorldActive;
	auto guard = BeginRuntimeLoadExecution(context, RuntimeSavePolicy::DedicatedDeterministic);
	auto prepared = PrepareRuntimeLoad(context, path, guard);
	const auto rolledBack = guard.rollback();
	if (!rolledBack) return Result::RollbackFailure;
	if (!prepared) return Result::InvalidCheckpoint;
	if (!prepared.passiveView) return Result::MissingView;
	if (prepared.passiveView->worldMinutes != expectedWorldMinutes) return Result::WorldMinutesMismatch;
	output = std::move(*prepared.passiveView);
	return Result::Ready;
}

PassiveCampaignPreparationResult PreparePassiveDedicatedCampaignCheckpoint(DedicatedCampaignSlot slot,
	std::uint32_t expectedWorldMinutes, PassiveCampaignView& output) noexcept
{
	if (slot != DedicatedCampaignSlot::A && slot != DedicatedCampaignSlot::B)
		return PassiveCampaignPreparationResult::InvalidCheckpoint;
	return PreparePassiveCampaignCheckpoint(GetGameContext(), DedicatedCampaignLogicalScratch(slot),
		expectedWorldMinutes, output);
}
