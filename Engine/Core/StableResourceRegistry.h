#ifndef ENGINE_CORE_STABLE_RESOURCE_REGISTRY_H
#define ENGINE_CORE_STABLE_RESOURCE_REGISTRY_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

// Owns move-only resources behind stable numeric handles. Handles are never
// reused during one registry lifetime, which keeps stale legacy IDs from
// silently resolving to an unrelated resource. clear() starts a new lifetime
// and restores the configured first handle.
template <typename Resource, typename Handle = std::uint32_t>
class StableResourceRegistry
{
public:
	struct Limits
	{
		Handle first = 1;
		Handle stride = 1;
		Handle maximum = std::numeric_limits<Handle>::max();
		std::size_t capacity = std::numeric_limits<std::size_t>::max();
	};

	explicit StableResourceRegistry(Limits limits = {})
		: limits_(limits), next_(limits.first), valid_(limits.first != 0 &&
			limits.stride != 0 && limits.first <= limits.maximum)
	{
	}

	StableResourceRegistry(const StableResourceRegistry&) = delete;
	StableResourceRegistry& operator=(const StableResourceRegistry&) = delete;

	std::optional<Handle> insert(Resource resource)
	{
		if (!valid_ || exhausted_ || resources_.size() >= limits_.capacity)
			return std::nullopt;

		const Handle handle = next_;
		const auto inserted = resources_.emplace(handle, std::move(resource));
		if (!inserted.second) return std::nullopt;

		if (limits_.stride > limits_.maximum - handle)
			exhausted_ = true;
		else
			next_ = static_cast<Handle>(handle + limits_.stride);
		return handle;
	}

	Resource* find(Handle handle)
	{
		const auto found = resources_.find(handle);
		return found == resources_.end() ? nullptr : &found->second;
	}

	const Resource* find(Handle handle) const
	{
		const auto found = resources_.find(handle);
		return found == resources_.end() ? nullptr : &found->second;
	}

	bool erase(Handle handle) { return resources_.erase(handle) != 0; }

	void clear()
	{
		resources_.clear();
		next_ = limits_.first;
		exhausted_ = false;
	}

	template <typename Visitor>
	void forEach(Visitor&& visitor)
	{
		for (auto& entry : resources_) visitor(entry.first, entry.second);
	}

	template <typename Visitor>
	void forEach(Visitor&& visitor) const
	{
		for (const auto& entry : resources_) visitor(entry.first, entry.second);
	}

	std::size_t size() const { return resources_.size(); }
	bool empty() const { return resources_.empty(); }
	Handle nextHandle() const { return next_; }
	bool exhausted() const { return exhausted_; }
	bool valid() const { return valid_; }

private:
	Limits limits_;
	Handle next_;
	bool valid_ = false;
	bool exhausted_ = false;
	std::unordered_map<Handle, Resource> resources_;
};

#endif
