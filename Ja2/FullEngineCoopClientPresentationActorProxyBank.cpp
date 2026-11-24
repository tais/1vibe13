#include "FullEngineCoopClientPresentationActorProxyBank.h"

#include "Animation Control.h"
#include "Animation Data.h"
#include "MemMan.h"
#include "Render Palette Bank.h"
#include "Render Palette Effects.h"
#include "Soldier Palette.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "World Tile Map.h"
#include "worlddef.h"
#include "worldman.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>

namespace
{
static_assert(TOTAL_SOLDIERS == MAX_NUM_SOLDIERS,
	"presentation animation-cache ownership requires one real cache owner per repository slot");
static_assert(TOTAL_SOLDIERS <=
	MaximumFullEngineCoopClientPresentationActorSlots,
	"presentation directory capacity must cover the legacy renderer bank");
static_assert(TOTALBODYTYPES == TacticalActorBodyTypeCount,
	"presentation body types must match the renderer ABI");
static_assert(NUMANIMATIONSTATES == TacticalAnimationStateCount,
	"presentation animation states must match the renderer ABI");
static_assert(NUMANIMATIONSURFACETYPES == TacticalAnimationSurfaceCount,
	"presentation animation surfaces must match the renderer ABI");

using Result = FullEngineCoopClientPresentationActorProxyResult;
using DirectoryResult =
	FullEngineCoopClientPresentationActorDirectoryResult;

FullEngineCoopClientLegacyPresentationActorProxyServices&
DefaultServices() noexcept
{
	static FullEngineCoopClientLegacyPresentationActorProxyServices services;
	return services;
}

bool IsTerminatedPaletteId(const PaletteRepID& id) noexcept
{
	for (const char byte : id)
		if (byte == '\0') return true;
	return false;
}

bool PaletteTableIsBounded() noexcept
{
	if (guiNumReplacements > 256u ||
		(guiNumReplacements != 0 && gpPalRep == nullptr))
		return false;
	for (std::size_t index = 0; index < guiNumReplacements; ++index)
		if (!IsTerminatedPaletteId(gpPalRep[index].ID)) return false;
	return true;
}

bool AssignPaletteId(PaletteRepID& output,
	const TacticalActorPresentationSnapshot& presentation,
	TacticalActorPresentationFlag presenceFlag,
	std::uint8_t index) noexcept
{
	std::memset(output, 0, sizeof(output));
	if ((presentation.flags & static_cast<std::uint8_t>(presenceFlag)) == 0)
		return index == 0;
	if (index >= guiNumReplacements || gpPalRep == nullptr ||
		!IsTerminatedPaletteId(gpPalRep[index].ID))
		return false;
	std::memcpy(output, gpPalRep[index].ID, sizeof(output));
	output[sizeof(output) - 1] = '\0';
	return true;
}

bool PaletteReplacementIsUsable(const PaletteRepID& id) noexcept
{
	if (id[0] == '\0') return true;
	UINT8 index = 0;
	if (!GetPaletteRepIndexFromID(id, &index) ||
		index >= guiNumReplacements || gpPalRep == nullptr ||
		gpPaletteSubRanges == nullptr ||
		gpPalRep[index].ubType >= guiNumPaletteSubRanges)
		return false;
	const PaletteSubRangeType& range =
		gpPaletteSubRanges[gpPalRep[index].ubType];
	if (range.ubStart > range.ubEnd) return false;
	const std::size_t count =
		static_cast<std::size_t>(range.ubEnd) - range.ubStart + 1;
	return gpPalRep[index].ubPaletteSize >= count &&
		gpPalRep[index].r != nullptr && gpPalRep[index].g != nullptr &&
		gpPalRep[index].b != nullptr;
}

bool ApplyPaletteReplacement(SGPPaletteEntry* palette,
	const PaletteRepID& id) noexcept
{
	if (id[0] == '\0') return true;
	if (!PaletteReplacementIsUsable(id)) return false;
	PaletteRepID mutableId{};
	std::memcpy(mutableId, id, sizeof(mutableId));
	return SetPaletteReplacement(palette, mutableId) != FALSE;
}

bool DecodeDisplayName(
	const std::array<std::uint16_t,
		TacticalActorDisplayNameCodeUnits>& source,
	SoldierIdentityComponent::Name& destination) noexcept
{
	std::fill(std::begin(destination), std::end(destination), L'\0');
	std::size_t output = 0;
	for (std::size_t input = 0; input < source.size(); ++input)
	{
		const std::uint16_t first = source[input];
		if (first == 0) return true;
		if (output + 1 >= SOLDIER_NAME_LENGTH) return false;
		if constexpr (sizeof(CHAR16) == 2)
		{
			destination[output++] = static_cast<CHAR16>(first);
		}
		else
		{
			std::uint32_t scalar = first;
			if (first >= 0xd800u && first <= 0xdbffu)
			{
				if (++input >= source.size()) return false;
				const std::uint16_t second = source[input];
				if (second < 0xdc00u || second > 0xdfffu) return false;
				scalar = 0x10000u +
					((static_cast<std::uint32_t>(first) - 0xd800u) << 10) +
					(static_cast<std::uint32_t>(second) - 0xdc00u);
			}
			else if (first >= 0xdc00u && first <= 0xdfffu)
			{
				return false;
			}
			destination[output++] = static_cast<CHAR16>(scalar);
		}
	}
	return false;
}

template <typename Destination, typename Source>
bool Representable(Source value) noexcept
{
	const std::int64_t widened = static_cast<std::int64_t>(value);
	return widened >= static_cast<std::int64_t>(
		(std::numeric_limits<Destination>::min)()) &&
		widened <= static_cast<std::int64_t>(
			(std::numeric_limits<Destination>::max)());
}

bool ProjectActor(const TacticalActorSnapshot& source,
	TacticalActor& destination) noexcept
{
	const TacticalActorPresentationSnapshot& presentation =
		source.presentation;
	if (!source.id.valid() || source.id.slot >= TOTAL_SOLDIERS ||
		!source.active || !source.inSector ||
		(presentation.flags & TacticalActorRenderPosePresent) == 0 ||
		!Representable<INT8>(source.team) ||
		!Representable<UINT8>(source.profile) ||
		!Representable<INT8>(source.life) ||
		!Representable<INT8>(source.maximumLife) ||
		!Representable<INT8>(source.breath) ||
		!Representable<INT8>(source.maximumBreath) ||
		presentation.bodyType >= TOTALBODYTYPES ||
		source.animation >= NUMANIMATIONSTATES ||
		presentation.animationSurface >= NUMANIMATIONSURFACETYPES ||
		source.level < 0 || source.level > 1 ||
		source.direction >= NUM_WORLD_DIRECTIONS ||
		presentation.animationDirection < 0 ||
		presentation.animationDirection >= NUM_WORLD_DIRECTIONS)
		return false;

	destination.initialize();
	destination.identity().id() = SoldierID{source.id.slot};
	destination.identity().incarnation() = source.id.incarnation;
	destination.identity().bodyType() = presentation.bodyType;
	destination.identity().profile() = static_cast<UINT8>(source.profile);
	if (!DecodeDisplayName(
			presentation.displayNameUtf16,
			destination.identity().name()))
		return false;

	destination.roster().active() = TRUE;
	destination.roster().inSector() = TRUE;
	destination.roster().team() = static_cast<INT8>(source.team);
	destination.actionPoints().current() = source.actionPoints;
	destination.vitals().health() = static_cast<INT8>(source.life);
	destination.vitals().maximumHealth() =
		static_cast<INT8>(source.maximumLife);
	destination.vitals().breath() = static_cast<INT8>(source.breath);
	destination.vitals().maximumBreath() =
		static_cast<INT8>(source.maximumBreath);

	const FLOAT worldX = static_cast<FLOAT>(presentation.worldXQ8) /
		TacticalWorldCoordinateScale;
	const FLOAT worldY = static_cast<FLOAT>(presentation.worldYQ8) /
		TacticalWorldCoordinateScale;
	destination.position().setWorldCoordinates(worldX, worldY);
	destination.position().gridNo() = source.grid;
	destination.position().level() = source.level;
	destination.position().direction() = source.direction;
	destination.position().heightAdjustment() =
		presentation.heightAdjustment;
	destination.position().animationHeightAdjustment() =
		(presentation.flags & TacticalActorPositiveAnimationHeight) != 0
			? 1.0f : 0.0f;
	destination.animationPlayback().state() = source.animation;
	destination.animationPlayback().surface() =
		presentation.animationSurface;
	destination.animationPlayback().frame() = presentation.animationFrame;

	// Logical-body layer ordering has its own authoritative eight-way value.
	// High-resolution facing has no separate replicated field yet, so retain the
	// exact ordinary actor direction instead of inventing sub-direction motion.
	destination.movement().animationDirection() =
		presentation.animationDirection;
	destination.movement().setHighResolutionFacing(
		source.direction, source.direction);
	if ((presentation.flags & TacticalActorMultiTileNonZ) != 0)
		destination.status().set(SOLDIER_MULTITILE_NZ);
	if ((presentation.flags & TacticalActorMultiTileZ) != 0)
		destination.status().set(SOLDIER_MULTITILE_Z);
	destination.awareness().setVisibilityAndRendered(TRUE);
	return true;
}

Result MapDirectoryResult(DirectoryResult result) noexcept
{
	switch (result)
	{
		case DirectoryResult::Success: return Result::Success;
		case DirectoryResult::ActorSlotOutOfRange:
			return Result::ActorSlotOutOfRange;
		case DirectoryResult::DuplicateActorSlot:
			return Result::DuplicateActorSlot;
		case DirectoryResult::InvalidActorGrid:
			return Result::InvalidActorGrid;
		case DirectoryResult::InvalidSnapshot:
		case DirectoryResult::InvalidCapacity:
			return Result::InvalidSnapshot;
		case DirectoryResult::StalePlan:
			return Result::DirectoryCommitFailure;
	}
	return Result::InvalidSnapshot;
}
}

