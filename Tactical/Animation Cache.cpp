#include "Animation Cache.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

#include "Animation Control.h"
#include "Animation Data.h"
#include "DEBUG.H"
#include "Debug Control.h"
#include "SoldierRepository.h"
#include "types.h"

UINT32 guiCacheSize		= MIN_CACHE_SIZE;

void DetermineOptimumAnimationCacheSize( )
{
	// If we have lots-a memory, adjust accordingly!
	guiCacheSize = MIN_CACHE_SIZE;
}

namespace
{
std::size_t EffectiveCacheCapacity() noexcept
{
	return std::clamp<std::size_t>(
		guiCacheSize, 1, MAX_CACHE_SIZE);
}

bool IsValidCacheOwner(SoldierID soldier) noexcept
{
	return soldier.i < MAX_NUM_SOLDIERS;
}
}

void SoldierAnimationCacheComponent::initialize(SoldierID soldier)
{
	AnimDebugMsg( String(
		"*** Initializing inline anim cache for soldier %d",
		soldier.i ) );
	if (!IsValidCacheOwner(soldier))
	{
		reset();
		return;
	}
	release(soldier);
	ClearAnimationSurfacesUsageHistory(soldier);
}

bool SoldierAnimationCacheComponent::acquire(
	SoldierID soldier, UINT16 surfaceIndex,
	UINT16 currentAnimation)
{
	if (!IsValidCacheOwner(soldier) ||
		surfaceIndex >= NUMANIMATIONSURFACETYPES)
		return false;

	const std::size_t capacity = EffectiveCacheCapacity();

	// Check to see if surface exists already
	for (std::size_t index = 0; index < capacity; ++index)
	{
		if (surfaces_[index] == surfaceIndex)
		{
			AnimDebugMsg( String(
				"Anim Cache: Hit %d ( Soldier %d )",
				surfaceIndex, soldier.i ) );
			if (hits_[index] < std::numeric_limits<INT16>::max())
				++hits_[index];
			return true;
		}
	}

	// Check if max size has been reached
	if (size_ >= capacity)
	{
		AnimDebugMsg( String(
			"Anim Cache: Determining Bump Candidate ( Soldier %d )",
			soldier.i ) );

		UINT16 currentSurface = EmptyEntry;
		if (TacticalActor* currentSoldier =
				GetJa2SoldierRepository().resolve(soldier.i))
		{
			currentSurface = DetermineSoldierAnimationSurface(
				currentSoldier, currentAnimation);
		}

		std::size_t candidate = capacity;
		INT16 fewestHits = std::numeric_limits<INT16>::max();
		for (std::size_t index = 0; index < capacity; ++index)
		{
			AnimDebugMsg( String(
				"Anim Cache: Slot %d Hits %d ( Soldier %d )",
				static_cast<unsigned>(index), hits_[index],
				soldier.i ) );

			if (surfaces_[index] == currentSurface)
			{
				AnimDebugMsg( String(
					"Anim Cache: REJECTING Slot %d EXISTING ANIM SURFACE ( Soldier %d )",
					static_cast<unsigned>(index), soldier.i ) );
			}
			else if (surfaces_[index] != EmptyEntry &&
					 (candidate == capacity ||
					  hits_[index] < fewestHits))
			{
				fewestHits = hits_[index];
				candidate = index;
			}
		}

		if (candidate == capacity) return false;
		AnimDebugMsg( String(
			"Anim Cache: Bumping %d ( Soldier %d )",
			static_cast<unsigned>(candidate), soldier.i ) );
		UnLoadAnimationSurface(soldier, surfaces_[candidate]);
		hits_[candidate] = 0;
		surfaces_[candidate] = EmptyEntry;
		--size_;
	}

	for (std::size_t index = 0; index < capacity; ++index)
	{
		if (surfaces_[index] == EmptyEntry)
		{
			AnimDebugMsg( String(
				"Anim Cache: Loading Surface %d ( Soldier %d )",
				surfaceIndex, soldier.i ) );
			if (!LoadAnimationSurface(
					soldier, surfaceIndex, currentAnimation))
				return false;
			hits_[index] = 0;
			surfaces_[index] = surfaceIndex;
			++size_;
			return true;
		}
	}

	return false;
}

void SoldierAnimationCacheComponent::release(SoldierID soldier)
{
	if (!IsValidCacheOwner(soldier))
	{
		reset();
		return;
	}

	for (std::size_t index = 0; index < surfaces_.size(); ++index)
	{
		if (surfaces_[index] != EmptyEntry)
			UnLoadAnimationSurface(soldier, surfaces_[index]);
	}
	reset();
}

void SoldierAnimationCacheComponent::reset() noexcept
{
	surfaces_.fill(EmptyEntry);
	hits_.fill(0);
	size_ = 0;
}

bool SoldierAnimationCacheComponent::contains(
	UINT16 surfaceIndex) const noexcept
{
	if (surfaceIndex == EmptyEntry) return false;
	return std::find(
		surfaces_.begin(), surfaces_.end(),
		surfaceIndex) != surfaces_.end();
}

INT16 SoldierAnimationCacheComponent::hitCount(
	UINT16 surfaceIndex) const noexcept
{
	if (surfaceIndex == EmptyEntry) return 0;
	const auto found = std::find(
		surfaces_.begin(), surfaces_.end(), surfaceIndex);
	if (found == surfaces_.end()) return 0;
	return hits_[static_cast<std::size_t>(
		std::distance(surfaces_.begin(), found))];
}

void SoldierAnimationCacheComponent::swapStorage(
	SoldierAnimationCacheComponent& other) noexcept
{
	surfaces_.swap(other.surfaces_);
	hits_.swap(other.hits_);
	std::swap(size_, other.size_);
}
