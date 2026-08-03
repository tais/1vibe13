#ifndef ENGINE_CORE_RESOURCE_HANDLE_SET_H
#define ENGINE_CORE_RESOURCE_HANDLE_SET_H

#include <cstddef>
#include <utility>
#include <vector>

// Transactional collection for move-only resource handles. A staging set
// releases every acquired handle when a later acquisition fails; moving it to
// a process/page owner commits the complete collection.
template <typename Handle>
class ResourceHandleSet
{
public:
	using value_type = decltype(std::declval<const Handle&>().get());

	ResourceHandleSet() = default;
	ResourceHandleSet(const ResourceHandleSet&) = delete;
	ResourceHandleSet& operator=(const ResourceHandleSet&) = delete;
	ResourceHandleSet(ResourceHandleSet&&) noexcept = default;
	ResourceHandleSet& operator=(ResourceHandleSet&&) noexcept = default;

	bool add(Handle handle)
	{
		if (!handle) return false;
		handles_.push_back(std::move(handle));
		return true;
	}

	bool add(Handle handle, value_type& publishedValue)
	{
		if (!handle) return false;
		handles_.push_back(std::move(handle));
		publishedValue = handles_.back().get();
		return true;
	}

	void clear() { handles_.clear(); }
	bool empty() const { return handles_.empty(); }
	std::size_t size() const { return handles_.size(); }

private:
	std::vector<Handle> handles_;
};

#endif
