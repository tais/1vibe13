#include "CampaignMercenaryArrivalMutators.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <type_traits>

namespace
{
	void Check(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}

	struct ArrivalMutationModel
	{
		std::array<std::uint32_t, 7> helicopterGrids =
			{{101, 102, 103, 104, 105, 106, 107}};
		std::uint32_t jerryGrid = 201;
		bool laptopQuest = false;
		bool includesJerry = false;
		bool jerryQuotes = false;
		bool helicopterCrash = false;
		bool helicopter = false;
		std::uint32_t offscreenGrid = 301;
	};

	void ApplyJerryMutation(
		ArrivalMutationModel& model,
		const CampaignMercenaryJerryMutation& mutation)
	{
		model.includesJerry = mutation.includesJerry;
		if (mutation.gridUpdate !=
			CampaignMercenaryJerryGridUpdate::Preserve)
		{
			model.jerryGrid = mutation.gridNo;
		}
	}

	constexpr bool LegacyInclusiveOrRange(
		std::uint8_t value, std::uint8_t lower,
		std::uint8_t upper) noexcept
	{
		return value >= lower || value <= upper;
	}

	constexpr bool LegacyArrivalCoordinatesAccepted(
		std::uint8_t x, std::uint8_t y) noexcept
	{
		return LegacyInclusiveOrRange(x, 1, 16) &&
			LegacyInclusiveOrRange(y, 1, 16);
	}
}

int main()
{
	using Slot = CampaignMercenaryHelicopterArrivalSlot;
	using GridUpdate = CampaignMercenaryJerryGridUpdate;
	using GridMutator = void (*)(Slot, std::uint32_t);
	using JerryGridMutator = void (*)(std::uint32_t);
	using BooleanMutator = void (*)(bool);
	using JerryMutator = void (*)(bool, std::int32_t);

	static_assert(std::is_same_v<
		std::underlying_type_t<Slot>, std::uint8_t>);
	static_assert(std::is_same_v<
		std::underlying_type_t<GridUpdate>, std::uint8_t>);
	static_assert(std::is_trivially_copyable_v<
		CampaignMercenaryJerryMutation>);
	static_assert(std::is_standard_layout_v<
		CampaignMercenaryJerryMutation>);
	static_assert(std::is_same_v<decltype(
		PlanCampaignMercenaryJerryMutation(false, 0)),
		CampaignMercenaryJerryMutation>);
	static_assert(std::is_same_v<decltype(
		&SetCampaignMercenaryHelicopterArrivalGrid), GridMutator>);
	static_assert(std::is_same_v<decltype(
		&SetCampaignMercenaryJerryArrivalGrid), JerryGridMutator>);
	static_assert(std::is_same_v<decltype(
		&SetCampaignMercenaryLaptopQuestEnabled), BooleanMutator>);
	static_assert(std::is_same_v<decltype(
		&ConfigureCampaignMercenaryJerry), JerryMutator>);
	static_assert(std::is_same_v<decltype(
		&SetCampaignMercenaryJerryQuotesEnabled), BooleanMutator>);
	static_assert(std::is_same_v<decltype(
		&SetCampaignMercenaryHelicopterCrashEnabled), BooleanMutator>);
	static_assert(std::is_same_v<decltype(
		&SetCampaignMercenaryHelicopterEnabled), BooleanMutator>);
	static_assert(std::is_same_v<decltype(
		&SetCampaignMercenaryOffscreenArrivalGrid), JerryGridMutator>);
	static_assert(CampaignMercenaryLegacyJerryGridNo == 15943);
	static_assert(static_cast<std::uint8_t>(Slot::First) == 0);
	static_assert(static_cast<std::uint8_t>(Slot::Second) == 1);
	static_assert(static_cast<std::uint8_t>(Slot::Third) == 2);
	static_assert(static_cast<std::uint8_t>(Slot::Fourth) == 3);
	static_assert(static_cast<std::uint8_t>(Slot::Fifth) == 4);
	static_assert(static_cast<std::uint8_t>(Slot::Sixth) == 5);
	static_assert(static_cast<std::uint8_t>(Slot::Seventh) == 6);

	const std::array<std::int32_t, 9> requestedGrids = {{
		std::numeric_limits<std::int32_t>::min(),
		-15943,
		-1,
		0,
		1,
		15943,
		65535,
		100000,
		std::numeric_limits<std::int32_t>::max()
	}};
	for (const bool includesJerry : {false, true})
	{
		for (const std::int32_t requestedGrid : requestedGrids)
		{
			const CampaignMercenaryJerryMutation mutation =
				PlanCampaignMercenaryJerryMutation(
					includesJerry, requestedGrid);
			Check(mutation.includesJerry == includesJerry,
				"the Jerry command always carries the requested enabled state");
			if (requestedGrid > 0)
			{
				Check(mutation.gridUpdate == GridUpdate::Requested &&
					mutation.gridNo ==
						static_cast<std::uint32_t>(requestedGrid),
					"positive Jerry grids retain the requested value");
			}
			else if (requestedGrid < 0)
			{
				Check(mutation.gridUpdate == GridUpdate::LegacyFallback &&
					mutation.gridNo == 15943,
					"every negative Jerry grid selects legacy grid 15943");
			}
			else
			{
				Check(mutation.gridUpdate == GridUpdate::Preserve &&
					mutation.gridNo == 0,
					"zero requests no Jerry-grid mutation");
			}
		}
	}

	// Exhaust the complete signed-16 range in addition to the INT32 extrema.
	// This catches accidental unsigned conversion before the sign decision.
	for (std::int32_t requestedGrid =
			std::numeric_limits<std::int16_t>::min();
		requestedGrid <= std::numeric_limits<std::int16_t>::max();
		++requestedGrid)
	{
		const CampaignMercenaryJerryMutation mutation =
			PlanCampaignMercenaryJerryMutation(true, requestedGrid);
		Check(
			(requestedGrid < 0 &&
				mutation.gridUpdate == GridUpdate::LegacyFallback &&
				mutation.gridNo == 15943) ||
			(requestedGrid == 0 &&
				mutation.gridUpdate == GridUpdate::Preserve) ||
			(requestedGrid > 0 &&
				mutation.gridUpdate == GridUpdate::Requested &&
				mutation.gridNo ==
					static_cast<std::uint32_t>(requestedGrid)),
			"the complete signed-16 Jerry domain retains its three-way policy");
	}

	ArrivalMutationModel model;
	ApplyJerryMutation(model,
		PlanCampaignMercenaryJerryMutation(true, 777));
	Check(model.includesJerry && model.jerryGrid == 777,
		"positive Jerry commands update enabled state and requested grid");
	ApplyJerryMutation(model,
		PlanCampaignMercenaryJerryMutation(false, 0));
	Check(!model.includesJerry && model.jerryGrid == 777,
		"zero Jerry commands update enabled state while preserving the grid");
	ApplyJerryMutation(model,
		PlanCampaignMercenaryJerryMutation(true, -1));
	Check(model.includesJerry && model.jerryGrid == 15943,
		"negative Jerry commands update enabled state and use the fallback");

	for (unsigned x = 0; x <= std::numeric_limits<std::uint8_t>::max(); ++x)
	{
		for (unsigned y = 0;
			y <= std::numeric_limits<std::uint8_t>::max(); ++y)
		{
			Check(LegacyArrivalCoordinatesAccepted(
				static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y)),
				"the legacy OR range check keeps its fallback unreachable");
		}
	}

	std::cout <<
		"Campaign mercenary-arrival mutation policy tests passed\n";
	return 0;
}