const char* FullEngineCoopClientPresentationActorProxyResultName(
	FullEngineCoopClientPresentationActorProxyResult result) noexcept
{
	switch (result)
	{
		case Result::Success: return "success";
		case Result::AllocationFailure: return "allocation failure";
		case Result::InvalidSnapshot: return "invalid snapshot";
		case Result::ActorSlotOutOfRange: return "actor slot out of range";
		case Result::DuplicateActorSlot: return "duplicate actor slot";
		case Result::InvalidActorGrid: return "invalid actor grid";
		case Result::MissingRenderPose: return "missing render pose";
		case Result::UnrepresentableActorState:
			return "unrepresentable actor state";
		case Result::RepositoryBindingConflict:
			return "repository binding conflict";
		case Result::ResourceFailure: return "render resource failure";
		case Result::NodeDetachFailure: return "merc node detach failure";
		case Result::NodeAttachFailure: return "merc node attach failure";
		case Result::DirectoryCommitFailure:
			return "actor directory commit failure";
		case Result::RollbackFailure: return "actor proxy rollback failure";
	}
	return "unknown actor proxy result";
}

bool FullEngineCoopClientLegacyPresentationActorProxyServices::
prepareExactResources(TacticalActor& proxy,
	const TacticalActorPresentationSnapshot& presentation,
	bool acquireAnimationSurface) noexcept
{
	const UINT16 surfaceIndex = presentation.animationSurface;
	if (surfaceIndex >= NUMANIMATIONSURFACETYPES ||
		proxy.animationPlayback().surface() != surfaceIndex ||
		(acquireAnimationSurface &&
		 !proxy.animationCache().acquire(proxy.identity().id(), surfaceIndex,
			proxy.animationPlayback().state())))
		return false;

	const AnimationSurfaceType& surface = gAnimSurfaceDatabase[surfaceIndex];
	if (surface.hVideoObject == nullptr ||
		surface.hVideoObject->pETRLEObject == nullptr ||
		surface.hVideoObject->pPaletteEntry == nullptr ||
		presentation.animationFrame >=
			surface.hVideoObject->usNumberOfObjects ||
		!PaletteTableIsBounded() ||
		!AssignPaletteId(proxy.renderState().headPalette(), presentation,
			TacticalActorHeadPalettePresent,
			presentation.headPaletteIndex) ||
		!AssignPaletteId(proxy.renderState().pantsPalette(), presentation,
			TacticalActorPantsPalettePresent,
			presentation.pantsPaletteIndex) ||
		!AssignPaletteId(proxy.renderState().vestPalette(), presentation,
			TacticalActorVestPalettePresent,
			presentation.vestPaletteIndex) ||
		!AssignPaletteId(proxy.renderState().skinPalette(), presentation,
			TacticalActorSkinPalettePresent,
			presentation.skinPaletteIndex))
		return false;

	RenderPaletteBank palette;
	SGPPaletteEntry* const base = static_cast<SGPPaletteEntry*>(
		MemAlloc(sizeof(SGPPaletteEntry) * RenderPaletteBank::EntryCount));
	if (base == nullptr) return false;
	palette.adoptBase8(base);
	std::memcpy(base, surface.hVideoObject->pPaletteEntry,
		sizeof(SGPPaletteEntry) * RenderPaletteBank::EntryCount);
	if (!ApplyPaletteReplacement(base, proxy.renderState().headPalette()) ||
		!ApplyPaletteReplacement(base, proxy.renderState().pantsPalette()) ||
		!ApplyPaletteReplacement(base, proxy.renderState().vestPalette()) ||
		!ApplyPaletteReplacement(base, proxy.renderState().skinPalette()))
		return false;
	PIXEL* const converted = Create16BPPPalette(base);
	if (converted == nullptr) return false;
	palette.adoptBase16(converted);
	if (!RenderPaletteEffects::populateActorShades(palette)) return false;
	proxy.palette().swapStorage(palette);
	return true;
}

