#include "DedicatedCoopBattleNotice.h"
#include "DedicatedCoopRuntime.h"
#include "TacticalWorldAdapter.h"
#include "types.h"
#include "Game Clock.h"
#include "Overhead.h"
#include "strategicmap.h"
#include "strategic.h"
#include "connect.h"
#include <algorithm>
#include <cstdio>
#include <limits>

namespace
{
DedicatedCoopBattleNoticeState* bound = nullptr;
bool NativeContext() noexcept
{
	const auto& world = CaptureJa2TacticalWorld();
	return IsDedicatedCoopProcess() && !is_networked && !is_client && !is_server &&
		world.loaded && world.worldGeneration && world.turnSerial &&
		!world.turn.inCombat && !gTacticalStatus.fLastBattleWon &&
		world.sector.x >= 1 && world.sector.x <= 16 && world.sector.y >= 1 && world.sector.y <= 16 &&
		world.sector.z >= 0 && world.sector.z <= 3;
}
bool SameContext(const DedicatedCoopBattleNotice& notice) noexcept
{
	const auto& world = CaptureJa2TacticalWorld();
	return NativeContext() && notice.worldGeneration == world.worldGeneration &&
		notice.turnSerial == world.turnSerial && notice.sector == world.sector;
}
}

DedicatedCoopBattleNoticeState::~DedicatedCoopBattleNoticeState()
{ UnbindDedicatedCoopBattleNoticeState(*this); }
bool BindDedicatedCoopBattleNoticeState(DedicatedCoopBattleNoticeState& state) noexcept
{
	if (bound && bound != &state) return false;
	bound = &state;
	return true;
}
void UnbindDedicatedCoopBattleNoticeState(DedicatedCoopBattleNoticeState& state) noexcept
{ if (bound == &state) bound = nullptr; }
bool DedicatedCoopBattleNoticePending() noexcept
{ return bound && (bound->pending() || bound->failure()); }

bool DeferDedicatedCoopBattleNotice(DedicatedCoopBattleNoticeKind kind) noexcept
{
	if (!bound || !IsDedicatedCoopProcess() || is_networked || is_client || is_server) return false;
	InterruptTime(); StopTimeCompression(); PauseGame();
	if (bound->failure_) return true;
	if (kind < DedicatedCoopBattleNoticeKind::Defeated || kind > DedicatedCoopBattleNoticeKind::Surrendered || !NativeContext())
	{ bound->failure_ = "invalid native battle notice"; return true; }
	if (bound->notice_.id)
	{
		if (bound->notice_.kind != kind || !SameContext(bound->notice_))
			bound->failure_ = "overlapping native battle notices";
		return true;
	}
	if (bound->nextId_ == std::numeric_limits<std::uint64_t>::max())
	{ bound->failure_ = "native battle notice IDs exhausted"; return true; }
	const auto& world = CaptureJa2TacticalWorld();
	bound->notice_ = {bound->nextId_++, world.worldGeneration, world.turnSerial, world.sector, kind};
	std::printf("[dedicated] native battle notice pending: id=%llu; kind=%u; world=%llu; turn=%llu; awaiting a player acknowledgement\n",
		static_cast<unsigned long long>(bound->notice_.id), unsigned(kind),
		static_cast<unsigned long long>(world.worldGeneration), static_cast<unsigned long long>(world.turnSerial));
	std::fflush(stdout);
	return true;
}

bool DeferDedicatedCoopBattleSectorLoss(std::int16_t x, std::int16_t y, std::int8_t z) noexcept
{
	if (!bound || !IsDedicatedCoopProcess() || is_networked || is_client || is_server ||
		!bound->notice_.id || z != 0 || !(bound->notice_.sector == (TacticalWorldSession::Sector{x, y, z}))) return false;
	if (bound->failure_) return true;
	if (bound->acknowledged_ || !SameContext(bound->notice_) ||
		!StrategicMap[CALCULATE_STRATEGIC_INDEX(x, y)].fEnemyControlled)
	{ bound->failure_ = "native sector loss changed after battle notice acknowledgement"; return true; }
	if (bound->notice_.sectorControlLost) return true;
	if (bound->nextId_ == std::numeric_limits<std::uint64_t>::max())
	{ bound->failure_ = "native battle notice IDs exhausted"; return true; }
	bound->notice_.id = bound->nextId_++;
	bound->notice_.sectorControlLost = true;
	std::printf("[dedicated] native battle notice includes lost sector: id=%llu; sector=%d,%d,%d\n",
		static_cast<unsigned long long>(bound->notice_.id), int(x), int(y), int(z));
	std::fflush(stdout);
	return true;
}

