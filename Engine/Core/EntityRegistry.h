#ifndef ENGINE_CORE_ENTITY_REGISTRY_H
#define ENGINE_CORE_ENTITY_REGISTRY_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

struct EntityId
{
	std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
	std::uint64_t generation = 0;

	bool valid() const
	{
		return index != std::numeric_limits<std::uint32_t>::max() && generation != 0;
	}
};

enum class EntityCreateError
{
	None,
	InvalidOwner,
	InvalidKind,
	CapacityReached,
	AllocationFailure
};

struct EntityCreateResult
{
	EntityCreateError error = EntityCreateError::None;
	EntityId id;

	explicit operator bool() const { return error == EntityCreateError::None && id.valid(); }
};

enum class EntityDestroyError
{
	None,
	InvalidId,
	NotAlive
};

struct EntityRecordSnapshot
{
	EntityId id;
	std::string ownerPackageId;
	std::string kind;
};

// Bounded generational identity registry. It owns no domain objects: packages
// and game adapters retain their data while sharing stale-safe handles across
// messages, commands, saves, and diagnostic boundaries.
class EntityRegistry
{
public:
	explicit EntityRegistry(std::size_t maximumEntities = 65536)
		: maximumEntities_(maximumEntities) {}

	EntityCreateResult create(std::string ownerPackageId, std::string kind) noexcept
	{
		if (!IsValidEngineIdentifier(ownerPackageId))
			return EntityCreateResult{EntityCreateError::InvalidOwner, {}};
		if (!IsValidEngineIdentifier(kind))
			return EntityCreateResult{EntityCreateError::InvalidKind, {}};

		if (!freeIndices_.empty())
		{
			const std::uint32_t index = freeIndices_.back();
			Slot& slot = slots_[index];
			try
			{
				slot.ownerPackageId = std::move(ownerPackageId);
				slot.kind = std::move(kind);
			}
			catch (...)
			{
				return EntityCreateResult{EntityCreateError::AllocationFailure, {}};
			}
			freeIndices_.pop_back();
			slot.alive = true;
			++aliveCount_;
			return EntityCreateResult{
				EntityCreateError::None, EntityId{index, slot.generation}};
		}

		if (slots_.size() < maximumEntities_ &&
			slots_.size() < std::numeric_limits<std::uint32_t>::max())
		{
			try
			{
				const std::uint32_t index = static_cast<std::uint32_t>(slots_.size());
				slots_.push_back(Slot{
					1, true, false, std::move(ownerPackageId), std::move(kind)});
				++aliveCount_;
				return EntityCreateResult{
					EntityCreateError::None, EntityId{index, 1}};
			}
			catch (...)
			{
				return EntityCreateResult{EntityCreateError::AllocationFailure, {}};
			}
		}

		// A failed free-list allocation during destroy must not permanently
		// consume capacity; recover such an unlisted dead slot by scanning only
		// on the otherwise-exhausted path.
		for (std::uint32_t index = 0; index < slots_.size(); ++index)
		{
			Slot& slot = slots_[index];
			if (slot.alive || slot.retired) continue;
			try
			{
				slot.ownerPackageId = std::move(ownerPackageId);
				slot.kind = std::move(kind);
			}
			catch (...)
			{
				return EntityCreateResult{EntityCreateError::AllocationFailure, {}};
			}
			slot.alive = true;
			++aliveCount_;
			return EntityCreateResult{
				EntityCreateError::None, EntityId{index, slot.generation}};
		}
		return EntityCreateResult{EntityCreateError::CapacityReached, {}};
	}

	EntityDestroyError destroy(EntityId id) noexcept
	{
		if (!id.valid() || id.index >= slots_.size()) return EntityDestroyError::InvalidId;
		Slot& slot = slots_[id.index];
		if (!slot.alive || slot.generation != id.generation)
			return EntityDestroyError::NotAlive;
		slot.alive = false;
		slot.ownerPackageId.clear();
		slot.kind.clear();
		--aliveCount_;
		if (slot.generation == std::numeric_limits<std::uint64_t>::max())
		{
			slot.retired = true;
			return EntityDestroyError::None;
		}
		++slot.generation;
		try
		{
			freeIndices_.push_back(id.index);
		}
		catch (...)
		{
			// The exhausted create path can rediscover this dead slot.
		}
		return EntityDestroyError::None;
	}

	bool alive(EntityId id) const
	{
		return id.valid() && id.index < slots_.size() && slots_[id.index].alive &&
			slots_[id.index].generation == id.generation;
	}

	std::size_t removePackage(const std::string& packageId) noexcept
	{
		std::size_t removed = 0;
		for (std::uint32_t index = 0; index < slots_.size(); ++index)
		{
			const Slot& slot = slots_[index];
			if (!slot.alive || slot.ownerPackageId != packageId) continue;
			const EntityId id{index, slot.generation};
			if (destroy(id) == EntityDestroyError::None) ++removed;
		}
		return removed;
	}

	std::vector<EntityRecordSnapshot> snapshot() const
	{
		std::vector<EntityRecordSnapshot> result;
		result.reserve(aliveCount_);
		for (std::uint32_t index = 0; index < slots_.size(); ++index)
		{
			const Slot& slot = slots_[index];
			if (!slot.alive) continue;
			result.push_back(EntityRecordSnapshot{
				EntityId{index, slot.generation}, slot.ownerPackageId, slot.kind});
		}
		return result;
	}

	std::size_t aliveCount() const { return aliveCount_; }
	std::size_t maximumEntities() const { return maximumEntities_; }

	static EntityRegistry& disabled()
	{
		static EntityRegistry registry(0);
		return registry;
	}

private:
	struct Slot
	{
		std::uint64_t generation;
		bool alive;
		bool retired;
		std::string ownerPackageId;
		std::string kind;
	};

	std::size_t maximumEntities_;
	std::vector<Slot> slots_;
	std::vector<std::uint32_t> freeIndices_;
	std::size_t aliveCount_ = 0;
};

#endif
