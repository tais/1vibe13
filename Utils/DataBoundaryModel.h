#ifndef UTILS_DATA_BOUNDARY_MODEL_H
#define UTILS_DATA_BOUNDARY_MODEL_H

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace UtilsDataBoundaryModel
{
	inline std::string_view TrimAsciiWhitespace(std::string_view value) noexcept
	{
		const auto isWhitespace = [](char character) {
			return character == ' ' || character == '\t' ||
				character == '\r' || character == '\n' ||
				character == '\f' || character == '\v';
		};
		while (!value.empty() && isWhitespace(value.front()))
			value.remove_prefix(1);
		while (!value.empty() && isWhitespace(value.back()))
			value.remove_suffix(1);
		return value;
	}

	inline bool ParseInt64(std::string_view text, std::int64_t& value) noexcept
	{
		text = TrimAsciiWhitespace(text);
		if (text.empty()) return false;

		bool positiveSign = false;
		if (text.front() == '+')
		{
			positiveSign = true;
			text.remove_prefix(1);
			if (text.empty()) return false;
		}

		std::int64_t staged = 0;
		const auto result = std::from_chars(
			text.data(), text.data() + text.size(), staged, 10);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
			return false;
		if (positiveSign && staged < 0) return false;
		value = staged;
		return true;
	}

	inline bool ParseUInt64(std::string_view text, std::uint64_t& value) noexcept
	{
		text = TrimAsciiWhitespace(text);
		if (text.empty() || text.front() == '-') return false;
		if (text.front() == '+')
		{
			text.remove_prefix(1);
			if (text.empty()) return false;
		}

		std::uint64_t staged = 0;
		const auto result = std::from_chars(
			text.data(), text.data() + text.size(), staged, 10);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
			return false;
		value = staged;
		return true;
	}

	inline bool ParseDouble(std::string_view text, double& value)
	{
		text = TrimAsciiWhitespace(text);
		if (text.empty()) return false;

		std::istringstream parser{std::string(text)};
		parser.imbue(std::locale::classic());
		double staged = 0.0;
		parser >> std::noskipws >> staged;
		if (!parser || parser.peek() != std::char_traits<char>::eof() ||
			!std::isfinite(staged)) return false;
		value = staged;
		return true;
	}

	inline bool ParseInt32List(
		std::string_view text, std::vector<std::int32_t>& destination)
	{
		std::vector<std::int32_t> staged;
		std::size_t start = 0;
		do
		{
			const std::size_t separator = text.find(',', start);
			const std::size_t length = separator == std::string_view::npos
				? text.size() - start : separator - start;
			std::int64_t parsed = 0;
			if (!ParseInt64(text.substr(start, length), parsed) ||
				parsed < std::numeric_limits<std::int32_t>::min() ||
				parsed > std::numeric_limits<std::int32_t>::max())
			{
				return false;
			}
			staged.push_back(static_cast<std::int32_t>(parsed));
			if (separator == std::string_view::npos) break;
			start = separator + 1;
		} while (true);

		destination = std::move(staged);
		return true;
	}

	inline bool ParseFloatList(
		std::string_view text, std::vector<float>& destination)
	{
		std::vector<float> staged;
		std::size_t start = 0;
		do
		{
			const std::size_t separator = text.find(',', start);
			const std::size_t length = separator == std::string_view::npos
				? text.size() - start : separator - start;
			double parsed = 0.0;
			if (!ParseDouble(text.substr(start, length), parsed) ||
				parsed < -std::numeric_limits<float>::max() ||
				parsed > std::numeric_limits<float>::max())
			{
				return false;
			}
			const float narrowed = static_cast<float>(parsed);
			if (!std::isfinite(narrowed) || (parsed != 0.0 && narrowed == 0.0f))
				return false;
			staged.push_back(narrowed);
			if (separator == std::string_view::npos) break;
			start = separator + 1;
		} while (true);

		destination = std::move(staged);
		return true;
	}

	inline bool CopyString(char* destination, std::size_t capacity,
		std::string_view source) noexcept
	{
		if (!destination || capacity == 0) return false;
		const std::size_t copied = source.size() < capacity
			? source.size() : capacity - 1;
		for (std::size_t index = 0; index < copied; ++index)
			destination[index] = source[index];
		destination[copied] = '\0';
		return copied == source.size();
	}

	template <std::size_t Capacity>
	bool CopyString(char (&destination)[Capacity],
		std::string_view source) noexcept
	{
		return CopyString(destination, Capacity, source);
	}

	template <typename State, typename Loader>
	bool PublishTransactionally(State& live, Loader&& loader)
	{
		State staged(live);
		if (!std::forward<Loader>(loader)(staged)) return false;
		live = std::move(staged);
		return true;
	}

	class UnknownXmlSubtree
	{
	public:
		bool enter() noexcept
		{
			if (depth_ == std::numeric_limits<std::size_t>::max()) return false;
			++depth_;
			return true;
		}

		bool leave() noexcept
		{
			if (depth_ == 0) return false;
			--depth_;
			return true;
		}

		bool active() const noexcept { return depth_ != 0; }
		std::size_t depth() const noexcept { return depth_; }

	private:
		std::size_t depth_ = 0;
	};

	inline bool EscapeXml(std::string_view source, std::string& destination)
	{
		std::string staged;
		staged.reserve(source.size());
		for (const unsigned char character : source)
		{
			switch (character)
			{
			case '&': staged += "&amp;"; break;
			case '<': staged += "&lt;"; break;
			case '>': staged += "&gt;"; break;
			case '"': staged += "&quot;"; break;
			case '\'': staged += "&apos;"; break;
			default:
				if (character < 0x20 && character != '\t' &&
					character != '\n' && character != '\r')
				{
					return false;
				}
				staged.push_back(static_cast<char>(character));
				break;
			}
		}
		destination = std::move(staged);
		return true;
	}

	inline bool SanitizeXmlComment(
		std::string_view source, std::string& destination)
	{
		std::string staged;
		staged.reserve(source.size() + 1);
		for (const unsigned char character : source)
		{
			if (character < 0x20 && character != '\t' &&
				character != '\n' && character != '\r')
			{
				return false;
			}
			if (character == '-' && !staged.empty() && staged.back() == '-')
				staged.push_back(' ');
			staged.push_back(static_cast<char>(character));
		}
		if (!staged.empty() && staged.back() == '-') staged.push_back(' ');
		destination = std::move(staged);
		return true;
	}

	constexpr bool IsExactTransfer(
		std::size_t requested, std::size_t transferred) noexcept
	{
		return requested == transferred;
	}
}

#endif