void FullEngineCoopClientLegacyPresentationActorProxyServices::
releaseResources(TacticalActor& proxy) noexcept
{
	proxy.initialize();
}

bool FullEngineCoopClientLegacyPresentationActorProxyServices::attachMercNode(
	std::int32_t grid, TacticalActor& proxy) noexcept
{
	return AddMercToHead(grid, &proxy, FALSE) != FALSE;
}

bool FullEngineCoopClientLegacyPresentationActorProxyServices::detachMercNode(
	std::int32_t, TacticalActor& proxy) noexcept
{
	// RemoveMerc() only searches the supplied grid and reports FALSE both for a
	// genuine failure and for an already-absent node. Search the complete owned
	// renderer world so success establishes the stronger service contract: no
	// merc-layer node anywhere retains this TacticalActor pointer. This also
	// removes an unexpected placeholder/duplicate instead of letting proxy
	// storage be recycled underneath it.
	if (gpWorldLevelData == nullptr) return true;
	const UINT32 tileCount = GetWorldTileMapSize();
	for (UINT32 tile = 0; tile < tileCount; ++tile)
	{
		for (;;)
		{
			LEVELNODE* matching = nullptr;
			for (LEVELNODE* node = GetMapElement(tile).pMercHead;
				node != nullptr; node = node->pNext)
			{
				if (node->pSoldier == &proxy)
				{
					matching = node;
					break;
				}
			}
			if (matching == nullptr) break;
			const BOOLEAN placeholder =
				(matching->uiFlags & LEVELNODE_MERCPLACEHOLDER) != 0
					? TRUE : FALSE;
			if (RemoveMerc(static_cast<INT32>(tile), &proxy, placeholder) == FALSE)
				return false;
		}
	}
	return true;
}

