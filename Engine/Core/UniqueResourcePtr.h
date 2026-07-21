#ifndef ENGINE_CORE_UNIQUE_RESOURCE_PTR_H
#define ENGINE_CORE_UNIQUE_RESOURCE_PTR_H

// Dependency-free move-only ownership for opaque C/API pointers. Resource is
// the pointed-to API type and Releaser is a stateless callable accepting
// Resource*. This keeps SDL and other platform headers outside Engine/Core.
template <typename Resource, typename Releaser>
class UniqueResourcePtr
{
public:
	UniqueResourcePtr() = default;
	explicit UniqueResourcePtr(Resource* value) : value_(value) {}
	~UniqueResourcePtr() { reset(); }

	UniqueResourcePtr(const UniqueResourcePtr&) = delete;
	UniqueResourcePtr& operator=(const UniqueResourcePtr&) = delete;

	UniqueResourcePtr(UniqueResourcePtr&& other) noexcept : value_(other.release()) {}
	UniqueResourcePtr& operator=(UniqueResourcePtr&& other) noexcept
	{
		if (this != &other) reset(other.release());
		return *this;
	}

	explicit operator bool() const { return value_ != nullptr; }
	Resource* get() const { return value_; }
	Resource& operator*() const { return *value_; }
	Resource* operator->() const { return value_; }

	Resource* release()
	{
		Resource* value = value_;
		value_ = nullptr;
		return value;
	}

	void reset(Resource* replacement = nullptr)
	{
		if (value_ && value_ != replacement) Releaser{}(value_);
		value_ = replacement;
	}

private:
	Resource* value_ = nullptr;
};

#endif
