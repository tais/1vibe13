#ifndef SGP_RESOURCE_HANDLE_H
#define SGP_RESOURCE_HANDLE_H

#include "types.h"

// Move-only ownership for numeric handles managed by legacy SGP registries.
// The tag makes resource kinds distinct; the releaser bridges the old API.
template <typename Tag, typename Releaser>
class UniqueResourceHandle
{
public:
	UniqueResourceHandle() = default;
	explicit UniqueResourceHandle(UINT32 value) : value_(value) {}
	~UniqueResourceHandle() { reset(); }

	UniqueResourceHandle(const UniqueResourceHandle&) = delete;
	UniqueResourceHandle& operator=(const UniqueResourceHandle&) = delete;

	UniqueResourceHandle(UniqueResourceHandle&& other) noexcept : value_(other.release()) {}
	UniqueResourceHandle& operator=(UniqueResourceHandle&& other) noexcept
	{
		if (this != &other) reset(other.release());
		return *this;
	}

	explicit operator bool() const { return value_ != 0; }
	UINT32 get() const { return value_; }

	UINT32 release()
	{
		const UINT32 value = value_;
		value_ = 0;
		return value;
	}

	void reset(UINT32 replacement = 0)
	{
		if (value_ != 0 && value_ != replacement) Releaser{}(value_);
		value_ = replacement;
	}

private:
	UINT32 value_ = 0;
};

#endif
