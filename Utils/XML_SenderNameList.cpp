#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "Debug Control.h"
#include "IndexedXmlModel.h"
#include "Text.h"
#include "XML_SenderNameList.h"
#include "sgp.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
	constexpr std::size_t SenderNameCapacity = 500;

	enum class SenderNameParseStage
	{
		None,
		List,
		Element,
		Property
	};

	struct SenderNameParseData
	{
		SenderNameParseData() : staged(SenderNameCapacity) {}

		SenderNameParseStage currentElement = SenderNameParseStage::None;
		std::string characterData;
		std::size_t currentIndex = 0;
		std::wstring currentName;
		bool sawIndex = false;
		bool sawName = false;
		std::size_t currentDepth = 0;
		std::size_t maxReadDepth = 0;
		IndexedXmlModel::StagedIndexedText<std::wstring> staged;
	};

	template <std::size_t Capacity>
	std::wstring ConvertUtf8Name(const std::string& source)
	{
		const int required = MultiByteToWideChar(
			CP_UTF8, 0, source.c_str(), -1, nullptr, 0);
		if (required <= 0 || static_cast<std::size_t>(required) > Capacity)
			throw std::runtime_error(
				"Sender-name XML Name exceeds its destination capacity");

		std::array<CHAR16, Capacity> converted{};
		if (MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1,
				converted.data(), static_cast<int>(converted.size())) != required)
			throw std::runtime_error("Sender-name XML Name is not valid UTF-8");
		return std::wstring(converted.data(),
			static_cast<std::size_t>(required - 1));
	}

	template <std::size_t Capacity>
	void PublishName(CHAR16 (&destination)[Capacity], std::wstring_view name)
	{
		std::copy(name.begin(), name.end(), destination);
		destination[name.size()] = L'\0';
	}

	void XMLCALL SenderNameStartElement(
		void* userData, const XML_Char* name, const XML_Char** attributes)
	{
		(void)attributes;
		auto* data = static_cast<SenderNameParseData*>(userData);

		if (data->currentDepth <= data->maxReadDepth)
		{
			if (std::strcmp(name, "SENDER_LIST") == 0 &&
				data->currentElement == SenderNameParseStage::None)
			{
				data->currentElement = SenderNameParseStage::List;
				++data->maxReadDepth;
			}
			else if (std::strcmp(name, "NAME") == 0 &&
				data->currentElement == SenderNameParseStage::List)
			{
				data->currentElement = SenderNameParseStage::Element;
				data->currentIndex = 0;
				data->currentName.clear();
				data->sawIndex = false;
				data->sawName = false;
				++data->maxReadDepth;
			}
			else if (data->currentElement == SenderNameParseStage::Element &&
				(std::strcmp(name, "uiIndex") == 0 ||
				 std::strcmp(name, "Name") == 0))
			{
				data->currentElement = SenderNameParseStage::Property;
				++data->maxReadDepth;
			}
			data->characterData.clear();
		}
		++data->currentDepth;
	}

	void XMLCALL SenderNameCharacterData(
		void* userData, const XML_Char* characters, int length)
	{
		auto* data = static_cast<SenderNameParseData*>(userData);
		if (data->currentDepth <= data->maxReadDepth && length > 0)
			data->characterData.append(
				characters, static_cast<std::size_t>(length));
	}

	void XMLCALL SenderNameEndElement(void* userData, const XML_Char* name)
	{
		auto* data = static_cast<SenderNameParseData*>(userData);

		if (data->currentDepth <= data->maxReadDepth)
		{
			if (std::strcmp(name, "SENDER_LIST") == 0)
			{
				data->currentElement = SenderNameParseStage::None;
			}
			else if (std::strcmp(name, "NAME") == 0)
			{
				if (!data->sawIndex || !data->sawName)
					throw std::runtime_error(
						"Sender-name XML NAME requires uiIndex and Name");
				if (data->staged.stage(data->currentIndex,
						std::move(data->currentName), MAX_SENDER_NAMES_CHARS) !=
					IndexedXmlModel::StageResult::Accepted)
					throw std::runtime_error(
						"Sender-name XML NAME is outside its table bounds");
				data->currentElement = SenderNameParseStage::List;
			}
			else if (std::strcmp(name, "uiIndex") == 0)
			{
				const auto index = IndexedXmlModel::ParseBoundedIndex(
					data->characterData, SenderNameCapacity,
					IndexedXmlModel::IndexSyntax::Decimal);
				if (!index)
					throw std::runtime_error(
						"Sender-name XML uiIndex is invalid or out of range");
				data->currentIndex = index.value;
				data->sawIndex = true;
				data->currentElement = SenderNameParseStage::Element;
			}
			else if (std::strcmp(name, "Name") == 0)
			{
				data->currentName =
					ConvertUtf8Name<MAX_SENDER_NAMES_CHARS>(data->characterData);
				data->sawName = true;
				data->currentElement = SenderNameParseStage::Element;
			}
			--data->maxReadDepth;
		}
		--data->currentDepth;
	}
}

BOOLEAN ReadInSenderNameList(STR fileName, BOOLEAN localizedVersion)
{
	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading SenderNameList.xml");
	static_assert(std::size(pSenderNameList) == SenderNameCapacity);

	SenderNameParseData data;
	const LegacyXmlCallbacks callbacks{
		&data, SenderNameStartElement, SenderNameEndElement,
		SenderNameCharacterData};
	const LegacyXmlResult result = ParseLegacyXmlFile(fileName, callbacks);
	if (!result)
	{
		if (result.status == LegacyXmlStatus::NotFound)
			return localizedVersion;
		if (result.status != LegacyXmlStatus::ReadError)
		{
			const auto message = FormatLegacyXmlFailure(fileName, result);
			LiveMessage(message.data());
		}
		return FALSE;
	}

	data.staged.publish([](std::size_t index, const std::wstring& name)
	{
		PublishName(pSenderNameList[index], name);
	});
	return TRUE;
}
