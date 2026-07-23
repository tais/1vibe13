#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include <Engine/Adapters/Legacy/PlatformAssets.h>

#include <cstdio>
#include <exception>
#include <limits>
#include <new>
#include <vector>

namespace
{
class ScopedXmlParser
{
public:
	explicit ScopedXmlParser(XML_Parser parser) noexcept : parser_(parser) {}
	~ScopedXmlParser()
	{
		if (parser_) XML_ParserFree(parser_);
	}

	ScopedXmlParser(const ScopedXmlParser&) = delete;
	ScopedXmlParser& operator=(const ScopedXmlParser&) = delete;

	XML_Parser get() const noexcept { return parser_; }

private:
	XML_Parser parser_;
};

LegacyXmlResult ReadFailure(AssetReadResult readResult,
	std::size_t maximumBytes) noexcept
{
	LegacyXmlResult result;
	result.byteLimit = maximumBytes;
	switch (readResult)
	{
		case AssetReadResult::NotFound:
			result.status = LegacyXmlStatus::NotFound;
			break;
		case AssetReadResult::TooLarge:
			result.status = LegacyXmlStatus::TooLarge;
			break;
		case AssetReadResult::InvalidPath:
			result.status = LegacyXmlStatus::InvalidInput;
			break;
		case AssetReadResult::IoError:
			result.status = LegacyXmlStatus::ReadError;
			break;
		case AssetReadResult::Success:
			result.status = LegacyXmlStatus::Success;
			break;
	}
	return result;
}

const char* SafePath(const char* logicalPath) noexcept
{
	return logicalPath && logicalPath[0] ? logicalPath : "<invalid path>";
}

const char* SafeParserError(XML_Error error) noexcept
{
	const XML_LChar* description = XML_ErrorString(error);
	return description ? description : "unknown parser error";
}

LegacyXmlResult ValidateXmlBytes(
	const void* bytes, std::size_t byteCount) noexcept
{
	LegacyXmlResult result;
	result.byteCount = byteCount;
	result.byteLimit = static_cast<std::size_t>(std::numeric_limits<int>::max());
	if ((!bytes && byteCount != 0) || byteCount > result.byteLimit)
	{
		result.status = byteCount > result.byteLimit
			? LegacyXmlStatus::TooLarge
			: LegacyXmlStatus::InvalidInput;
		return result;
	}
	result.status = LegacyXmlStatus::Success;
	return result;
}

void CaptureParserPosition(
	XML_Parser parser, LegacyXmlResult& result) noexcept
{
	if (!parser) return;
	result.line = XML_GetCurrentLineNumber(parser);
	result.column = XML_GetCurrentColumnNumber(parser);
}

LegacyXmlResult ParseWithOwnedParser(XML_Parser rawParser,
	const void* bytes, std::size_t byteCount,
	const LegacyXmlCallbacks& callbacks,
	LegacyXmlResult result) noexcept
{
	ScopedXmlParser parser(rawParser);
	if (!parser.get())
	{
		result.status = LegacyXmlStatus::ParserUnavailable;
		return result;
	}

	try
	{
		XML_SetUserData(parser.get(), callbacks.userData);
		XML_SetElementHandler(parser.get(),
			callbacks.startElement, callbacks.endElement);
		XML_SetCharacterDataHandler(parser.get(), callbacks.characterData);
		if (callbacks.parserReady)
			callbacks.parserReady(parser.get(), callbacks.userData);
		if (callbacks.beforeParse) callbacks.beforeParse(callbacks.userData);

		static constexpr char emptyDocument = '\0';
		const char* const document = bytes
			? static_cast<const char*>(bytes)
			: &emptyDocument;
		if (XML_Parse(parser.get(), document, static_cast<int>(byteCount), XML_TRUE) ==
			XML_STATUS_OK)
		{
			result.status = LegacyXmlStatus::Success;
			return result;
		}

		result.status = LegacyXmlStatus::Malformed;
		result.parserError = XML_GetErrorCode(parser.get());
		CaptureParserPosition(parser.get(), result);
		return result;
	}
	catch (const std::bad_alloc&)
	{
		result.status = LegacyXmlStatus::OutOfMemory;
		CaptureParserPosition(parser.get(), result);
		return result;
	}
	catch (const std::exception& error)
	{
		result.status = LegacyXmlStatus::CallbackError;
		CaptureParserPosition(parser.get(), result);
		std::snprintf(result.callbackDiagnostic.data(),
			result.callbackDiagnostic.size(), "%s", error.what());
		result.callbackDiagnostic.back() = '\0';
		return result;
	}
	catch (...)
	{
		result.status = LegacyXmlStatus::CallbackError;
		CaptureParserPosition(parser.get(), result);
		return result;
	}
}

LegacyXmlResult ReadXmlAsset(const AssetSource& assets,
	const std::string& logicalPath, std::size_t maximumBytes,
	AssetData& asset) noexcept
{
	const std::size_t parserLimit =
		static_cast<std::size_t>(std::numeric_limits<int>::max());
	const std::size_t effectiveLimit =
		maximumBytes < parserLimit ? maximumBytes : parserLimit;
	try
	{
		const AssetReadResult readResult =
			assets.read(logicalPath, asset, effectiveLimit);
		if (readResult != AssetReadResult::Success)
			return ReadFailure(readResult, effectiveLimit);

		LegacyXmlResult result;
		result.status = LegacyXmlStatus::Success;
		result.byteCount = asset.bytes.size();
		result.byteLimit = effectiveLimit;
		return result;
	}
	catch (const std::bad_alloc&)
	{
		LegacyXmlResult result;
		result.status = LegacyXmlStatus::OutOfMemory;
		result.byteLimit = effectiveLimit;
		return result;
	}
	catch (...)
	{
		LegacyXmlResult result;
		result.status = LegacyXmlStatus::ReadError;
		result.byteLimit = effectiveLimit;
		return result;
	}
}
}

