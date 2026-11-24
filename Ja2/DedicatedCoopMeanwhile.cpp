#include "DedicatedCoopMeanwhile.h"
#include "DedicatedCoopRuntime.h"
#include "TacticalWorldAdapter.h"
#include "types.h"
#include "Meanwhile.h"
#include "Game Clock.h"
#include "Overhead.h"
#include "PreBattle Interface.h"
#include "strategicmap.h"
#include "connect.h"
#include <cstdio>
#include <limits>

namespace
{
DedicatedCoopMeanwhileState* bound = nullptr;
bool NativeContext() noexcept
{
	const auto& scene = gCurrentMeanwhileDef;
	return IsDedicatedCoopProcess() && !is_networked && !is_client && !is_server &&
		gfMeanwhileTryingToStart && !gfInMeanwhile && !gfTacticalTraversal &&
		!(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) && !IsJa2TacticalCombatActive() &&
		GamePaused() && PauseStateLocked() && guiLockPauseStateLastReasonId == 6 &&
		scene.ubMeanwhileID < NUM_MEANWHILES && scene.sSectorX >= 1 && scene.sSectorX <= 16 &&
		scene.sSectorY >= 1 && scene.sSectorY <= 16;
}
bool SameContext(const DedicatedCoopMeanwhileNotice& notice) noexcept
{
	const auto& world = CaptureJa2TacticalWorld();
	const auto& scene = gCurrentMeanwhileDef;
	return NativeContext() && notice.worldSeconds == GetWorldTotalSeconds() &&
		notice.worldLoaded == world.loaded && notice.worldGeneration == world.worldGeneration &&
		notice.turnSerial == world.turnSerial && notice.worldSector == world.sector &&
		notice.scene == scene.ubMeanwhileID && notice.npc == scene.ubNPCNumber &&
		notice.triggerEvent == scene.usTriggerEvent && notice.sceneX == scene.sSectorX && notice.sceneY == scene.sSectorY;
}
}

DedicatedCoopMeanwhileState::~DedicatedCoopMeanwhileState()
{ UnbindDedicatedCoopMeanwhileState(*this); }
bool BindDedicatedCoopMeanwhileState(DedicatedCoopMeanwhileState& state) noexcept
{
	if (bound && bound != &state) return false;
	bound = &state; return true;
}
void UnbindDedicatedCoopMeanwhileState(DedicatedCoopMeanwhileState& state) noexcept
{ if (bound == &state) bound = nullptr; }
bool DedicatedCoopMeanwhilePending() noexcept
{ return bound && (bound->pending() || bound->failure()); }

bool DeferDedicatedCoopMeanwhile() noexcept
{
	if (!bound || !IsDedicatedCoopProcess() || is_networked || is_client || is_server) return false;
	InterruptTime(); StopTimeCompression(); PauseGame();
	if (bound->failure_) return true;
	if (!NativeContext()) { bound->failure_ = "invalid native meanwhile context"; return true; }
	if (bound->notice_.id)
	{
		if (!SameContext(bound->notice_)) bound->failure_ = "overlapping native meanwhile scenes";
		return true;
	}
	if (bound->nextId_ == std::numeric_limits<std::uint64_t>::max())
	{ bound->failure_ = "native meanwhile IDs exhausted"; return true; }
	const auto& world = CaptureJa2TacticalWorld();
	const auto& scene = gCurrentMeanwhileDef;
	bound->notice_ = {bound->nextId_++, world.worldGeneration, world.turnSerial, GetWorldTotalSeconds(),
		world.sector, world.loaded, scene.sSectorX, scene.sSectorY, scene.usTriggerEvent, scene.ubMeanwhileID, scene.ubNPCNumber};
	std::printf("[dedicated] native meanwhile pending: id=%llu; scene=%u; time=%u; awaiting explicit skip\n",
		static_cast<unsigned long long>(bound->notice_.id), unsigned(scene.ubMeanwhileID), GetWorldTotalSeconds());
	std::fflush(stdout);
	return true;
}

DedicatedCoopMeanwhileResult AcknowledgeDedicatedCoopMeanwhile(std::uint64_t id) noexcept
{
	using Result = DedicatedCoopMeanwhileResult;
	if (!bound) return Result::NotPending;
	if (bound->failure_) return Result::Failed;
	if (!bound->notice_.id) return Result::NotPending;
	if (!id || id != bound->notice_.id) return Result::StaleNotice;
	if (bound->acknowledged_) return Result::NotPending;
	if (!SameContext(bound->notice_)) return Result::NativeContextChanged;
	bound->acknowledged_ = true;
	return Result::Applied;
}

DedicatedCoopMeanwhileResult CompleteDedicatedCoopMeanwhile(std::uint64_t id) noexcept
{
	using Result = DedicatedCoopMeanwhileResult;
	if (!bound) return Result::NotPending;
	if (bound->failure_) return Result::Failed;
	if (!bound->notice_.id || !bound->acknowledged_) return Result::NotPending;
	if (!id || id != bound->notice_.id) return Result::StaleNotice;
	if (!SameContext(bound->notice_))
	{ bound->failure_ = "native meanwhile context changed before skip"; return Result::Failed; }
	const auto scene = bound->notice_.scene;
	// Consume before applying consequences: a partial native failure must not
	// permit duplicate troop dispatches, quest starts or RNG consumption.
	bound->notice_ = {}; bound->acknowledged_ = false;
	gfMeanwhileTryingToStart = FALSE;
	try { SkipMeanwhileScene(); }
	catch (...)
	{
		bound->failure_ = "native meanwhile skip failed";
		InterruptTime(); StopTimeCompression(); PauseGame();
		return Result::Failed;
	}
	InterruptTime(); StopTimeCompression(); PauseGame();
	std::printf("[dedicated] native meanwhile skipped: id=%llu; scene=%u; time=%u; campaign paused\n",
		static_cast<unsigned long long>(id), unsigned(scene), GetWorldTotalSeconds());
	std::fflush(stdout);
	return Result::Applied;
}
