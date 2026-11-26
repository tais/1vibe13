#include "CoopCampaignActionAuthority.h"
#include "CoopCampaignTime.h"
#include <cstdio>
#include <stdexcept>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
PeerIdentity Identity(unsigned n) { PeerIdentity p{}; p[0] = static_cast<std::uint8_t>(n); return p; }
CoopCampaignActionRequest Travel()
{
	CoopCampaignActionRequest request;
	request.sessionEpoch = 0x0102030405060708ull; request.controlRevision = 9; request.groupsRevision = 10; request.requestId = 11;
	request.group = {3, 0x04030201}; request.destinationX = 10; request.destinationY = 1;
	return request;
}
void Codec()
{
	auto request = Travel();
	CoopCampaignActionRequestBytes bytes{}, expected{{'J','2','A','Q',
		static_cast<std::uint8_t>(CurrentProtocolVersion),static_cast<std::uint8_t>(CurrentProtocolVersion >> 8),1,0,
		8,7,6,5,4,3,2,1, 9,0,0,0,0,0,0,0, 10,0,0,0,0,0,0,0, 11,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0, 3,10,1,0, 1,2,3,4}};
	CHECK(EncodeCoopCampaignActionRequest(request, bytes) && bytes == expected, "exact little-endian travel vector; no claimed peer identity");
	for (unsigned action = 1; action <= 10; ++action)
	{
		request = Travel(); request.action = static_cast<CoopCampaignAction>(action);
		if (action != 1) { request.group = {}; request.destinationX = request.destinationY = 0; request.decision = 0x0807060504030201ull; }
		CoopCampaignActionRequest decoded;
		CHECK(EncodeCoopCampaignActionRequest(request, bytes) && DecodeCoopCampaignActionRequest(bytes.data(), bytes.size(), decoded) &&
			SameCoopCampaignActionRequest(request, decoded), "every canonical tagged action roundtrips");
		if (action != 1) CHECK(bytes[40] == 1 && bytes[47] == 8 && !bytes[48] && !bytes[52], "arrival decision is little endian and separate from zero group identity");
		for (std::size_t size = 0; size <= bytes.size() + 1; ++size)
			if (size != bytes.size()) CHECK(!DecodeCoopCampaignActionRequest(bytes.data(), size, decoded) &&
				SameCoopCampaignActionRequest(request, decoded), "nonexact request lengths leave prior output intact");
		for (unsigned at : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 51u})
		{
			auto bad = bytes; bad[at] ^= 0xff;
			CHECK(!DecodeCoopCampaignActionRequest(bad.data(), bad.size(), decoded) && SameCoopCampaignActionRequest(request, decoded),
				"corrupt version/action/magic/reserved request bytes are transactional");
		}
		for (unsigned at : {8u, 16u, 24u, 32u})
		{
			auto bad = bytes; std::fill(bad.begin() + at, bad.begin() + at + 8, 0);
			CHECK(!DecodeCoopCampaignActionRequest(bad.data(), bad.size(), decoded), "zero epoch/revision/request id rejected");
		}
		for (unsigned outcome = 1; outcome <= 8; ++outcome)
		{
			CoopCampaignActionResult result{request, 12, 13, static_cast<CoopCampaignActionOutcome>(outcome), 0}, out;
			CoopCampaignActionResultBytes encoded;
			CHECK(EncodeCoopCampaignActionResult(result, encoded) && encoded[3] == 'R' && encoded[7] == outcome && encoded[56] == 12 &&
				encoded[64] == 13 && DecodeCoopCampaignActionResult(encoded.data(), encoded.size(), out) &&
				SameCoopCampaignActionRequest(request, out.request) && out.controlRevision == 12 && out.groupsRevision == 13 &&
				out.outcome == result.outcome && !out.nativeDetail, "every terminal result roundtrips its exact request and revisions");
			for (std::size_t size = 0; size <= encoded.size() + 1; ++size)
				if (size != encoded.size()) CHECK(!DecodeCoopCampaignActionResult(encoded.data(), size, out) &&
					SameCoopCampaignActionRequest(request, out.request) && out.outcome == result.outcome, "nonexact result lengths are transactional");
			for (unsigned at : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 51u, 74u, 75u, 76u, 77u, 78u, 79u})
			{
				auto bad = encoded; bad[at] ^= 0xff;
				CHECK(!DecodeCoopCampaignActionResult(bad.data(), bad.size(), out) && out.outcome == result.outcome,
					"invalid result header and reserved bytes leave terminal output intact");
			}
		}
	}
	request = Travel();
	for (unsigned fault = 0; fault < 7; ++fault)
	{
		auto bad = request;
		if (fault == 0) bad.decision = 1;
		if (fault == 1) bad.group.slot = 0;
		if (fault == 2) bad.group.incarnation = 0;
		if (fault == 3) bad.destinationX = 0;
		if (fault == 4) bad.destinationX = 17;
		if (fault == 5) bad.destinationY = 0;
		if (fault == 6) bad.destinationY = 17;
		auto saved = bytes;
		CHECK(!EncodeCoopCampaignActionRequest(bad, bytes) && bytes == saved, "invalid travel payload cannot replace output");
	}
	request.action = CoopCampaignAction::StopArrival; request.decision = 1; request.group = {}; request.destinationX = request.destinationY = 0;
	CHECK(EncodeCoopCampaignActionRequest(request, bytes), "canonical arrival fixture");
	for (unsigned at : {48u, 49u, 50u, 52u, 53u, 54u, 55u})
	{
		auto bad = bytes; bad[at] = 1; CoopCampaignActionRequest decoded;
		CHECK(!DecodeCoopCampaignActionRequest(bad.data(), bad.size(), decoded), "unused arrival group/destination bytes must all be zero");
	}
	request.decision = 0;
	CHECK(!EncodeCoopCampaignActionRequest(request, bytes), "arrival decision cannot be zero");
	CHECK(!DecodeCoopCampaignActionRequest(nullptr, 56, request), "null request rejected");
	CoopCampaignActionResult result{Travel(), 9, 10, CoopCampaignActionOutcome::Applied, 0}, decoded;
	CoopCampaignActionResultBytes encoded{}; encoded.fill(0xee); const auto saved = encoded;
	CHECK(!EncodeCoopCampaignActionResult(result, encoded) && encoded == saved, "applied receipt requires consumed control barrier");
	result.outcome = CoopCampaignActionOutcome::NativeRejected;
	CHECK(!EncodeCoopCampaignActionResult(result, encoded), "native rejection also requires consumed control barrier");
	result.controlRevision = 10; result.nativeDetail = 0x0102;
	CHECK(EncodeCoopCampaignActionResult(result, encoded) && encoded[72] == 2 && encoded[73] == 1 &&
		DecodeCoopCampaignActionResult(encoded.data(), encoded.size(), decoded) && decoded.nativeDetail == 0x0102, "captured native detail has explicit width/endianness");
	result.outcome = CoopCampaignActionOutcome::Stale;
	CHECK(!EncodeCoopCampaignActionResult(result, encoded), "preflight rejection cannot claim native execution detail");
	CHECK(!DecodeCoopCampaignActionResult(nullptr, 80, decoded), "null result rejected");
}
struct Fixture
{
	PeerIdentity identities[2]{Identity(1), Identity(2)};
	CoopCampaignActionAuthority::Peer peers[2]{{identities[0], TransportPeer{1}}, {identities[1], TransportPeer{2}}};
	CoopCampaignStatusLedger status;
	CoopCampaignGroups groups;
	CoopCampaignActionAuthority authority;
	Fixture()
	{
		CoopCampaignStatus clock; clock.phase = CoopCampaignPhase::Strategic;
		CHECK(status.beginSession(1) && status.observe(clock, identities, 2) && authority.reconcile(peers, 2), "two ready authenticated peers");
		groups.sessionEpoch = 1; groups.revision = 1; groups.available = true; groups.groupCount = groups.memberCount = 1;
		groups.groups[0].id = {3, 4}; groups.groups[0].x = 9; groups.groups[0].y = 1; groups.groups[0].memberCount = 1;
		groups.members[0].actor = {1, 1};
		CHECK(ValidCoopCampaignGroups(groups), "native group observation fixture");
	}
	CoopCampaignActionRequest request(std::uint64_t id = 1) const
	{
		auto r = Travel(); r.sessionEpoch = 1; r.controlRevision = status.value().timeControlRevision; r.groupsRevision = groups.revision;
		r.requestId = id; r.group = groups.groups[0].id;
		return r;
	}
	CoopCampaignActionRequest arrival(CoopCampaignAction action, std::uint64_t id = 1) const
	{
		auto r = request(id); r.action = action; r.decision = status.value().arrival.decision;
		r.group = {}; r.destinationX = r.destinationY = 0;
		return r;
	}
	void observeArrival(CoopCampaignArrival value)
	{
		auto clock = status.value(); clock.arrival = value;
		CHECK(status.observe(clock, identities, 2), "observe native arrival");
	}
};
void PolicyAndState()
{
	Fixture f;
	const auto allowed = [&](const CoopCampaignActionRequest& r) {
		return ValidateCoopCampaignActionRequest(r, f.status.value(), f.groups, f.identities[1], true, true, true);
	};
	CHECK(allowed(f.request()) == CoopCampaignActionOutcome::Applied, "shared campaign authority is independent of time leadership");
	CHECK(ValidateCoopCampaignActionRequest(f.request(), f.status.value(), f.groups, f.identities[0], false, true, true) == CoopCampaignActionOutcome::NotReady &&
		ValidateCoopCampaignActionRequest(f.request(), f.status.value(), f.groups, f.identities[0], true, false, true) == CoopCampaignActionOutcome::Unauthorized &&
		ValidateCoopCampaignActionRequest(f.request(), f.status.value(), f.groups, f.identities[0], true, true, false) == CoopCampaignActionOutcome::Unavailable &&
		ValidateCoopCampaignActionRequest(f.request(), f.status.value(), f.groups, PeerIdentity{}, true, true, true) == CoopCampaignActionOutcome::NotReady,
		"readiness, explicit policy, native context and authenticated identity are separate gates");
	for (unsigned fault = 0; fault < 5; ++fault)
	{
		auto r = f.request();
		if (fault == 0) ++r.sessionEpoch;
		if (fault == 1) ++r.controlRevision;
		if (fault == 2) ++r.groupsRevision;
		if (fault == 3) ++r.group.incarnation;
		if (fault == 4) ++r.group.slot;
		CHECK(allowed(r) == CoopCampaignActionOutcome::Stale, "epoch/control/groups/exact group identity must match server observation");
	}
	auto r = f.request(); r.destinationX = 11;
	CHECK(allowed(r) == CoopCampaignActionOutcome::Unsupported, "multi-leg travel excluded");
	r.destinationX = 10; r.destinationY = 2;
	CHECK(allowed(r) == CoopCampaignActionOutcome::Unsupported, "diagonal travel excluded");
	f.groups.groups[0].vehicle = true;
	CHECK(allowed(f.request()) == CoopCampaignActionOutcome::Unsupported, "vehicles await native travel implementation");
	f.groups.groups[0].vehicle = false; f.groups.groups[0].betweenSectors = true; f.groups.groups[0].nextX = 10; f.groups.groups[0].nextY = 1;
	CHECK(allowed(f.request()) == CoopCampaignActionOutcome::Unavailable, "in-transit groups cannot receive new routes");
	f.groups.groups[0].betweenSectors = false; f.groups.groups[0].nextX = f.groups.groups[0].nextY = 0;
	CoopCampaignArrival npc; npc.decision = 1; npc.kind = CoopCampaignArrivalKind::WildernessNpc;
	npc.stage = CoopCampaignArrivalStage::Pending; npc.x = 10; npc.y = 1; npc.pendingCount = 1;
	f.observeArrival(npc);
	CHECK(allowed(f.request()) == CoopCampaignActionOutcome::Unavailable, "travel cannot bypass an arrival decision");
	CHECK(allowed(f.arrival(CoopCampaignAction::AcknowledgeArrival)) == CoopCampaignActionOutcome::Unsupported &&
		allowed(f.arrival(CoopCampaignAction::StopArrival)) == CoopCampaignActionOutcome::Applied, "retained NPC route requires explicit stop");
	npc.decision = 2; npc.finalDestination = true; f.observeArrival(npc);
	CHECK(allowed(f.arrival(CoopCampaignAction::AcknowledgeArrival)) == CoopCampaignActionOutcome::Applied, "finished NPC trip supports acknowledgment");
	r = f.arrival(CoopCampaignAction::StopArrival); --r.decision;
	CHECK(allowed(r) == CoopCampaignActionOutcome::Stale, "older decision cannot answer its replacement");
	CoopCampaignArrival battle; battle.decision = 3; battle.kind = CoopCampaignArrivalKind::Battle;
	battle.stage = CoopCampaignArrivalStage::Prepared; battle.x = 10; battle.y = 1; battle.pendingCount = 1;
	battle.involvedMercs = 1; battle.nativeEnterSector = battle.nativeRetreat = true; f.observeArrival(battle);
	CHECK(allowed(f.arrival(CoopCampaignAction::EnterArrivalForced)) == CoopCampaignActionOutcome::Applied &&
		allowed(f.arrival(CoopCampaignAction::EnterArrivalSpread)) == CoopCampaignActionOutcome::Unsupported &&
		allowed(f.arrival(CoopCampaignAction::RetreatArrival)) == CoopCampaignActionOutcome::Applied &&
		allowed(f.arrival(CoopCampaignAction::StopArrival)) == CoopCampaignActionOutcome::Unsupported, "exact prepared native action capabilities");
	battle.nativePlacement = true; battle.nativeRetreat = false; f.observeArrival(battle);
	CHECK(allowed(f.arrival(CoopCampaignAction::EnterArrivalForced)) == CoopCampaignActionOutcome::Unsupported &&
		allowed(f.arrival(CoopCampaignAction::EnterArrivalSpread)) == CoopCampaignActionOutcome::Applied &&
		allowed(f.arrival(CoopCampaignAction::RetreatArrival)) == CoopCampaignActionOutcome::Unsupported, "deployment and retreat follow native capability flags");
	battle.pendingCount = 2; f.observeArrival(battle);
	CHECK(allowed(f.arrival(CoopCampaignAction::EnterArrivalSpread)) == CoopCampaignActionOutcome::Unavailable, "multiple pending decisions block battle entry");
	battle.pendingCount = 1; f.observeArrival(battle);
	f.groups.available = false; f.groups.groupCount = f.groups.memberCount = 0;
	CHECK(allowed(f.arrival(CoopCampaignAction::EnterArrivalSpread)) == CoopCampaignActionOutcome::Applied, "exact arrival can resolve while group observation is explicitly unavailable");
}
void ReturnToMapFromSecuredSector()
{
	Fixture f;
	const auto allowed = [&](const CoopCampaignActionRequest& r) {
		return ValidateCoopCampaignActionRequest(r, f.status.value(), f.groups, f.identities[1], true, true, false);
	};
	const auto returnRequest = [&](std::uint64_t decision) {
		auto r = f.arrival(CoopCampaignAction::ReturnToMap); r.decision = decision;
		return r;
	};
	CoopCampaignActionRequestBytes bytes{};
	CoopCampaignActionRequest decoded;
	CHECK(EncodeCoopCampaignActionRequest(returnRequest(7), bytes) && bytes[6] == 11 &&
		DecodeCoopCampaignActionRequest(bytes.data(), bytes.size(), decoded) &&
		SameCoopCampaignActionRequest(decoded, returnRequest(7)), "return-to-map is kind 11 and roundtrips");
	auto travelShaped = returnRequest(7); travelShaped.group = f.groups.groups[0].id;
	CHECK(!ValidCoopCampaignActionRequest(returnRequest(0)) && !ValidCoopCampaignActionRequest(travelShaped),
		"return-to-map carries only an exact secured-sector token");
	CHECK(allowed(returnRequest(1)) == CoopCampaignActionOutcome::Unavailable,
		"strategic play has no secured sector to leave");
	auto clock = f.status.value(); clock.phase = CoopCampaignPhase::Tactical;
	CHECK(f.status.observe(clock, f.identities, 2), "observe ordinary tactical encounter");
	CHECK(allowed(returnRequest(1)) == CoopCampaignActionOutcome::Unavailable,
		"an unsecured encounter cannot be abandoned through return-to-map");
	clock = f.status.value(); clock.securedSector = 5;
	CHECK(f.status.observe(clock, f.identities, 2), "observe secured sector");
	CHECK(allowed(returnRequest(5)) == CoopCampaignActionOutcome::Applied,
		"any ready player may return from a secured sector, independent of time leadership");
	CHECK(allowed(returnRequest(4)) == CoopCampaignActionOutcome::Stale, "an older secured-sector token is stale");
	auto old = returnRequest(5); --old.controlRevision;
	CHECK(allowed(old) == CoopCampaignActionOutcome::Stale, "return binds the current control revision");
	clock = f.status.value(); clock.surrenderOffer = 3;
	CHECK(f.status.observe(clock, f.identities, 2) && allowed(returnRequest(5)) == CoopCampaignActionOutcome::Unavailable,
		"a pending surrender offer keeps the sector open");
	CHECK(ValidateCoopCampaignActionRequest(returnRequest(5), f.status.value(), f.groups, f.identities[1], false, true, false) ==
			CoopCampaignActionOutcome::NotReady &&
		ValidateCoopCampaignActionRequest(returnRequest(5), f.status.value(), f.groups, f.identities[1], true, false, false) ==
			CoopCampaignActionOutcome::Unauthorized, "readiness and explicit policy still gate the return");
}
void EnterSectorForPeacefulPlay()
{
	Fixture f;
	const auto allowed = [&](const CoopCampaignActionRequest& r) {
		return ValidateCoopCampaignActionRequest(r, f.status.value(), f.groups, f.identities[1], true, true, true);
	};
	auto enter = f.request(); enter.action = CoopCampaignAction::EnterSector;
	enter.destinationX = f.groups.groups[0].x; enter.destinationY = f.groups.groups[0].y;
	CoopCampaignActionRequestBytes bytes{}; CoopCampaignActionRequest decoded;
	CHECK(EncodeCoopCampaignActionRequest(enter, bytes) && bytes[6] == 12 &&
		DecodeCoopCampaignActionRequest(bytes.data(), bytes.size(), decoded) && SameCoopCampaignActionRequest(decoded, enter),
		"enter-sector is kind 12 and roundtrips with its exact group and displayed sector");
	auto withDecision = enter; withDecision.decision = 1;
	auto withoutGroup = enter; withoutGroup.group = {};
	CHECK(!ValidCoopCampaignActionRequest(withDecision) && !ValidCoopCampaignActionRequest(withoutGroup),
		"enter-sector names a group and its sector, never a decision token");
	CHECK(allowed(enter) == CoopCampaignActionOutcome::Applied, "any ready player may enter a group's own sector");
	auto elsewhere = enter; elsewhere.destinationX = 10;
	CHECK(allowed(elsewhere) == CoopCampaignActionOutcome::Stale, "the displayed sector must be the group's current sector");
	CHECK(ValidateCoopCampaignActionRequest(enter, f.status.value(), f.groups, f.identities[1], true, true, false) ==
		CoopCampaignActionOutcome::Unavailable, "a loaded tactical world blocks peaceful entry");
	f.groups.groups[0].z = 1;
	CHECK(allowed(enter) == CoopCampaignActionOutcome::Applied, "an underground level may be entered peacefully");
	f.groups.groups[0].z = 0; f.groups.groups[0].vehicle = true;
	CHECK(allowed(enter) == CoopCampaignActionOutcome::Unsupported, "vehicle groups await native vehicle support");
	f.groups.groups[0].vehicle = false; f.groups.groups[0].betweenSectors = true;
	f.groups.groups[0].nextX = 10; f.groups.groups[0].nextY = 1;
	CHECK(allowed(enter) == CoopCampaignActionOutcome::Unavailable, "a travelling group cannot enter a sector");
	f.groups.groups[0].betweenSectors = false; f.groups.groups[0].nextX = f.groups.groups[0].nextY = 0;
	CoopCampaignArrival npc; npc.decision = 1; npc.kind = CoopCampaignArrivalKind::WildernessNpc;
	npc.stage = CoopCampaignArrivalStage::Pending; npc.x = 10; npc.y = 1; npc.pendingCount = 1;
	f.observeArrival(npc);
	enter.controlRevision = f.status.value().timeControlRevision;
	CHECK(allowed(enter) == CoopCampaignActionOutcome::Unavailable, "a pending arrival decision comes first");
}