LegacyXmlResult ParseLegacyXmlBytes(const void* bytes, std::size_t byteCount,
	const LegacyXmlCallbacks& callbacks) noexcept
{
	LegacyXmlResult result = ValidateXmlBytes(bytes, byteCount);
	if (!result) return result;
	return ParseWithOwnedParser(
		XML_ParserCreate(nullptr), bytes, byteCount, callbacks, result);
}

LegacyXmlResult ParseLegacyXmlExternalEntityBytes(XML_Parser parentParser,
	const XML_Char* context, const void* bytes, std::size_t byteCount,
	const LegacyXmlCallbacks& callbacks) noexcept
{
	LegacyXmlResult result = ValidateXmlBytes(bytes, byteCount);
	if (!result) return result;
	if (!parentParser)
	{
		result.status = LegacyXmlStatus::InvalidInput;
		return result;
	}
	return ParseWithOwnedParser(
		XML_ExternalEntityParserCreate(parentParser, context, nullptr),
		bytes, byteCount, callbacks, result);
}

LegacyXmlResult ParseLegacyXmlAsset(const AssetSource& assets,
	const std::string& logicalPath, const LegacyXmlCallbacks& callbacks,
	std::size_t maximumBytes) noexcept
{
	AssetData asset;
	LegacyXmlResult result =
		ReadXmlAsset(assets, logicalPath, maximumBytes, asset);
	if (!result) return result;
	result = ParseLegacyXmlBytes(
		asset.bytes.data(), asset.bytes.size(), callbacks);
	result.byteLimit = maximumBytes <
		static_cast<std::size_t>(std::numeric_limits<int>::max())
			? maximumBytes
			: static_cast<std::size_t>(std::numeric_limits<int>::max());
	return result;
}