struct FullEngineCoopClientPresentationActorProxyBank::Impl
{
	struct RepositoryBank
	{
		RepositoryBank() noexcept
			: repository(records.data(), slots.data(), records.size())
		{
			repository.initializeSlots();
		}

		std::array<TacticalActor, TOTAL_SOLDIERS> records;
		std::array<TacticalActor*, TOTAL_SOLDIERS> slots{};
		Ja2SoldierRepository repository;
		std::array<bool, TOTAL_SOLDIERS> prepared{};
		std::array<bool, TOTAL_SOLDIERS> attached{};
	};

	explicit Impl(
		FullEngineCoopClientPresentationActorProxyServices& selected) noexcept
		: services(selected)
	{
		registryNext = registryHead();
		registryHead() = this;
	}

	~Impl() noexcept
	{
		if (repositoryLeaseOwner() == this)
			repositoryLeaseOwner() = nullptr;
		Impl** link = &registryHead();
		while (*link != nullptr && *link != this)
			link = &(*link)->registryNext;
		if (*link == this) *link = registryNext;
	}

	static Impl*& registryHead() noexcept
	{
		static Impl* head = nullptr;
		return head;
	}

	static Impl*& repositoryLeaseOwner() noexcept
	{
		static Impl* owner = nullptr;
		return owner;
	}

	bool ownsRepository(const Ja2SoldierRepository* repository) const noexcept
	{
		return repository == &banks[0].repository ||
			repository == &banks[1].repository;
	}

	static bool isPresentationRepository(
		const Ja2SoldierRepository* repository) noexcept
	{
		for (const Impl* bank = registryHead(); bank != nullptr;
			bank = bank->registryNext)
			if (bank->ownsRepository(repository)) return true;
		return false;
	}