void TakeExitGridFromSecuredSector()
{
	Fixture f;
	const auto allowed = [&](const CoopCampaignActionRequest& r) {
		return ValidateCoopCampaignActionRequest(r, f.status.value(), f.groups, f.identities[1], true, true, false);
	};
	const auto exitRequest = [&](std::uint64_t decision) {
		auto r = f.arrival(CoopCampaignAction::TakeExitGrid); r.decision = decision; r.group = f.groups.groups[0].id;
		return r;
	};
	CoopCampaignActionRequestBytes bytes{}; CoopCampaignActionRequest decoded;
	CHECK(EncodeCoopCampaignActionRequest(exitRequest(5), bytes) && bytes[6] == 13 &&
		DecodeCoopCampaignActionRequest(bytes.data(), bytes.size(), decoded) && SameCoopCampaignActionRequest(decoded, exitRequest(5)),
		"take-exit-grid is kind 13 and roundtrips the secured sector and exact group");
	auto noGroup = exitRequest(5); noGroup.group = {};
	auto withDestination = exitRequest(5); withDestination.destinationX = 9;
	CHECK(!ValidCoopCampaignActionRequest(noGroup) && !ValidCoopCampaignActionRequest(exitRequest(0)) &&
		!ValidCoopCampaignActionRequest(withDestination), "exit-grid requests carry only a sector token and a group");
	CHECK(allowed(exitRequest(5)) == CoopCampaignActionOutcome::Unavailable, "strategic play has no exit grid to take");
	auto clock = f.status.value(); clock.phase = CoopCampaignPhase::Tactical; clock.securedSector = 5;
	CHECK(f.status.observe(clock, f.identities, 2), "observe secured sector");
	CHECK(allowed(exitRequest(5)) == CoopCampaignActionOutcome::Applied, "any ready player may take a secured exit grid");
	CHECK(allowed(exitRequest(4)) == CoopCampaignActionOutcome::Stale, "older secured sector is stale");
	auto otherGroup = exitRequest(5); otherGroup.group = {9, 9};
	CHECK(allowed(otherGroup) == CoopCampaignActionOutcome::Stale, "unknown group is stale");
	f.groups.groups[0].vehicle = true;
	CHECK(allowed(exitRequest(5)) == CoopCampaignActionOutcome::Unsupported, "vehicle groups cannot take exit grids");
	f.groups.groups[0].vehicle = false; f.groups.available = false; f.groups.groupCount = f.groups.memberCount = 0;
	CHECK(allowed(exitRequest(5)) == CoopCampaignActionOutcome::Unavailable, "exit grids need a group observation");
}

