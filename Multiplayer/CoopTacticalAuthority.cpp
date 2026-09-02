#include "CoopTacticalAuthority.h"

#include <algorithm>
#include <limits>

namespace CoopSession
{
namespace
{
bool ValidContext(const TacticalAuthorityContext& context) noexcept
{
	return context.sessionEpoch != 0 && context.worldGeneration != 0 &&
		context.revision != 0 && context.turnSerial != 0;
}
}

TacticalIntentAuthority::TacticalIntentAuthority(
	AdmissionRegistry& admission) noexcept
	: admission_(admission)
{
}

void TacticalIntentAuthority::resetAdmissionEpoch(
	std::uint64_t sessionEpoch) noexcept
{
	if (sequenceEpoch_ == sessionEpoch) return;
	sequenceEpoch_ = sessionEpoch;
	clearSequences();
}

TacticalAuthorityConfigurationResult TacticalIntentAuthority::beginSession(
	TacticalAuthorityContext context) noexcept
{
	if (!ValidContext(context))
	{
		configured_ = false;
		context_ = {};
		clearActorBindings();
		return TacticalAuthorityConfigurationResult::InvalidContext;
	}
	const AuthorityConfiguration& admissionConfiguration =
		admission_.configuration();
	if (!admissionConfiguration.enabled || !admissionConfiguration.complete() ||
		admissionConfiguration.sessionEpoch != context.sessionEpoch)
	{
		configured_ = false;
		context_ = {};
		clearActorBindings();
		return TacticalAuthorityConfigurationResult::AdmissionUnavailable;
	}
	resetAdmissionEpoch(context.sessionEpoch);
	context_ = context;
	configured_ = true;
	clearActorBindings();
	return TacticalAuthorityConfigurationResult::Success;
}

TacticalAuthorityConfigurationResult TacticalIntentAuthority::beginGeneration(
	std::uint64_t generation,
	std::uint64_t revision,
	std::uint64_t turnSerial) noexcept
{
	if (!configured_)
		return TacticalAuthorityConfigurationResult::AdmissionUnavailable;
	if (generation == 0 || revision == 0 || turnSerial == 0)
		return TacticalAuthorityConfigurationResult::InvalidContext;
	if (generation <= context_.worldGeneration)
		return TacticalAuthorityConfigurationResult::StaleContext;

	context_.worldGeneration = generation;
	context_.revision = revision;
	context_.turnSerial = turnSerial;
	clearActorBindings();
	return TacticalAuthorityConfigurationResult::Success;
}

TacticalAuthorityConfigurationResult TacticalIntentAuthority::advanceContext(
	std::uint64_t revision,
	std::uint64_t turnSerial) noexcept
{
	if (!configured_)
		return TacticalAuthorityConfigurationResult::AdmissionUnavailable;
	if (revision == 0 || turnSerial == 0)
		return TacticalAuthorityConfigurationResult::InvalidContext;
	if (revision < context_.revision || turnSerial < context_.turnSerial)
		return TacticalAuthorityConfigurationResult::StaleContext;

	context_.revision = revision;
	context_.turnSerial = turnSerial;
	return TacticalAuthorityConfigurationResult::Success;
}

TacticalActorBindingResult TacticalIntentAuthority::bindActor(
	const PeerIdentity& peer,
	TacticalEntityId actor) noexcept
{
	if (!configured_)
		return TacticalActorBindingResult::NotConfigured;
	if (IsZero(peer))
		return TacticalActorBindingResult::InvalidPeer;
	if (!actor.valid())
		return TacticalActorBindingResult::InvalidActor;

	const ActorBinding* existing = findActor(actor);
	if (existing != nullptr)
	{
		return existing->peer == peer
			? TacticalActorBindingResult::Success
			: TacticalActorBindingResult::ActorAlreadyOwned;
	}
	if (actorBindingCount_ >= actorBindings_.size())
		return TacticalActorBindingResult::CapacityReached;

	ActorBinding binding;
	binding.peer = peer;
	binding.actor = actor;
	actorBindings_[actorBindingCount_++] = binding;
	return TacticalActorBindingResult::Success;
}

bool TacticalIntentAuthority::unbindActor(TacticalEntityId actor) noexcept
{
	for (std::size_t index = 0; index < actorBindingCount_; ++index)
	{
		if (actorBindings_[index].actor != actor) continue;
		std::move(actorBindings_.begin() + index + 1,
			actorBindings_.begin() + actorBindingCount_,
			actorBindings_.begin() + index);
		actorBindings_[--actorBindingCount_] = {};
		return true;
	}
	return false;
}

void TacticalIntentAuthority::clearActorBindings() noexcept
{
	actorBindings_ = {};
	actorBindingCount_ = 0;
}

TacticalIntentAuthorizationResult TacticalIntentAuthority::authorize(
	const TransportPeer& sender,
	const TacticalIntent& intent) noexcept
{
	TacticalIntentAuthorizationResult result;
	result.commandId = intent.commandId;
	if (!configured_)
		return result;

	PeerIdentity resolvedPeer{};
	if (!admission_.resolvePeerForIntent(
		sender, intent.sessionEpoch, resolvedPeer))
	{
		result.reason = TacticalIntentAuthorizationReason::NotAdmitted;
		return result;
	}
	result.peerIdentity = resolvedPeer;
	if (intent.commandId == 0)
	{
		result.reason = TacticalIntentAuthorizationReason::InvalidCommandId;
		return result;
	}
	if (!IsStructurallyValidTacticalIntent(intent))
	{
		result.reason = TacticalIntentAuthorizationReason::InvalidIntent;
		return result;
	}
	PeerSequence* sequence = findOrCreatePeerSequence(resolvedPeer);
	if (sequence == nullptr)
	{
		result.reason = TacticalIntentAuthorizationReason::PeerCapacityReached;
		return result;
	}
	result.nextExpectedCommandId = sequence->exhausted
		? 0 : sequence->nextCommandId;
	if (sequence->exhausted)
	{
		result.reason = TacticalIntentAuthorizationReason::SequenceExhausted;
		return result;
	}
	if (intent.commandId < sequence->nextCommandId)
	{
		result.reason = TacticalIntentAuthorizationReason::DuplicateCommand;
		return result;
	}
	if (intent.commandId > sequence->nextCommandId)
	{
		result.reason = TacticalIntentAuthorizationReason::OutOfOrderCommand;
		return result;
	}
	result.commandConsumed = true;
	if (sequence->nextCommandId == std::numeric_limits<std::uint64_t>::max())
	{
		sequence->exhausted = true;
		result.nextExpectedCommandId = 0;
	}
	else
	{
		++sequence->nextCommandId;
		result.nextExpectedCommandId = sequence->nextCommandId;
	}

	if (intent.sessionEpoch != context_.sessionEpoch)
	{
		result.reason = TacticalIntentAuthorizationReason::WrongSessionEpoch;
		return result;
	}
	if (intent.claimedPeerIdentity != resolvedPeer)
	{
		result.reason = TacticalIntentAuthorizationReason::ClaimedIdentityMismatch;
		return result;
	}
	if (intent.worldGeneration != context_.worldGeneration)
	{
		result.reason = TacticalIntentAuthorizationReason::WrongGeneration;
		return result;
	}
	if (intent.baseRevision < context_.revision)
	{
		result.reason = TacticalIntentAuthorizationReason::StaleRevision;
		return result;
	}
	if (intent.baseRevision > context_.revision)
	{
		result.reason = TacticalIntentAuthorizationReason::FutureRevision;
		return result;
	}
	if (intent.turnSerial < context_.turnSerial)
	{
		result.reason = TacticalIntentAuthorizationReason::StaleTurn;
		return result;
	}
	if (intent.turnSerial > context_.turnSerial)
	{
		result.reason = TacticalIntentAuthorizationReason::FutureTurn;
		return result;
	}
	if (!peerOwnsActor(resolvedPeer, intent.actor))
	{
		result.reason = TacticalIntentAuthorizationReason::ActorNotOwned;
		return result;
	}

	result.reason = TacticalIntentAuthorizationReason::None;
	return result;
}

bool TacticalIntentAuthority::canRetirePeerSequence(
	const PeerIdentity& peer) const noexcept
{
	return !IsZero(peer) && admission_.credentialRetired(peer);
}

bool TacticalIntentAuthority::retirePeerSequence(
	const PeerIdentity& peer) noexcept
{
	if (!canRetirePeerSequence(peer)) return false;
	std::size_t actorOutput = 0;
	for (std::size_t index = 0; index < actorBindingCount_; ++index)
	{
		if (actorBindings_[index].peer == peer) continue;
		if (actorOutput != index)
			actorBindings_[actorOutput] = actorBindings_[index];
		++actorOutput;
	}
	for (std::size_t index = actorOutput; index < actorBindingCount_; ++index)
		actorBindings_[index] = ActorBinding{};
	actorBindingCount_ = actorOutput;
	for (std::size_t index = 0; index < peerSequenceCount_; ++index)
	{
		if (peerSequences_[index].peer != peer) continue;
		std::move(peerSequences_.begin() + index + 1,
			peerSequences_.begin() + peerSequenceCount_,
			peerSequences_.begin() + index);
		peerSequences_[--peerSequenceCount_] = PeerSequence{};
		break;
	}
	return true;
}

const TacticalIntentAuthority::ActorBinding*
TacticalIntentAuthority::findActor(TacticalEntityId actor) const noexcept
{
	for (std::size_t index = 0; index < actorBindingCount_; ++index)
		if (actorBindings_[index].actor == actor) return &actorBindings_[index];
	return nullptr;
}

const TacticalIntentAuthority::PeerSequence*
TacticalIntentAuthority::findPeerSequence(
	const PeerIdentity& peer) const noexcept
{
	for (std::size_t index = 0; index < peerSequenceCount_; ++index)
		if (peerSequences_[index].peer == peer) return &peerSequences_[index];
	return nullptr;
}

TacticalIntentAuthority::PeerSequence*
TacticalIntentAuthority::findPeerSequence(const PeerIdentity& peer) noexcept
{
	return const_cast<PeerSequence*>(
		static_cast<const TacticalIntentAuthority&>(*this).findPeerSequence(peer));
}

TacticalIntentAuthority::PeerSequence*
TacticalIntentAuthority::findOrCreatePeerSequence(
	const PeerIdentity& peer) noexcept
{
	PeerSequence* existing = findPeerSequence(peer);
	if (existing != nullptr) return existing;
	if (peerSequenceCount_ >= peerSequences_.size()) return nullptr;
	PeerSequence sequence;
	sequence.peer = peer;
	peerSequences_[peerSequenceCount_++] = sequence;
	return &peerSequences_[peerSequenceCount_ - 1];
}

bool TacticalIntentAuthority::peerOwnsActor(
	const PeerIdentity& peer,
	TacticalEntityId actor) const noexcept
{
	const ActorBinding* binding = findActor(actor);
	return binding != nullptr && binding->peer == peer;
}

void TacticalIntentAuthority::clearSequences() noexcept
{
	peerSequences_ = {};
	peerSequenceCount_ = 0;
}
}
