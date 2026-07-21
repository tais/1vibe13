#include <Engine/Core/LocalizationDocument.h>

#include <Engine/Core/Identifier.h>

#include <string_view>
#include <unordered_set>
#include <utility>

namespace
{
std::string_view TrimAscii(std::string_view value)
{
	while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
		value.remove_prefix(1);
	while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
		value.remove_suffix(1);
	return value;
}

bool IsContinuationByte(unsigned char byte)
{
	return (byte & 0xc0u) == 0x80u;
}

bool IsValidUtf8(std::string_view text)
{
	for (std::size_t index = 0; index < text.size();)
	{
		const unsigned char first = static_cast<unsigned char>(text[index]);
		if (first <= 0x7fu)
		{
			if (first == 0) return false;
			++index;
			continue;
		}
		if (first >= 0xc2u && first <= 0xdfu)
		{
			if (index + 1 >= text.size() ||
				!IsContinuationByte(static_cast<unsigned char>(text[index + 1]))) return false;
			index += 2;
			continue;
		}
		if (first >= 0xe0u && first <= 0xefu)
		{
			if (index + 2 >= text.size()) return false;
			const unsigned char second = static_cast<unsigned char>(text[index + 1]);
			const unsigned char third = static_cast<unsigned char>(text[index + 2]);
			if (!IsContinuationByte(second) || !IsContinuationByte(third) ||
				(first == 0xe0u && second < 0xa0u) ||
				(first == 0xedu && second >= 0xa0u)) return false;
			index += 3;
			continue;
		}
		if (first >= 0xf0u && first <= 0xf4u)
		{
			if (index + 3 >= text.size()) return false;
			const unsigned char second = static_cast<unsigned char>(text[index + 1]);
			if (!IsContinuationByte(second) ||
				!IsContinuationByte(static_cast<unsigned char>(text[index + 2])) ||
				!IsContinuationByte(static_cast<unsigned char>(text[index + 3])) ||
				(first == 0xf0u && second < 0x90u) ||
				(first == 0xf4u && second >= 0x90u)) return false;
			index += 4;
			continue;
		}
		return false;
	}
	return true;
}

bool DecodeText(std::string_view encoded, std::string& decoded)
{
	decoded.clear();
	decoded.reserve(encoded.size());
	for (std::size_t index = 0; index < encoded.size(); ++index)
	{
		const char value = encoded[index];
		if (value != '\\')
		{
			if (static_cast<unsigned char>(value) < 32u) return false;
			decoded.push_back(value);
			continue;
		}
		if (++index >= encoded.size()) return false;
		switch (encoded[index])
		{
			case '\\': decoded.push_back('\\'); break;
			case 'n': decoded.push_back('\n'); break;
			case 'r': decoded.push_back('\r'); break;
			case 't': decoded.push_back('\t'); break;
			case '=': decoded.push_back('='); break;
			default: return false;
		}
	}
	return true;
}
}

LocalizationDocumentResult ParseLocalizationDocument(
	const std::vector<std::uint8_t>& bytes,
	std::vector<LocalizationDocumentEntry>& entries,
	std::size_t maximumDocumentBytes,
	std::size_t maximumEntries,
	std::size_t maximumTextBytes) noexcept
{
	if (bytes.size() > maximumDocumentBytes)
		return {LocalizationDocumentError::TooLarge, 0};
	try
	{
		const char* data = bytes.empty()
			? "" : reinterpret_cast<const char*>(bytes.data());
		std::string_view document(data, bytes.size());
		if (!IsValidUtf8(document))
			return {LocalizationDocumentError::InvalidUtf8, 0};
		if (document.size() >= 3 &&
			static_cast<unsigned char>(document[0]) == 0xefu &&
			static_cast<unsigned char>(document[1]) == 0xbbu &&
			static_cast<unsigned char>(document[2]) == 0xbfu)
			document.remove_prefix(3);

		std::vector<LocalizationDocumentEntry> parsed;
		parsed.reserve(maximumEntries < 64 ? maximumEntries : 64);
		std::unordered_set<std::string> keys;
		keys.reserve(maximumEntries < 64 ? maximumEntries : 64);
		bool sawHeader = false;
		std::size_t lineNumber = 0;
		std::size_t offset = 0;
		while (offset <= document.size())
		{
			++lineNumber;
			const std::size_t end = document.find('\n', offset);
			std::string_view line = document.substr(offset,
				end == std::string_view::npos ? std::string_view::npos : end - offset);
			if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
			line = TrimAscii(line);
			if (!line.empty() && line.front() != '#' && line.front() != ';')
			{
				if (!sawHeader)
				{
					if (line != "JA2-LOCALIZATION 1")
						return {LocalizationDocumentError::MissingOrUnsupportedHeader, lineNumber};
					sawHeader = true;
				}
				else
				{
					const std::size_t separator = line.find('=');
					if (separator == std::string_view::npos)
						return {LocalizationDocumentError::InvalidRecord, lineNumber};
					const std::string_view keyView = TrimAscii(line.substr(0, separator));
					const std::string_view encodedText = TrimAscii(line.substr(separator + 1));
					const std::string key(keyView);
					if (!IsValidEngineIdentifier(key))
						return {LocalizationDocumentError::InvalidKey, lineNumber};
					if (!keys.insert(key).second)
						return {LocalizationDocumentError::DuplicateKey, lineNumber};
					std::string text;
					if (!DecodeText(encodedText, text))
						return {LocalizationDocumentError::InvalidRecord, lineNumber};
					if (text.empty())
						return {LocalizationDocumentError::EmptyText, lineNumber};
					if (text.size() > maximumTextBytes)
						return {LocalizationDocumentError::TextTooLarge, lineNumber};
					if (parsed.size() >= maximumEntries)
						return {LocalizationDocumentError::TooManyEntries, lineNumber};
					parsed.push_back(LocalizationDocumentEntry{key, std::move(text)});
				}
			}
			if (end == std::string_view::npos) break;
			offset = end + 1;
		}
		if (!sawHeader)
			return {LocalizationDocumentError::MissingOrUnsupportedHeader, lineNumber};
		entries = std::move(parsed);
		return {};
	}
	catch (...)
	{
		return {LocalizationDocumentError::AllocationFailure, 0};
	}
}