void SetAssignmentOnTheMap()
{
	Fixture f;
	const auto allowed = [&](const CoopCampaignActionRequest& r, bool strategic = true) {
		return ValidateCoopCampaignActionRequest(r, f.status.value(), f.groups, f.identities[1], true, true, strategic);
	};
	auto assign = f.request(); assign.action = CoopCampaignAction::SetAssignment; assign.group = {};
	assign.decision = 0x0003u | (std::uint64_t(0x01020304u) << 16); assign.destinationX = 5; assign.destinationY = 4;
	CoopCampaignActionRequestBytes bytes{}; CoopCampaignActionRequest decoded;
	CHECK(EncodeCoopCampaignActionRequest(assign, bytes) && bytes[6] == 14 && bytes[40] == 3 && bytes[42] == 4 &&
		bytes[45] == 1 && bytes[49] == 5 && bytes[50] == 4 &&
		DecodeCoopCampaignActionRequest(bytes.data(), bytes.size(), decoded) && SameCoopCampaignActionRequest(decoded, assign),
		"set-assignment is kind 14 and packs the merc (slot, incarnation), code and parameter");
	auto noMerc = assign; noMerc.decision = 0x0003u;
	auto noSlot = assign; noSlot.decision = 0xFFFFu | (std::uint64_t(7) << 16);
	auto wide = assign; wide.decision |= std::uint64_t(1) << 48;
	auto unknown = assign; unknown.destinationX = 9;
	auto none = assign; none.destinationX = 0;
	auto grouped = assign; grouped.group = f.groups.groups[0].id;
	CHECK(!ValidCoopCampaignActionRequest(noMerc) && !ValidCoopCampaignActionRequest(noSlot) &&
		!ValidCoopCampaignActionRequest(wide) && !ValidCoopCampaignActionRequest(unknown) &&
		!ValidCoopCampaignActionRequest(none) && !ValidCoopCampaignActionRequest(grouped),
		"an assignment names exactly one merc identity and a known co-op assignment code");
	CHECK(allowed(assign) == CoopCampaignActionOutcome::Applied, "any ready player may assign a merc on the map");
	CHECK(allowed(assign, false) == CoopCampaignActionOutcome::Unavailable, "a loaded tactical world blocks assignments");
	CoopCampaignArrival npc; npc.decision = 1; npc.kind = CoopCampaignArrivalKind::WildernessNpc;
	npc.stage = CoopCampaignArrivalStage::Pending; npc.x = 10; npc.y = 1; npc.pendingCount = 1;
	f.observeArrival(npc);
	assign.controlRevision = f.status.value().timeControlRevision;
	CHECK(allowed(assign) == CoopCampaignActionOutcome::Unavailable, "a pending arrival decision comes first");
}

