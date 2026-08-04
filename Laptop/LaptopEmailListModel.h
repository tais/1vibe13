#ifndef LAPTOP_EMAIL_LIST_MODEL_H
#define LAPTOP_EMAIL_LIST_MODEL_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace LaptopEmailListModel
{
	inline constexpr std::size_t NoIndex =
		std::numeric_limits<std::size_t>::max();

	constexpr std::size_t PageCount(
		std::size_t messageCount, std::size_t messagesPerPage) noexcept
	{
		if (messagesPerPage == 0) return 0;
		return messageCount / messagesPerPage +
			(messageCount % messagesPerPage != 0 ? 1 : 0);
	}

	constexpr std::size_t NormalizeInboxPage(std::size_t savedPage,
		std::size_t messageCount, std::size_t messagesPerPage) noexcept
	{
		const auto pageCount = PageCount(messageCount, messagesPerPage);
		return pageCount == 0 ? 0
			: (savedPage < pageCount ? savedPage : pageCount - 1);
	}

	template<typename Index>
	constexpr bool IsIndexInRange(
		Index index, std::size_t size) noexcept
	{
		if constexpr (std::is_signed<Index>::value)
		{
			if (index < 0) return false;
		}
		return static_cast<std::size_t>(index) < size;
	}

	constexpr bool CanStoreUnsigned16(std::int64_t value) noexcept
	{
		return value >= 0 && value <=
			static_cast<std::int64_t>(std::numeric_limits<std::uint16_t>::max());
	}

	constexpr bool CanAppendMessage(std::size_t messageCount,
		std::int32_t greatestId, std::size_t messageLimit) noexcept
	{
		if (messageLimit == 0 || messageCount >= messageLimit ||
			greatestId < -1 ||
			greatestId >= std::numeric_limits<std::int32_t>::max()) return false;
		return greatestId < 0 ||
			static_cast<std::size_t>(greatestId) + 1 < messageLimit;
	}

	constexpr std::int32_t NextMessageId(std::int32_t greatestId) noexcept
	{
		return greatestId < 0 ? 0 : greatestId + 1;
	}

	constexpr bool IsMoreRecent(std::uint32_t candidateDate,
		std::int32_t candidateId, bool hasCurrent, std::uint32_t currentDate,
		std::int32_t currentId) noexcept
	{
		return !hasCurrent || candidateDate > currentDate ||
			(candidateDate == currentDate && candidateId > currentId);
	}

	// Legacy Wildfire availability mail uses paired subject/body entries for
	// lengths 170..177 and the final generic pair for every later profile.
	constexpr std::size_t WildfireSubjectLine(
		std::int32_t messageLength) noexcept
	{
		if (messageLength >= 170 && messageLength <= 177)
			return static_cast<std::size_t>(messageLength - 170) * 2;
		if (messageLength >= 178) return 16;
		return NoIndex;
	}

	// pEmailPageInfo reserves its final slot as a null sentinel. The historical
	// page count includes that sentinel, hence the subtraction here.
	constexpr std::size_t BodyPageCount(std::size_t reportedEntryCount,
		std::size_t tableCapacity) noexcept
	{
		if (reportedEntryCount == 0 || tableCapacity < 2) return 0;
		const auto reportedPages = reportedEntryCount - 1;
		const auto maximumPages = tableCapacity - 1;
		return reportedPages < maximumPages ? reportedPages : maximumPages;
	}

	constexpr std::size_t NormalizeBodyPage(std::size_t requestedPage,
		std::size_t reportedEntryCount, std::size_t tableCapacity) noexcept
	{
		const auto pageCount = BodyPageCount(reportedEntryCount, tableCapacity);
		return pageCount == 0 ? 0
			: (requestedPage < pageCount ? requestedPage : pageCount - 1);
	}

	constexpr bool HasNextBodyPage(std::size_t currentPage,
		std::size_t reportedEntryCount, std::size_t tableCapacity) noexcept
	{
		const auto pageCount = BodyPageCount(reportedEntryCount, tableCapacity);
		return currentPage < pageCount && currentPage + 1 < pageCount;
	}

	template<typename Character, std::size_t Capacity>
	bool CopyText(Character (&destination)[Capacity],
		const Character* source) noexcept
	{
		static_assert(Capacity > 0, "text buffers need a terminator slot");
		if (!source)
		{
			destination[0] = Character{};
			return false;
		}

		std::size_t index = 0;
		while (index + 1 < Capacity && source[index] != Character{})
		{
			destination[index] = source[index];
			++index;
		}
		destination[index] = Character{};
		return source[index] == Character{};
	}

	template<typename Character, std::size_t Capacity>
	bool AppendText(Character (&destination)[Capacity],
		const Character* source) noexcept
	{
		static_assert(Capacity > 0, "text buffers need a terminator slot");
		std::size_t length = 0;
		while (length < Capacity && destination[length] != Character{})
			++length;
		if (length == Capacity || !source) return false;

		std::size_t sourceIndex = 0;
		while (length + 1 < Capacity &&
			source[sourceIndex] != Character{})
		{
			destination[length++] = source[sourceIndex++];
		}
		destination[length] = Character{};
		return source[sourceIndex] == Character{};
	}

	template<typename Character>
	bool BoundedLength(const Character* text, std::size_t maximumLength,
		std::size_t& length) noexcept
	{
		length = 0;
		if (!text) return false;
		while (length <= maximumLength && text[length] != Character{})
			++length;
		return length <= maximumLength;
	}

	constexpr bool IsSerializedSubjectSizeValid(std::size_t byteCount,
		std::size_t characterSize, std::size_t maximumCharacters) noexcept
	{
		return characterSize != 0 && byteCount >= characterSize &&
			byteCount % characterSize == 0 &&
			byteCount / characterSize <= maximumCharacters + 1;
	}
}

#endif
