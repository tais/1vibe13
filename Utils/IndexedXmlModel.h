#ifndef INDEXED_XML_MODEL_H
#define INDEXED_XML_MODEL_H

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace IndexedXmlModel
{
	enum class IndexSyntax
	{
		Decimal,
		CStyleUnsigned
	};

	struct ParsedIndex
	{
		bool valid = false;
		std::size_t value = 0;

		explicit constexpr operator bool() const noexcept { return valid; }
	};

	constexpr bool IsAsciiSpace(char value) noexcept
	{
		return value == ' ' || value == '\t' || value == '\n' ||
			value == '\r' || value == '\f' || value == '\v';
	}

	constexpr std::string_view TrimAscii(std::string_view value) noexcept
	{
		while (!value.empty() && IsAsciiSpace(value.front()))
			value.remove_prefix(1);
		while (!value.empty() && IsAsciiSpace(value.back()))
			value.remove_suffix(1);
		return value;
	}

	inline ParsedIndex ParseBoundedIndex(std::string_view text,
		std::size_t capacity, IndexSyntax syntax = IndexSyntax::Decimal) noexcept
	{
		text = TrimAscii(text);
		if (text.empty() || capacity == 0 || text.front() == '-') return {};
		if (text.front() == '+')
		{
			text.remove_prefix(1);
			if (text.empty()) return {};
		}

		int base = 10;
		if (syntax == IndexSyntax::CStyleUnsigned && text.size() > 1 &&
			text.front() == '0')
		{
			if (text.size() > 2 && (text[1] == 'x' || text[1] == 'X'))
			{
				base = 16;
				text.remove_prefix(2);
				if (text.empty()) return {};
			}
			else
			{
				base = 8;
			}
		}

		std::uintmax_t parsed = 0;
		const auto result = std::from_chars(
			text.data(), text.data() + text.size(), parsed, base);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
			parsed >= static_cast<std::uintmax_t>(capacity))
			return {};
		return { true, static_cast<std::size_t>(parsed) };
	}

	enum class StageResult
	{
		Accepted,
		IndexOutOfRange,
		TextTooLong
	};

	template <typename Text>
	class StagedIndexedText
	{
	public:
		struct Record
		{
			std::size_t index;
			Text text;
		};

		explicit StagedIndexedText(std::size_t tableCapacity) noexcept
			: tableCapacity_(tableCapacity)
		{
		}

		StageResult stage(std::size_t index, Text text,
			std::size_t textCapacity)
		{
			if (index >= tableCapacity_) return StageResult::IndexOutOfRange;
			if (textCapacity == 0 || text.size() >= textCapacity)
				return StageResult::TextTooLong;
			records_.push_back({ index, std::move(text) });
			return StageResult::Accepted;
		}

		template <typename Publisher>
		void publish(Publisher&& publisher) const
		{
			for (const Record& record : records_)
				publisher(record.index, record.text);
		}

		std::size_t size() const noexcept { return records_.size(); }
		bool empty() const noexcept { return records_.empty(); }

	private:
		std::size_t tableCapacity_;
		std::vector<Record> records_;
	};
}

#endif
