#ifndef ENGINE_CORE_UNIQUE_RESOURCE_HANDLE_H
#define ENGINE_CORE_UNIQUE_RESOURCE_HANDLE_H

#include <cstdint>

// Dependency-free move-only ownership for numeric registry handles. Value and
// InvalidValue adapt APIs that use a signed -1 sentinel while preserving the
// established unsigned-zero default.
template <typename Tag, typename Releaser,
	typename Value = std::uint32_t, Value InvalidValue = Value{}>
class UniqueResourceHandle
{
public:
	UniqueResourceHandle() = default;
	explicit UniqueResourceHandle(Value value) : value_(value) {}
	~UniqueResourceHandle() { reset(); }

	UniqueResourceHandle(const UniqueResourceHandle&) = delete;
	UniqueResourceHandle& operator=(const UniqueResourceHandle&) = delete;

	UniqueResourceHandle(UniqueResourceHandle&& other) noexcept : value_(other.release()) {}
	UniqueResourceHandle& operator=(UniqueResourceHandle&& other) noexcept
	{
		if (this != &other) reset(other.release());
		return *this;
	}

	explicit operator bool() const { return value_ != InvalidValue; }
	Value get() const { return value_; }

	Value release()
	{
		const Value value = value_;
		value_ = InvalidValue;
		return value;
	}

	void reset(Value replacement = InvalidValue)
	{
		if (value_ != InvalidValue && value_ != replacement) Releaser{}(value_);
		value_ = replacement;
	}

private:
	Value value_ = InvalidValue;
};

#endif