	bool canBeginActivation(
		const Ja2SoldierRepository* repository) const noexcept
	{
		const Impl* const owner = repositoryLeaseOwner();
		return (owner == nullptr || owner == this) &&
			!isPresentationRepository(repository);
	}

	bool acquireRepositoryLease() noexcept
	{
		Impl*& owner = repositoryLeaseOwner();
		if (owner != nullptr && owner != this) return false;
		owner = this;
		return true;
	}

	void releaseDetachedResources(RepositoryBank& bank) noexcept
	{
		for (std::size_t slot = 0; slot < bank.prepared.size(); ++slot)
		{
			// An attached node owns a raw pointer into records[slot]. Never reset
			// resources or that record until detachment has been proven.
			if (!bank.prepared[slot] || bank.attached[slot]) continue;
			services.releaseResources(bank.records[slot]);
			bank.prepared[slot] = false;
		}
	}

	bool detachAttachedNodes() noexcept
	{
		bool detached = true;
		for (RepositoryBank& bank : banks)
		{
			for (std::size_t slot = 0; slot < bank.attached.size(); ++slot)
			{
				if (!bank.attached[slot]) continue;
				if (services.detachMercNode(
						bank.records[slot].position().gridNo(),
						bank.records[slot]))
					bank.attached[slot] = false;
				else
					detached = false;
			}
		}
		// A prepared staging record can deliberately borrow the slot-indexed
		// animation-surface lock from the active record until a successful commit
		// transfers that cache. If any staging node remains attached, retain both
		// complete banks so its backing lock cannot be released out from under it.
		if (detached)
			for (RepositoryBank& bank : banks)
				releaseDetachedResources(bank);
		return detached;
	}

	bool hasAttachedNodes() const noexcept
	{
		for (const RepositoryBank& bank : banks)
			if (std::find(bank.attached.begin(), bank.attached.end(), true) !=
				bank.attached.end())
				return true;
		return false;
	}

	void releaseRepositoryLeaseIfSafe() noexcept
	{
		if (!active && !hasAttachedNodes() &&
			repositoryLeaseOwner() == this)
			repositoryLeaseOwner() = nullptr;
	}

	bool recoverQuarantine() noexcept
	{
		if (!quarantined) return true;
		if (!detachAttachedNodes()) return false;
		quarantined = false;
		releaseRepositoryLeaseIfSafe();
		return true;
	}

	void failClosed() noexcept
	{
		quarantined = !detachAttachedNodes();
		directory.reset();
		if (previousRepository != nullptr)
			BindJa2SoldierRepository(*previousRepository);
		previousRepository = nullptr;
		active = false;
		releaseRepositoryLeaseIfSafe();
	}

	FullEngineCoopClientPresentationActorProxyServices& services;
	RepositoryBank banks[2];
	FullEngineCoopClientPresentationActorDirectory directory;
	Ja2SoldierRepository* previousRepository = nullptr;
	std::size_t activeIndex = 0;
	bool active = false;
	bool quarantined = false;
	Impl* registryNext = nullptr;
};

FullEngineCoopClientPresentationActorProxyBank::
FullEngineCoopClientPresentationActorProxyBank() noexcept
	: FullEngineCoopClientPresentationActorProxyBank(DefaultServices())
{
}

FullEngineCoopClientPresentationActorProxyBank::
FullEngineCoopClientPresentationActorProxyBank(
	FullEngineCoopClientPresentationActorProxyServices& services) noexcept
{
	try
	{
		impl_ = new Impl(services);
	}
	catch (...)
	{
		impl_ = nullptr;
	}
}

FullEngineCoopClientPresentationActorProxyBank::~
FullEngineCoopClientPresentationActorProxyBank()
{
	teardown();
	if (impl_ != nullptr && impl_->hasAttachedNodes())
	{
		// A LEVELNODE still owns a raw pointer into one of these fixed records.
		// There is no safe way to move that record or reclaim its render resources
		// after the world manager refused detachment. Preserve the complete Impl as
		// a last-resort lifetime quarantine; a bounded leak on a corrupt/failing
		// teardown is preferable to a renderer use-after-free. The leaked Impl also
		// remains registered as the repository-lease owner, preventing another bank
		// from publishing into the renderer beside the orphaned node.
		impl_ = nullptr;
		return;
	}
	delete impl_;
}

