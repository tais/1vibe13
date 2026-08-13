#include <Engine/Adapters/JA2/TacticalDoorUiSession.h>

#include <cstddef>
#include <cstdio>
#include <type_traits>

static_assert(
	std::is_standard_layout<TacticalDoorStructureIdentity>::value &&
	std::is_trivially_copyable<TacticalDoorStructureIdentity>::value,
	"door structure identity must remain a pointer-free public value ABI");
static_assert(
	std::is_standard_layout<TacticalDoorUiContext>::value &&
	std::is_trivially_copyable<TacticalDoorUiContext>::value,
	"door UI context must remain a pointer-free public value ABI");
static_assert(
	std::is_same<decltype(TacticalDoorUiContext::worldGeneration),
		std::uint64_t>::value &&
	std::is_same<decltype(TacticalDoorStructureIdentity::grid),
		std::int32_t>::value &&
	std::is_same<decltype(TacticalDoorStructureIdentity::baseGrid),
		std::int32_t>::value &&
	std::is_same<decltype(TacticalDoorStructureIdentity::structureId),
		std::uint16_t>::value &&
	std::is_same<decltype(TacticalDoorStructureIdentity::fingerprint),
		std::uint64_t>::value,
	"door UI public identity field widths are part of the SDK contract");
static_assert(
	offsetof(TacticalDoorStructureIdentity, grid) <
		offsetof(TacticalDoorStructureIdentity, baseGrid) &&
	offsetof(TacticalDoorStructureIdentity, baseGrid) <
		offsetof(TacticalDoorStructureIdentity, structureId) &&
	offsetof(TacticalDoorStructureIdentity, structureId) <
		offsetof(TacticalDoorStructureIdentity, fingerprint) &&
	offsetof(TacticalDoorUiContext, actor) <
		offsetof(TacticalDoorUiContext, worldGeneration) &&
	offsetof(TacticalDoorUiContext, worldGeneration) <
		offsetof(TacticalDoorUiContext, structure) &&
	offsetof(TacticalDoorUiContext, structure) <
		offsetof(TacticalDoorUiContext, direction) &&
	offsetof(TacticalDoorUiContext, direction) <
		offsetof(TacticalDoorUiContext, closingDoor),
	"door UI public identity field order is part of the SDK contract");

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
	if (!condition)
	{
		++failures;
		std::printf("FAIL  %s\n", message);
		return;
	}
	std::printf("ok    %s\n", message);
}
}

int main()
{
	TacticalDoorUiSession session;
	const TacticalEntityId actor{7, 0x10203040u};
	const TacticalDoorStructureIdentity door{1311, 1310, 0x4567u,
		0x0102030405060708ull};
	const TacticalDoorUiContext context{actor, 41, door, 2, false};

	Check(!session.active() && !session.context().valid() &&
		!session.matches(41, actor, door),
		"door UI sessions start empty and reject resolution without capture");
	Check(!session.begin({{}, 41, door, 2, false}) &&
		!session.begin({actor, 0, door, 2, false}) &&
		!session.begin({actor, 41, {}, 2, false}) &&
		!session.begin({actor, 41, door, 8, false}),
		"door UI sessions reject invalid actor, world, structure, and direction values");
	Check(session.begin(context) && session.active() &&
		session.context().actor == actor &&
		session.context().worldGeneration == 41 &&
		session.context().structure == door &&
		session.context().direction == 2 &&
		!session.context().closingDoor &&
		session.matches(41, actor, door),
		"door UI sessions retain one exact pointer-free modal identity");
	Check(!session.begin({actor, 42, door, 3, true}),
		"an active door modal cannot be replaced without explicit teardown");
	Check(!session.matches(42, actor, door) &&
		!session.matches(41, TacticalEntityId{actor.slot,
			actor.incarnation + 1}, door) &&
		!session.matches(41, actor,
			TacticalDoorStructureIdentity{door.grid + 1, door.baseGrid,
				door.structureId, door.fingerprint}) &&
		!session.matches(41, actor,
			TacticalDoorStructureIdentity{door.grid, door.baseGrid + 1,
				door.structureId, door.fingerprint}) &&
		!session.matches(41, actor,
			TacticalDoorStructureIdentity{door.grid, door.baseGrid,
				static_cast<std::uint16_t>(door.structureId + 1),
				door.fingerprint}) &&
		!session.matches(41, actor,
			TacticalDoorStructureIdentity{door.grid, door.baseGrid,
				door.structureId, door.fingerprint + 1}),
		"world transitions, actor slot reuse, and missing or replaced structures fail closed");
	session.reset();
	Check(!session.active() && session.begin(
		{actor, 42, door, 3, true}) &&
		session.context().closingDoor && session.context().direction == 3,
		"explicit teardown permits a fresh world/modal selection");
	session.reset();

	return failures == 0 ? 0 : 1;
}