bool PublishDedicatedCoopMilitiaBattleNotice(std::uint8_t x, std::uint8_t y, bool held,
	std::uint16_t militiaLost, std::uint16_t enemiesKilled) noexcept
{
	if (!bound || !IsDedicatedCoopProcess() || is_networked || is_client || is_server) return false;
	InterruptTime(); StopTimeCompression(); PauseGame();
	if (bound->failure_) return false;
	if (bound->notice_.id || x < 1 || x > 16 || y < 1 || y > 16)
	{ bound->failure_ = bound->notice_.id ? "overlapping native battle notices" : "invalid militia battle notice"; return false; }
	if (bound->nextId_ == std::numeric_limits<std::uint64_t>::max())
	{ bound->failure_ = "native battle notice IDs exhausted"; return false; }
	DedicatedCoopBattleNotice notice;
	notice.id = bound->nextId_++;
	notice.sector = {x, y, 0};
	notice.kind = held ? DedicatedCoopBattleNoticeKind::MilitiaHeld : DedicatedCoopBattleNoticeKind::MilitiaDefeated;
	notice.militiaLost = static_cast<std::uint8_t>(std::min<std::uint16_t>(militiaLost, 255));
	notice.enemiesKilled = static_cast<std::uint8_t>(std::min<std::uint16_t>(enemiesKilled, 255));
	bound->notice_ = notice;
	bound->acknowledged_ = false;
	std::printf("[dedicated] militia battle notice pending: id=%llu; sector=%u,%u; outcome=%s; militia_lost=%u; enemies_killed=%u; awaiting a player acknowledgement\n",
		static_cast<unsigned long long>(notice.id), unsigned(x), unsigned(y), held ? "held" : "defeated",
		unsigned(notice.militiaLost), unsigned(notice.enemiesKilled));
	std::fflush(stdout);
	return true;
}

DedicatedCoopBattleNoticeResult AcknowledgeDedicatedCoopBattleNotice(std::uint64_t id) noexcept
{
	using Result = DedicatedCoopBattleNoticeResult;
	if (!bound) return Result::NotPending;
	if (bound->failure_) return Result::Failed;
	if (!bound->notice_.id) return Result::NotPending;
	if (!id || id != bound->notice_.id) return Result::StaleNotice;
	if (bound->acknowledged_) return Result::NotPending;
	if (IsDedicatedCoopMilitiaBattleNotice(bound->notice_.kind))
	{
		// Nothing to unload: the battle was resolved and applied already.
		if (IsJa2TacticalWorldLoaded()) return Result::NativeContextChanged;
		std::printf("[dedicated] militia battle notice acknowledged: id=%llu\n", static_cast<unsigned long long>(id));
		std::fflush(stdout);
		bound->notice_ = {};
		return Result::Applied;
	}
	if (!SameContext(bound->notice_)) return Result::NativeContextChanged;
	bound->acknowledged_ = true;
	return Result::Applied;
}

DedicatedCoopBattleNoticeResult CompleteDedicatedCoopBattleNotice(std::uint64_t id) noexcept
{
	using Result = DedicatedCoopBattleNoticeResult;
	if (!bound) return Result::NotPending;
	if (bound->failure_) return Result::Failed;
	if (!bound->notice_.id || !bound->acknowledged_) return Result::NotPending;
	if (!id || id != bound->notice_.id) return Result::StaleNotice;
	if (!SameContext(bound->notice_))
	{
		bound->failure_ = "native battle notice context changed before unload";
		return Result::Failed;
	}
	// Consume before the ordinary native unload. A partial sector save, exception
	// or refusal must not allow a duplicate acknowledgement to repeat its effects.
	bound->notice_ = {};
	bound->acknowledged_ = false;
	try
	{
		if (!CheckAndHandleUnloadingOfCurrentWorld() || IsJa2TacticalWorldLoaded())
		{
			bound->failure_ = "native battle notice world unload refused";
			return Result::Failed;
		}
	}
	catch (...)
	{
		bound->failure_ = "native battle notice continuation failed";
		return Result::Failed;
	}
	return Result::Applied;
}