void FullEngineCoopClientPresentationActorProxyBank::transferAnimationCache(
	TacticalActor& source, TacticalActor& destination) noexcept
{
	source.animationCache().swapStorage(destination.animationCache());
}

FullEngineCoopClientPresentationActorProxyResult
FullEngineCoopClientPresentationActorProxyBank::materialize(
	const TacticalWorldSnapshot& snapshot) noexcept
{
	if (impl_ == nullptr) return Result::AllocationFailure;
	Impl& state = *impl_;
	if (!state.recoverQuarantine()) return Result::RollbackFailure;
	if (state.active &&
		&GetJa2SoldierRepository() !=
			&state.banks[state.activeIndex].repository)
		return Result::RepositoryBindingConflict;
	if (!state.active &&
		!state.canBeginActivation(&GetJa2SoldierRepository()))
		return Result::RepositoryBindingConflict;

	FullEngineCoopClientPresentationActorDirectoryPlan directoryPlan;
	const DirectoryResult staged = state.directory.stage(
		snapshot, TOTAL_SOLDIERS, directoryPlan);
	if (staged != DirectoryResult::Success) return MapDirectoryResult(staged);

	const std::size_t stagingIndex = state.active ? 1 - state.activeIndex : 0;
	Impl::RepositoryBank& staging = state.banks[stagingIndex];
	state.releaseDetachedResources(staging);
	std::array<bool, TOTAL_SOLDIERS> transferCache{};

	Ja2SoldierRepository* const repositoryBefore = &GetJa2SoldierRepository();
	BindJa2SoldierRepository(staging.repository);
	for (const TacticalActorSnapshot& actor : snapshot.actors())
	{
		// The staged directory applies the exact native visibility policy,
		// including removal of dead actors whose graphic has left the grid.
		if (directoryPlan.desiredActorAt(actor.id.slot) != actor.id) continue;
		if ((actor.presentation.flags & TacticalActorRenderPosePresent) == 0)
		{
			state.releaseDetachedResources(staging);
			BindJa2SoldierRepository(*repositoryBefore);
			return Result::MissingRenderPose;
		}
		if (actor.id.slot >= TOTAL_SOLDIERS ||
			!ProjectActor(actor, staging.records[actor.id.slot]))
		{
			if (actor.id.slot < TOTAL_SOLDIERS)
				state.services.releaseResources(
					staging.records[actor.id.slot]);
			state.releaseDetachedResources(staging);
			BindJa2SoldierRepository(*repositoryBefore);
			return Result::UnrepresentableActorState;
		}

		const bool canTransfer = state.active &&
			state.banks[state.activeIndex].prepared[actor.id.slot] &&
			state.banks[state.activeIndex].records[actor.id.slot]
				.animationCache().contains(
					actor.presentation.animationSurface);
		if (!state.services.prepareExactResources(
				staging.records[actor.id.slot], actor.presentation,
				!canTransfer))
		{
			state.services.releaseResources(staging.records[actor.id.slot]);
			state.releaseDetachedResources(staging);
			BindJa2SoldierRepository(*repositoryBefore);
			return Result::ResourceFailure;
		}
		staging.prepared[actor.id.slot] = true;
		transferCache[actor.id.slot] = canTransfer;
	}
	BindJa2SoldierRepository(*repositoryBefore);

	Impl::RepositoryBank* old = state.active
		? &state.banks[state.activeIndex] : nullptr;
	std::array<bool, TOTAL_SOLDIERS> detachedOld{};
	if (old != nullptr)
	{
		for (std::size_t slot = 0; slot < old->attached.size(); ++slot)
		{
			if (!old->attached[slot]) continue;
			if (!state.services.detachMercNode(
					old->records[slot].position().gridNo(), old->records[slot]))
			{
				bool rollback = true;
				for (std::size_t restore = 0; restore < detachedOld.size(); ++restore)
				{
					if (!detachedOld[restore]) continue;
					if (!state.services.attachMercNode(
							old->records[restore].position().gridNo(),
							old->records[restore]))
						rollback = false;
					else
						old->attached[restore] = true;
				}
				state.releaseDetachedResources(staging);
				if (!rollback)
				{
					state.failClosed();
					return Result::RollbackFailure;
				}
				return Result::NodeDetachFailure;
			}
			old->attached[slot] = false;
			detachedOld[slot] = true;
		}
	}

	if (!state.active)
	{
		if (!state.acquireRepositoryLease())
		{
			state.releaseDetachedResources(staging);
			return Result::RepositoryBindingConflict;
		}
		state.previousRepository = repositoryBefore;
	}
	BindJa2SoldierRepository(staging.repository);
	bool attachmentFailed = false;
	for (std::size_t slot = 0; slot < staging.prepared.size(); ++slot)
	{
		if (!staging.prepared[slot]) continue;
		if (!state.services.attachMercNode(
				staging.records[slot].position().gridNo(),
				staging.records[slot]))
		{
			attachmentFailed = true;
			break;
		}
		staging.attached[slot] = true;
	}

	auto rollbackCommit = [&]() noexcept {
		bool rollback = true;
		for (std::size_t slot = 0; slot < staging.attached.size(); ++slot)
		{
			if (!staging.attached[slot]) continue;
			if (!state.services.detachMercNode(
					staging.records[slot].position().gridNo(),
					staging.records[slot]))
				rollback = false;
			else
				staging.attached[slot] = false;
		}
		if (old != nullptr)
		{
			BindJa2SoldierRepository(old->repository);
			for (std::size_t slot = 0; slot < detachedOld.size(); ++slot)
			{
				if (!detachedOld[slot]) continue;
				if (!state.services.attachMercNode(
						old->records[slot].position().gridNo(),
						old->records[slot]))
					rollback = false;
				else
					old->attached[slot] = true;
			}
		}
		else
		{
			BindJa2SoldierRepository(*repositoryBefore);
			state.previousRepository = nullptr;
			state.releaseRepositoryLeaseIfSafe();
		}
		// Keep any node whose detach failed marked and its resources alive so
		// failClosed() can make one final removal attempt. Releasing it here
		// would leave a renderer node pointing into an initialized record.
		if (rollback) state.releaseDetachedResources(staging);
		return rollback;
	};

	if (attachmentFailed)
	{
		if (!rollbackCommit())
		{
			state.failClosed();
			return Result::RollbackFailure;
		}
		return Result::NodeAttachFailure;
	}
	if (state.directory.commit(directoryPlan) != DirectoryResult::Success)
	{
		if (!rollbackCommit())
		{
			state.failClosed();
			return Result::RollbackFailure;
		}
		return Result::DirectoryCommitFailure;
	}

	if (old != nullptr)
	{
		for (std::size_t slot = 0; slot < transferCache.size(); ++slot)
			if (transferCache[slot])
				transferAnimationCache(
					old->records[slot], staging.records[slot]);
		state.releaseDetachedResources(*old);
	}
	state.activeIndex = stagingIndex;
	state.active = true;
	return Result::Success;
}

