#ifndef ENGINE_CORE_RANDOM_SOURCE_H
#define ENGINE_CORE_RANDOM_SOURCE_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

class RandomSource
{
public:
	virtual ~RandomSource() = default;
	virtual std::uint32_t next(std::uint32_t upperBound) = 0;
};

class SequenceRandomSource final : public RandomSource
{
public:
	explicit SequenceRandomSource(std::vector<std::uint32_t> values)
		: values_(std::move(values))
	{
	}

	std::uint32_t next(std::uint32_t upperBound) override
	{
		if (upperBound == 0 || values_.empty()) return 0;
		const std::uint32_t value = values_[position_++ % values_.size()];
		return value % upperBound;
	}

	std::size_t position() const { return position_; }
	void rewind() { position_ = 0; }

private:
	std::vector<std::uint32_t> values_;
	std::size_t position_ = 0;
};

#endif
