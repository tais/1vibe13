#include "DedicatedCoopSurrender.h"
#include "DedicatedCoopRuntime.h"
#include "GameContext.h"
#include "CampaignCivilianQuotePolicy.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Civ Quotes.h"
#include "Game Clock.h"
#include "Overhead.h"
#include "ai.h"
#include "connect.h"
#include <limits>
#include <cstdio>

namespace
{
DedicatedCoopSurrenderState* bound = nullptr;
bool NativeContext(const TacticalActor& speaker) noexcept
{
	const auto& world = CaptureJa2TacticalWorld();
	return IsDedicatedCoopProcess() && !is_networked && !is_client && !is_server &&
		CampaignCivilianQuotePolicy(GetGameContext().capabilities()).completesSurrenderOfferAfterQuote() &&
		world.loaded && world.worldGeneration && world.turnSerial &&
		world.turn.inCombat && world.turn.currentTeam == ENEMY_TEAM &&
		speaker.roster().active() && speaker.roster().inSector() &&
		speaker.roster().team() == ENEMY_TEAM && speaker.vitals().health() >= OKLIFE &&
		speaker.aiPlanning().action() == AI_ACTION_OFFER_SURRENDER &&
		!(gTacticalStatus.fEnemyFlags & ENEMY_OFFERED_SURRENDER);
}
}

DedicatedCoopSurrenderState::~DedicatedCoopSurrenderState()
{ UnbindDedicatedCoopSurrenderState(*this); }
bool BindDedicatedCoopSurrenderState(DedicatedCoopSurrenderState& state) noexcept
{
	if (bound && bound != &state) return false;
	bound = &state;
	return true;
}
void UnbindDedicatedCoopSurrenderState(DedicatedCoopSurrenderState& state) noexcept
{ if (bound == &state) bound = nullptr; }
bool DedicatedCoopSurrenderPending() noexcept
{ return bound && (bound->pending() || bound->failure()); }

bool DeferDedicatedCoopSurrender(TacticalActor& speaker) noexcept
{
	if (!bound || !IsDedicatedCoopProcess() || is_networked || is_client || is_server)
		return false;
	InterruptTime(); StopTimeCompression(); PauseGame();
	if (bound->failure_) return true;
	const auto id = GetJa2TacticalEntityId(speaker);
	const auto& world = CaptureJa2TacticalWorld();
	if (!NativeContext(speaker) || !id.valid() || ResolveJa2TacticalEntity(id) != &speaker)
	{ bound->failure_ = "invalid native surrender offer"; return true; }
	if (bound->offer_.id)
	{
		if (bound->offer_.speaker != id || bound->offer_.worldGeneration != world.worldGeneration ||
			bound->offer_.turnSerial != world.turnSerial)
			bound->failure_ = "overlapping native surrender offers";
		return true;
	}
	if (bound->nextId_ == std::numeric_limits<std::uint64_t>::max())
	{ bound->failure_ = "native surrender offer IDs exhausted"; return true; }
	bound->offer_ = {bound->nextId_++, world.worldGeneration, world.turnSerial, id};
	std::printf("[dedicated] native surrender offer pending: id=%llu; world=%llu; turn=%llu; speaker=%u:%u; awaiting an explicit player answer\n",
		static_cast<unsigned long long>(bound->offer_.id), static_cast<unsigned long long>(world.worldGeneration),
		static_cast<unsigned long long>(world.turnSerial), id.slot, id.incarnation);
	std::fflush(stdout);
	return true;
}

DedicatedCoopSurrenderResult ReplyToDedicatedCoopSurrender(
	std::uint64_t offer, DedicatedCoopSurrenderReply reply) noexcept
{
	using Result = DedicatedCoopSurrenderResult;
	if (!bound) return Result::NotPending;
	if (bound->failure_) return Result::Failed;
	if (!bound->offer_.id) return Result::NotPending;
	if (!offer || offer != bound->offer_.id) return Result::StaleOffer;
	if (reply != DedicatedCoopSurrenderReply::ContinueFighting && reply != DedicatedCoopSurrenderReply::Surrender)
		return Result::NativeContextChanged;
	const auto pending = bound->offer_;
	const auto& world = CaptureJa2TacticalWorld();
	auto* speaker = ResolveJa2TacticalEntity(pending.speaker);
	if (!speaker || !NativeContext(*speaker) || pending.worldGeneration != world.worldGeneration ||
		pending.turnSerial != world.turnSerial)
		return Result::NativeContextChanged;
	// Consume before the native continuation: a partial mutation or exception
	// must never allow replay. Use the exact retained speaker, not the quote UI's
	// mutable global pointer or whichever mercenary a client has selected.
	bound->offer_ = {};
	try
	{
		CompleteCivSurrenderOffer(*speaker, reply == DedicatedCoopSurrenderReply::Surrender);
	}
	catch (...)
	{
		bound->failure_ = "native surrender continuation failed";
		return Result::Failed;
	}
	return Result::Applied;
}