void ContractDecisions()
{
	Fixture f;
	const auto allowed = [&](const CoopCampaignActionRequest& r, bool strategic = true) {
		return ValidateCoopCampaignActionRequest(r, f.status.value(), f.groups, f.identities[1], true, true, strategic);
	};
	const std::uint64_t merc = 0x0002u | (std::uint64_t(9) << 16);
	auto extend = f.request(); extend.action = CoopCampaignAction::ExtendContract; extend.group = {};
	extend.decision = merc; extend.destinationX = 2; extend.destinationY = 0;
	auto dismiss = extend; dismiss.action = CoopCampaignAction::DismissMerc; dismiss.destinationX = 0;
	CoopCampaignActionRequestBytes bytes{}; CoopCampaignActionRequest decoded;
	CHECK(EncodeCoopCampaignActionRequest(extend, bytes) && bytes[6] == 15 && bytes[49] == 2 &&
		DecodeCoopCampaignActionRequest(bytes.data(), bytes.size(), decoded) && SameCoopCampaignActionRequest(decoded, extend),
		"extend-contract is kind 15 with the packed merc and a length code");
	CHECK(EncodeCoopCampaignActionRequest(dismiss, bytes) && bytes[6] == 16 &&
		DecodeCoopCampaignActionRequest(bytes.data(), bytes.size(), decoded) && SameCoopCampaignActionRequest(decoded, dismiss),
		"dismiss is kind 16 with only the packed merc");
	auto tooLong = extend; tooLong.destinationX = 4;
	auto noLength = extend; noLength.destinationX = 0;
	auto dismissWithLength = dismiss; dismissWithLength.destinationX = 1;
	auto noMerc = dismiss; noMerc.decision = 0;
	CHECK(!ValidCoopCampaignActionRequest(tooLong) && !ValidCoopCampaignActionRequest(noLength) &&
		!ValidCoopCampaignActionRequest(dismissWithLength) && !ValidCoopCampaignActionRequest(noMerc),
		"contract lengths are 1-3 and a dismissal names only the merc");
	CHECK(allowed(extend) == CoopCampaignActionOutcome::Applied && allowed(dismiss) == CoopCampaignActionOutcome::Applied,
		"any ready player may extend or dismiss on the map");
	CHECK(allowed(extend, false) == CoopCampaignActionOutcome::Unavailable, "a loaded tactical world blocks contract decisions");
	auto clock = f.status.value(); clock.contractsDue = 1;
	CHECK(f.status.observe(clock, f.identities, 2), "observe an ended contract");
	extend.controlRevision = f.status.value().timeControlRevision;
	CHECK(allowed(extend) == CoopCampaignActionOutcome::Applied, "ended contracts are decided while the clock is held");
	CoopCampaignTimeRequest time{1, f.status.value().timeControlRevision, 1, CoopCampaignTimeAction::SixtyMinutes};
	CHECK(ValidateCoopCampaignTimeRequest(time, f.status.value(), f.status.value().timeLeader, true, true) ==
		CoopCampaignTimeOutcome::NativeBlocked && !IsZero(f.status.value().timeLeader),
		"time cannot run while a contract awaits its decision");
}