void FullEngineCoopClientPresentationActorProxyBank::teardown() noexcept
{
	if (impl_ == nullptr) return;
	Impl& state = *impl_;
	state.quarantined = !state.detachAttachedNodes();
	state.directory.reset();
	if (state.previousRepository != nullptr)
		BindJa2SoldierRepository(*state.previousRepository);
	state.previousRepository = nullptr;
	state.active = false;
	state.releaseRepositoryLeaseIfSafe();
}

bool FullEngineCoopClientPresentationActorProxyBank::active() const noexcept
{
	return impl_ != nullptr && impl_->active;
}

std::uint64_t FullEngineCoopClientPresentationActorProxyBank::
worldGeneration() const noexcept
{
	return impl_ != nullptr ? impl_->directory.worldGeneration() : 0;
}

const TacticalActor* FullEngineCoopClientPresentationActorProxyBank::proxy(
	std::size_t slot) const noexcept
{
	return impl_ != nullptr && impl_->active && slot < TOTAL_SOLDIERS &&
		impl_->banks[impl_->activeIndex].prepared[slot]
		? &impl_->banks[impl_->activeIndex].records[slot] : nullptr;
}

TacticalActor* FullEngineCoopClientPresentationActorProxyBank::proxy(
	std::size_t slot) noexcept
{
	return const_cast<TacticalActor*>(
		static_cast<const FullEngineCoopClientPresentationActorProxyBank&>(*this)
			.proxy(slot));
}

const FullEngineCoopClientPresentationActorDirectory&
FullEngineCoopClientPresentationActorProxyBank::directory() const noexcept
{
	static const FullEngineCoopClientPresentationActorDirectory empty;
	return impl_ != nullptr ? impl_->directory : empty;
}
