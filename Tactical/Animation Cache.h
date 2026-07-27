#ifndef __ANIMATION_CACHE_H
#define __ANIMATION_CACHE_H

#include <array>

#include "Overhead Types.h"

#define MAX_CACHE_SIZE		20
//#define MIN_CACHE_SIZE		2
#define MIN_CACHE_SIZE		4

class Ja2SoldierRepository;

// Historical save-v101 layout only. Current soldiers own a fixed-capacity
// SoldierAnimationCacheComponent and never copy these raw pointers.
typedef struct
{
	UINT16	*usCachedSurfaces;
	INT16		*sCacheHits;
	UINT8		ubCacheSize;

} AnimationSurfaceCacheType;

// Runtime animation-surface working set for one canonical soldier slot.
// Storage is inline: soldier creation cannot fail for cache allocation, whole
// soldier copies cannot alias heap buffers, and reset never leaks memory.
class SoldierAnimationCacheComponent
{
public:
	SoldierAnimationCacheComponent() noexcept { reset(); }
	SoldierAnimationCacheComponent(
		const SoldierAnimationCacheComponent&) noexcept
	{
		reset();
	}
	SoldierAnimationCacheComponent& operator=(
		const SoldierAnimationCacheComponent& other) noexcept
	{
		if (this != &other) reset();
		return *this;
	}

	void initialize(SoldierID soldier);
	bool acquire(
		SoldierID soldier, UINT16 surfaceIndex,
		UINT16 currentAnimation);
	void release(SoldierID soldier);
	void reset() noexcept;

	bool contains(UINT16 surfaceIndex) const noexcept;
	INT16 hitCount(UINT16 surfaceIndex) const noexcept;
	UINT8 size() const noexcept { return size_; }
	bool empty() const noexcept { return size_ == 0; }

private:
	friend class Ja2SoldierRepository;

	// Cache ownership follows the canonical repository slot because the global
	// animation usage table is indexed by that slot. Repository replacement
	// detaches and restores this storage around whole-record copies.
	void swapStorage(SoldierAnimationCacheComponent& other) noexcept;

	static constexpr UINT16 EmptyEntry = 65000;

	std::array<UINT16, MAX_CACHE_SIZE> surfaces_;
	std::array<INT16, MAX_CACHE_SIZE> hits_;
	UINT8 size_ = 0;
};

extern UINT32 guiCacheSize;

void DetermineOptimumAnimationCacheSize( );



#endif
