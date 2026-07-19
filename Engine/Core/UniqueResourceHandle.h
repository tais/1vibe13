#ifndef ENGINE_CORE_UNIQUE_RESOURCE_HANDLE_H
#define ENGINE_CORE_UNIQUE_RESOURCE_HANDLE_H

#include <cstdint>

// Dependency-free move-only ownership for numeric registry handles.
template <typename Tag, typename Releaser>
class UniqueResourceHandle
{
public:
	UniqueResourceHandle() = default;
	explicit UniqueResourceHandle(std::uint32_t value) : value_(value) {}
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
	std::uint32_t get() const { return value_; }

	std::uint32_t release()
	{
		const std::uint32_t value = value_;
		value_ = 0;
		return value;
	}

	void reset(std::uint32_t replacement = 0)
	{
		if (value_ != 0 && value_ != replacement) Releaser{}(value_);
		value_ = replacement;
	}

private:
	std::uint32_t value_ = 0;
};

#endif
