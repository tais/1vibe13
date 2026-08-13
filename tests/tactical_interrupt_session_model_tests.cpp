#include <Engine/Adapters/JA2/TacticalWorldSession.h>

#include <cstdint>
#include <initializer_list>
#include <type_traits>

static_assert(std::is_standard_layout<
	TacticalWorldSession::Snapshot::Interrupt>::value,
	"public interrupt snapshots must remain layout-safe values");
static_assert(std::is_trivially_copyable<
	TacticalWorldSession::Snapshot::Interrupt>::value,
	"public interrupt snapshots must remain pointer-free copy values");
static_assert(sizeof(TacticalWorldSession::Snapshot::Interrupt) == 2,
	"public interrupt snapshots must retain their two scalar fields");

namespace
{
bool Same(
	const TacticalWorldSession::Snapshot::Interrupt& left,
	const TacticalWorldSession::Snapshot::Interrupt& right) noexcept
{
	return left == right;
}
}

int main()
{
	TacticalWorldSession session;
	if (!Same(
			session.snapshot().interrupt,
			TacticalWorldSession::Snapshot::Interrupt{}))
		return 1;

	for (std::uint16_t value = 0; value <= 255; ++value)
	{
		for (const bool disabled : {false, true})
		{
			const TacticalWorldSession::Snapshot::Interrupt expected{
				static_cast<std::uint8_t>(value), disabled};
			session.restoreInterruptState(expected);
			if (!Same(session.snapshot().interrupt, expected)) return 2;
		}
	}

	session.restoreInterruptState({7, true});
	const TacticalWorldSession::Snapshot::Interrupt captured =
		session.snapshot().interrupt;
	session.setPendingInterrupt(3);
	if (!Same(captured, {7, true}) ||
		!Same(session.snapshot().interrupt, {3, true}))
		return 3;
	session.setPlayerInterruptsDisabled(false);
	if (!Same(session.snapshot().interrupt, {3, false})) return 4;

	// World lifecycle did not implicitly rewrite either legacy field. Their
	// established reset site calls the explicit reset gateway instead.
	session.commitLoad();
	session.unload();
	if (!Same(session.snapshot().interrupt, {3, false})) return 5;
	session.resetInterruptState();
	if (!Same(
			session.snapshot().interrupt,
			TacticalWorldSession::Snapshot::Interrupt{}))
		return 6;

	TacticalWorldSession restored;
	TacticalWorldSession::Snapshot state = session.snapshot();
	state.interrupt = {8, true};
	restored.restore(state);
	if (!Same(restored.snapshot().interrupt, {8, true})) return 7;

	return 0;
}