LegacyXmlResult ParseLegacyXmlExternalEntityAsset(const AssetSource& assets,
	const std::string& logicalPath, XML_Parser parentParser,
	const XML_Char* context, const LegacyXmlCallbacks& callbacks,
	std::size_t maximumBytes) noexcept
{
	if (!parentParser)
	{
		LegacyXmlResult result;
		result.status = LegacyXmlStatus::InvalidInput;
		result.byteLimit = maximumBytes;
		return result;
	}

	AssetData asset;
	LegacyXmlResult result =
		ReadXmlAsset(assets, logicalPath, maximumBytes, asset);
	if (!result) return result;
	result = ParseLegacyXmlExternalEntityBytes(parentParser, context,
		asset.bytes.data(), asset.bytes.size(), callbacks);
	result.byteLimit = maximumBytes <
		static_cast<std::size_t>(std::numeric_limits<int>::max())
			? maximumBytes
			: static_cast<std::size_t>(std::numeric_limits<int>::max());
	return result;
}

LegacyXmlResult ParseLegacyXmlFile(const char* logicalPath,
	const LegacyXmlCallbacks& callbacks, std::size_t maximumBytes) noexcept
{
	if (!logicalPath || !logicalPath[0])
	{
		LegacyXmlResult result;
		result.status = LegacyXmlStatus::InvalidInput;
		result.byteLimit = maximumBytes;
		return result;
	}

	try
	{
		return ParseLegacyXmlAsset(
			GetPlatformAssetSource(), logicalPath, callbacks, maximumBytes);
	}
	catch (const std::bad_alloc&)
	{
		LegacyXmlResult result;
		result.status = LegacyXmlStatus::OutOfMemory;
		result.byteLimit = maximumBytes;
		return result;
	}
	catch (...)
	{
		LegacyXmlResult result;
		result.status = LegacyXmlStatus::ReadError;
		result.byteLimit = maximumBytes;
		return result;
	}
}

std::array<char, LegacyXmlFailureMessageBytes> FormatLegacyXmlFailure(
	const char* logicalPath, const LegacyXmlResult& result) noexcept
{
	std::array<char, LegacyXmlFailureMessageBytes> message{};
	const char* const path = SafePath(logicalPath);
	switch (result.status)
	{
		case LegacyXmlStatus::Success:
			std::snprintf(message.data(), message.size(),
				"XML document loaded: %s", path);
			break;
		case LegacyXmlStatus::InvalidInput:
			std::snprintf(message.data(), message.size(),
				"Invalid XML input path or buffer: %s", path);
			break;
		case LegacyXmlStatus::NotFound:
			std::snprintf(message.data(), message.size(),
				"XML file not found: %s", path);
			break;
		case LegacyXmlStatus::TooLarge:
			std::snprintf(message.data(), message.size(),
				"XML file exceeds the %zu-byte safety limit: %s",
				result.byteLimit, path);
			break;
		case LegacyXmlStatus::ReadError:
			std::snprintf(message.data(), message.size(),
				"XML read error in %s", path);
			break;
		case LegacyXmlStatus::OutOfMemory:
			std::snprintf(message.data(), message.size(),
				"Not enough memory to load XML file: %s", path);
			break;
		case LegacyXmlStatus::ParserUnavailable:
			std::snprintf(message.data(), message.size(),
				"Unable to create XML parser for %s", path);
			break;
		case LegacyXmlStatus::CallbackError:
			if (result.callbackDiagnostic[0] && result.line)
				std::snprintf(message.data(), message.size(),
					"XML callback failed while processing %s at line %llu: %s",
					path, static_cast<unsigned long long>(result.line),
					result.callbackDiagnostic.data());
			else if (result.callbackDiagnostic[0])
				std::snprintf(message.data(), message.size(),
					"XML callback failed while processing %s: %s",
					path, result.callbackDiagnostic.data());
			else
				std::snprintf(message.data(), message.size(),
					"XML callback failed while processing %s", path);
			break;
		case LegacyXmlStatus::Malformed:
			std::snprintf(message.data(), message.size(),
				"XML Parser Error in %s: %s at line %llu, column %llu",
				path, SafeParserError(result.parserError),
				static_cast<unsigned long long>(result.line),
				static_cast<unsigned long long>(result.column));
			break;
	}
	message.back() = '\0';
	return message;
}
