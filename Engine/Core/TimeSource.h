#ifndef ENGINE_CORE_TIME_SOURCE_H
#define ENGINE_CORE_TIME_SOURCE_H

#include <cstdint>

class MonotonicTimeSource
{
public:
	virtual ~MonotonicTimeSource() = default;
	virtual std::uint64_t nowMicroseconds() const = 0;
};

class ManualTimeSource final : public MonotonicTimeSource
{
public:
	std::uint64_t nowMicroseconds() const override { return microseconds_; }
	void setMicroseconds(std::uint64_t value) { microseconds_ = value; }
	void advanceMicroseconds(std::uint64_t amount) { microseconds_ += amount; }

private:
	std::uint64_t microseconds_ = 0;
};

#endif