void TimeLeadTransfer()
{
	Fixture f;
	auto lead = f.request(); lead.action = CoopCampaignAction::TransferTimeLead; lead.group = {};
	lead.destinationX = lead.destinationY = 0;
	CoopCampaignActionRequestBytes bytes{}; CoopCampaignActionRequest decoded;
	CHECK(EncodeCoopCampaignActionRequest(lead, bytes) && bytes[6] == 17 &&
		DecodeCoopCampaignActionRequest(bytes.data(), bytes.size(), decoded) && SameCoopCampaignActionRequest(decoded, lead),
		"transfer-time-lead is kind 17 and carries no fields");
	auto noisy = lead; noisy.decision = 1;
	CHECK(!ValidCoopCampaignActionRequest(noisy), "a time-lead transfer names nothing");
	const auto& status = f.status.value();
	const auto leader = status.timeLeader;
	const auto other = leader == f.identities[0] ? f.identities[1] : f.identities[0];
	CHECK(ValidateCoopCampaignActionRequest(lead, status, f.groups, leader, true, true, true) == CoopCampaignActionOutcome::Applied,
		"the leader may hand the time controls to the other ready player");
	CHECK(ValidateCoopCampaignActionRequest(lead, status, f.groups, other, true, true, true) == CoopCampaignActionOutcome::Unauthorized,
		"nobody may take the controls from a ready leader");
	auto clock = status; clock.readyPeers = 1;
	CoopCampaignStatusLedger offline; CHECK(offline.beginSession(1), "third ledger");
	const PeerIdentity both[] = {f.identities[0], f.identities[1]};
	CHECK(offline.observe(clock, both, 2), "both ready first");
	const auto elected = offline.value().timeLeader;
	const auto claimer = elected == f.identities[0] ? f.identities[1] : f.identities[0];
	const PeerIdentity rest[] = {claimer};
	CHECK(offline.observe(clock, rest, 1) && !offline.value().timeLeaderReady, "the elected leader went offline");
	auto claim = lead; claim.controlRevision = offline.value().timeControlRevision;
	CHECK(ValidateCoopCampaignActionRequest(claim, offline.value(), f.groups, claimer, true, true, true) ==
		CoopCampaignActionOutcome::Applied, "a ready player may take the controls from an offline leader");
	CHECK(ValidateCoopCampaignActionRequest(claim, offline.value(), f.groups, elected, true, true, true) ==
		CoopCampaignActionOutcome::Unavailable, "a lone leader has nobody to hand the controls to");
	const auto revision = offline.value().leadershipRevision, control = offline.value().timeControlRevision;
	CHECK(offline.transferTimeLeadership(claimer) && offline.value().timeLeader == claimer && offline.value().timeLeaderReady &&
		offline.value().leadershipRevision == revision + 1 && offline.value().timeControlRevision == control + 1,
		"the transfer is a new leadership and control revision");
	CHECK(!offline.transferTimeLeadership(claimer) && !offline.transferTimeLeadership({}),
		"no transfer to the current leader or to nobody");
}

