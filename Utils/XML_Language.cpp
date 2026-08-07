#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "Debug Control.h"
#include "IndexedXmlModel.h"
#include "Text.h"
#include "XML.h"
#include "XML_Language.h"
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
	constexpr std::size_t LanguageEntryCapacity = 1000;
	constexpr UINT32 TacticalMessageFileType = 0;

	enum class LanguageParseStage
	{
		None,
		List,
		Element,
		Property
	};

	struct LanguageLocationParseData
	{
		LanguageLocationParseData(
			BOOLEAN localizedVersion, LANGUAGE_LOCATION* languageDestination,
			UINT32 parsedFileType)
			: localized(localizedVersion != FALSE),
			  destination(languageDestination),
			  fileType(parsedFileType),
			  staged(LanguageEntryCapacity)
		{
		}

		LanguageParseStage currentElement = LanguageParseStage::None;
		std::string characterData;
		std::size_t currentIndex = 0;
		std::wstring currentMessage;
		bool sawIndex = false;
		bool sawMessage = false;
		std::size_t currentDepth = 0;
		std::size_t maxReadDepth = 0;
		bool localized;
		LANGUAGE_LOCATION* destination;
		UINT32 fileType;
		IndexedXmlModel::StagedIndexedText<std::wstring> staged;
	};

	template <std::size_t Capacity>
	std::wstring ConvertUtf8Text(const std::string& source)
	{
		const int required = MultiByteToWideChar(
			CP_UTF8, 0, source.c_str(), -1, nullptr, 0);
		if (required <= 0 || static_cast<std::size_t>(required) > Capacity)
			throw std::runtime_error("Language XML Message exceeds its destination capacity");

		std::array<CHAR16, Capacity> converted{};
		if (MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1,
				converted.data(), static_cast<int>(converted.size())) != required)
			throw std::runtime_error("Language XML Message is not valid UTF-8");
		return std::wstring(converted.data(),
			static_cast<std::size_t>(required - 1));
	}

	template <std::size_t Capacity>
	void PublishText(CHAR16 (&destination)[Capacity], std::wstring_view text)
	{
		std::copy(text.begin(), text.end(), destination);
		destination[text.size()] = L'\0';
	}

	void XMLCALL LanguageLocationStartElement(
		void* userData, const XML_Char* name, const XML_Char** attributes)
	{
		(void)attributes;
		auto* data = static_cast<LanguageLocationParseData*>(userData);

		if (data->currentDepth <= data->maxReadDepth)
		{
			if (std::strcmp(name, "MESSAGES") == 0 &&
				data->currentElement == LanguageParseStage::None)
			{
				data->currentElement = LanguageParseStage::List;
				++data->maxReadDepth;
			}
			else if (std::strcmp(name, "TEXT") == 0 &&
				data->currentElement == LanguageParseStage::List)
			{
				data->currentElement = LanguageParseStage::Element;
				data->currentIndex = 0;
				data->currentMessage.clear();
				data->sawIndex = false;
				data->sawMessage = false;
				++data->maxReadDepth;
			}
			else if (data->currentElement == LanguageParseStage::Element &&
				(std::strcmp(name, "uiIndex") == 0 ||
				 std::strcmp(name, "Message") == 0))
			{
				data->currentElement = LanguageParseStage::Property;
				++data->maxReadDepth;
			}
			data->characterData.clear();
		}
		++data->currentDepth;
	}

	void XMLCALL LanguageLocationCharacterData(
		void* userData, const XML_Char* characters, int length)
	{
		auto* data = static_cast<LanguageLocationParseData*>(userData);
		if (data->currentDepth <= data->maxReadDepth && length > 0)
			data->characterData.append(
				characters, static_cast<std::size_t>(length));
	}

	void XMLCALL LanguageLocationEndElement(
		void* userData, const XML_Char* name)
	{
		auto* data = static_cast<LanguageLocationParseData*>(userData);

		if (data->currentDepth <= data->maxReadDepth)
		{
			if (std::strcmp(name, "MESSAGES") == 0)
			{
				data->currentElement = LanguageParseStage::None;
			}
			else if (std::strcmp(name, "TEXT") == 0)
			{
				if (!data->sawIndex || !data->sawMessage)
					throw std::runtime_error(
						"Language XML TEXT requires uiIndex and Message");
				if (data->staged.stage(data->currentIndex,
						std::move(data->currentMessage), MAX_MESSAGE_NAMES_CHARS) !=
					IndexedXmlModel::StageResult::Accepted)
					throw std::runtime_error(
						"Language XML TEXT is outside its table bounds");
				data->currentElement = LanguageParseStage::List;
			}
			else if (std::strcmp(name, "uiIndex") == 0)
			{
				const auto index = IndexedXmlModel::ParseBoundedIndex(
					data->characterData, LanguageEntryCapacity,
					IndexedXmlModel::IndexSyntax::CStyleUnsigned);
				if (!index)
					throw std::runtime_error(
						"Language XML uiIndex is invalid or out of range");
				data->currentIndex = index.value;
				data->sawIndex = true;
				data->currentElement = LanguageParseStage::Element;
			}
			else if (std::strcmp(name, "Message") == 0)
			{
				data->currentMessage =
					ConvertUtf8Text<MAX_MESSAGE_NAMES_CHARS>(data->characterData);
				data->sawMessage = true;
				data->currentElement = LanguageParseStage::Element;
			}
			--data->maxReadDepth;
		}
		--data->currentDepth;
	}
}

LANGUAGE_LOCATION zlanguageText[LanguageEntryCapacity];

BOOLEAN ReadInLanguageLocation(STR fileName, BOOLEAN localizedVersion,
	LANGUAGE_LOCATION* languageDestination, UINT32 fileType)
{
	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading NewTacticalMessages.xml");

	if (!localizedVersion && !languageDestination) return FALSE;
	static_assert(std::size(zlanguageText) == LanguageEntryCapacity);
	static_assert(std::size(XMLTacticalMessages) == LanguageEntryCapacity);

	LanguageLocationParseData data(
		localizedVersion, languageDestination, fileType);
	const LegacyXmlCallbacks callbacks{
		&data, LanguageLocationStartElement, LanguageLocationEndElement,
		LanguageLocationCharacterData};
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

	data.staged.publish([&](std::size_t index, const std::wstring& message)
	{
		if (!data.localized)
			data.destination[index].uiIndex = static_cast<UINT32>(index);
		if (data.fileType == TacticalMessageFileType)
			PublishText(XMLTacticalMessages[index], message);
	});
	return TRUE;
}