void ReceiptsAndSerialization()
{
	Fixture f; unsigned executions = 0;
	auto apply = [&](const CoopCampaignActionRequest& r) {
		++executions;
		CHECK(f.status.value().timeControlRevision > r.controlRevision && f.authority.deliveries()[1].pending,
			"global barrier and bounded receipt are reserved before native mutation");
		return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::Applied, 0};
	};
	auto first = f.request();
	CHECK(f.authority.submit(first, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 1, "non-time-leader can execute shared campaign action");
	CHECK(f.authority.submit(first, f.peers[0], true, true, true, f.status, f.groups, apply) && executions == 1 &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Stale, "competing peer request against same campaign barrier cannot execute");
	f.authority.delivered(0);
	CHECK(f.authority.submit(first, f.peers[1], false, false, false, f.status, f.groups, apply) && executions == 1 &&
		f.authority.deliveries()[1].result.outcome == CoopCampaignActionOutcome::Applied, "exact duplicate preserves historical success through resync");
	auto second = f.request(2);
	CHECK(!f.authority.submit(second, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 1, "pending receipt backpressures newer request");
	f.authority.delivered(1);
	auto conflict = f.request();
	CHECK(!f.authority.submit(conflict, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 1,
		"same request id with refreshed revision cannot execute");
	CHECK(f.authority.submit(second, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 2, "newer request after delivery uses fresh barrier");
	f.authority.delivered(1);
	CHECK(!f.authority.submit(first, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 2, "old replay cannot replace most recent receipt");
	conflict = f.request();
	CHECK(!f.authority.submit(conflict, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 2, "older request id with current revisions cannot execute");
	auto nativeReject = [&](const CoopCampaignActionRequest&) { ++executions; return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::NativeRejected, 9}; };
	auto rejected = f.request(3);
	CHECK(f.authority.submit(rejected, f.peers[1], true, true, true, f.status, f.groups, nativeReject) && executions == 3 &&
		f.authority.deliveries()[1].result.outcome == CoopCampaignActionOutcome::NativeRejected && f.authority.deliveries()[1].result.nativeDetail == 9 &&
		f.status.value().timeControlRevision > rejected.controlRevision, "native refusal consumes barrier and captures exact native reason");
	f.authority.delivered(1);
	CHECK(f.authority.submit(rejected, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 3 &&
		f.authority.deliveries()[1].result.outcome == CoopCampaignActionOutcome::NativeRejected, "rejected duplicate never springs to life later");
	f.authority.delivered(1);
	auto unready = f.request(4);
	CHECK(f.authority.submit(unready, f.peers[1], false, true, true, f.status, f.groups, apply) && executions == 3 &&
		f.authority.deliveries()[1].result.outcome == CoopCampaignActionOutcome::NotReady &&
		f.authority.reconcile(f.peers, 2) && f.authority.deliveries()[1].pending, "unready terminal outcome survives same-transport resync");
	f.authority.delivered(1);
	CHECK(f.authority.submit(unready, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 3 &&
		f.authority.deliveries()[1].result.outcome == CoopCampaignActionOutcome::NotReady, "later readiness does not execute rejected request");
	f.authority.delivered(1);
	auto maximum = f.request(UINT64_MAX);
	CHECK(f.authority.submit(maximum, f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 4, "last representable request id executes once");
	f.authority.delivered(1);
	CHECK(!f.authority.submit(f.request(1), f.peers[1], true, true, true, f.status, f.groups, apply) && executions == 4, "request id cannot wrap to one on same transport");
}
void TransportAndFailure()
{
	Fixture f; unsigned executions = 0;
	auto apply = [&](const CoopCampaignActionRequest&) { ++executions; return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::Applied, 0}; };
	auto r = f.request();
	CHECK(!f.authority.submit(r, {}, true, true, true, f.status, f.groups, apply) &&
		!f.authority.submit(r, {Identity(9), TransportPeer{9}}, true, true, true, f.status, f.groups, apply) && !executions,
		"empty and unregistered authenticated bindings cannot reserve receipts or execute");
	CHECK(f.authority.submit(r, f.peers[0], true, true, true, f.status, f.groups, apply), "pending old transport receipt");
	const auto saved = f.authority.deliveries()[0];
	CoopCampaignActionAuthority::Peer bad[2]{f.peers[0], f.peers[0]};
	CHECK(!f.authority.reconcile(bad, 2) && f.authority.deliveries()[0].pending &&
		SameCoopCampaignActionRequest(saved.result.request, f.authority.deliveries()[0].result.request), "bad peer reconciliation preserves receipts");
	bad[1] = f.peers[1]; bad[1].transport = bad[0].transport;
	CHECK(!f.authority.reconcile(bad, 2) && !f.authority.reconcile(nullptr, 1) && !f.authority.reconcile(f.peers, 5), "duplicate transports, missing and oversized peer arrays rejected");
	const auto old = f.peers[0]; f.peers[0].transport = TransportPeer{3};
	CHECK(f.authority.reconcile(f.peers, 2) && !f.authority.deliveries()[0].pending &&
		!f.authority.submit(f.request(2), old, true, true, true, f.status, f.groups, apply) && executions == 1,
		"replacement transport drops old receipt and queued old-transport request");
	CHECK(f.authority.submit(f.request(), f.peers[0], true, true, true, f.status, f.groups, apply) && executions == 2,
		"replacement transport starts its own request sequence with current observations");
	f.authority.delivered(0);
	auto fail = [&](const CoopCampaignActionRequest&) { ++executions; return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::Failed, 10}; };
	r = f.request(2);
	CHECK(f.authority.submit(r, f.peers[0], true, true, true, f.status, f.groups, fail) && f.authority.failed() && executions == 3 &&
		f.authority.deliveries()[0].result.nativeDetail == 10, "native failure latches fail-stop with terminal native detail");
	f.authority.delivered(0);
	CHECK(f.authority.submit(f.request(3), f.peers[0], true, true, true, f.status, f.groups, apply) && executions == 3 &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Failed, "fresh requests cannot mutate after a native partial failure");
	CHECK(f.authority.reconcile(nullptr, 0) && f.authority.reconcile(f.peers, 2) && f.authority.failed(), "disconnect/reconnect cannot clear runtime failure");
	f.authority.clear(); CHECK(!f.authority.failed() && f.authority.reconcile(f.peers, 2), "explicit new-session reset clears failure and receipts");
	auto throws = [&](const CoopCampaignActionRequest&) -> CoopCampaignActionNativeResult { ++executions; throw std::runtime_error("native failure"); };
	CHECK(f.authority.submit(f.request(), f.peers[0], true, true, true, f.status, f.groups, throws) && f.authority.failed() && executions == 4 &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Failed, "unexpected native exception produces fail-stop receipt");
	f.authority.clear(); CHECK(f.authority.reconcile(f.peers, 2), "clear exception fixture");
	auto invalid = [&](const CoopCampaignActionRequest&) { return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::Stale, 100}; };
	CHECK(f.authority.submit(f.request(), f.peers[0], true, true, true, f.status, f.groups, invalid) && f.authority.failed() &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Failed && !f.authority.deliveries()[0].result.nativeDetail,
		"unexpected callback status cannot claim a successful or unconsumed action");
}
void BattleNoticeSerialization()
{
	Fixture f;
	auto clock = f.status.value(); clock.phase = CoopCampaignPhase::Tactical;
	clock.battleNotice = {8, CoopCampaignBattleNoticeKind::Defeated, 9, 1, 0};
	CHECK(f.status.observe(clock, f.identities, 2), "native battle outcome holds tactical simulation");
	auto request = f.arrival(CoopCampaignAction::AcknowledgeBattleNotice); request.decision = 8;
	for (unsigned peer = 0; peer != 2; ++peer)
		CHECK(ValidateCoopCampaignActionRequest(request, f.status.value(), f.groups, f.identities[peer], true, true, false) ==
			CoopCampaignActionOutcome::Applied, "either ready player can acknowledge the battle without time leadership");
	CHECK(ValidateCoopCampaignActionRequest(request, f.status.value(), f.groups, f.identities[1], false, true, false) ==
		CoopCampaignActionOutcome::NotReady, "battle acknowledgement requires readiness");
	CHECK(ValidateCoopCampaignActionRequest(request, f.status.value(), f.groups, f.identities[1], true, false, false) ==
		CoopCampaignActionOutcome::Unauthorized, "battle acknowledgement requires campaign policy authorization");
	unsigned executions = 0;
	auto apply = [&](const auto&) { ++executions; return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::Applied, 0}; };
	CHECK(f.authority.submit(request, f.peers[1], true, true, false, f.status, f.groups, apply) && executions == 1,
		"nonleader acknowledgement consumes one shared control revision before native mutation");
	CHECK(f.authority.submit(request, f.peers[0], true, true, false, f.status, f.groups, apply) && executions == 1 &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Stale, "competing peer cannot acknowledge twice");
	clock = f.status.value(); clock.battleNotice = {}; clock.phase = CoopCampaignPhase::Transition;
	CHECK(f.status.observe(clock, f.identities, 2), "consumed notice leaves a paused transition");
	CHECK(f.authority.submit(request, f.peers[1], false, false, false, f.status, f.groups, apply) && executions == 1 &&
		f.authority.deliveries()[1].result.outcome == CoopCampaignActionOutcome::Applied,
		"duplicate acknowledgement replays retained receipt without rerunning native continuation");
	f.authority.delivered(0); ++request.requestId; request.controlRevision = f.status.value().timeControlRevision;
	CHECK(f.authority.submit(request, f.peers[0], true, true, false, f.status, f.groups, apply) && executions == 1 &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Unavailable,
		"fresh request cannot restart an acknowledged transition");
	clock.phase = CoopCampaignPhase::Tactical;
	CHECK(f.status.observe(clock, f.identities, 2), "new tactical context without any notice");
	f.authority.delivered(0); ++request.requestId; request.controlRevision = f.status.value().timeControlRevision;
	CHECK(f.authority.submit(request, f.peers[0], true, true, false, f.status, f.groups, apply) && executions == 1 &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Stale, "old notice cannot target a later battle");
}

void SurrenderSerialization()
{
	Fixture f;
	auto clock = f.status.value(); clock.phase = CoopCampaignPhase::Tactical; clock.surrenderOffer = 8;
	CHECK(f.status.observe(clock, f.identities, 2), "native tactical surrender hold publishes");
	auto request = f.arrival(CoopCampaignAction::DeclineSurrender); request.decision = 8;
	for (unsigned peer = 0; peer != 2; ++peer)
		CHECK(ValidateCoopCampaignActionRequest(request, f.status.value(), f.groups, f.identities[peer], true, true, false) ==
			CoopCampaignActionOutcome::Applied, "either ready player can answer without strategic or time-leader authority");
	CHECK(ValidateCoopCampaignActionRequest(request, f.status.value(), f.groups, f.identities[1], false, true, false) ==
		CoopCampaignActionOutcome::NotReady, "tactical dialog still requires campaign readiness");
	CHECK(ValidateCoopCampaignActionRequest(request, f.status.value(), f.groups, f.identities[1], true, false, false) ==
		CoopCampaignActionOutcome::Unauthorized, "authorization remains distinct from readiness");
	unsigned executed = 0;
	auto apply = [&](const auto&) { ++executed; return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::Applied, 0}; };
	CHECK(f.authority.submit(request, f.peers[1], true, true, false, f.status, f.groups, apply) && executed == 1,
		"nonleader answer consumes the shared barrier before one native call");
	auto competing = request; competing.action = CoopCampaignAction::AcceptSurrender;
	CHECK(f.authority.submit(competing, f.peers[0], true, true, false, f.status, f.groups, apply) && executed == 1 &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Stale,
		"simultaneous conflicting answer cannot run the continuation twice");
	clock = f.status.value(); clock.surrenderOffer = 0;
	CHECK(f.status.observe(clock, f.identities, 2), "native completion clears the offer before the next request");
	CHECK(f.authority.submit(request, f.peers[1], false, false, false, f.status, f.groups, apply) && executed == 1 &&
		f.authority.deliveries()[1].result.outcome == CoopCampaignActionOutcome::Applied,
		"duplicate after completion/resync returns the historical outcome without replay");
	f.authority.delivered(0); ++competing.requestId; competing.controlRevision = f.status.value().timeControlRevision;
	CHECK(f.authority.submit(competing, f.peers[0], true, true, false, f.status, f.groups, apply) && executed == 1 &&
		f.authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Stale,
		"fresh request ID cannot answer an offer that another peer consumed");
}

}
int main() { BattleNoticeSerialization();  Codec(); SurrenderSerialization(); PolicyAndState(); ReturnToMapFromSecuredSector(); EnterSectorForPeacefulPlay(); TakeExitGridFromSecuredSector(); SetAssignmentOnTheMap(); ContractDecisions(); TimeLeadTransfer(); ReceiptsAndSerialization(); TransportAndFailure(); return failures ? 1 : 0; }
