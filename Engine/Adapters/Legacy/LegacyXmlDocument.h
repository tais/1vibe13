#ifndef ENGINE_ADAPTERS_LEGACY_LEGACY_XML_DOCUMENT_H
#define ENGINE_ADAPTERS_LEGACY_LEGACY_XML_DOCUMENT_H

#include <Engine/Core/AssetSource.h>

#include "expat.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

constexpr std::size_t DefaultLegacyXmlReadLimit = DefaultAssetReadLimit;
constexpr std::size_t LegacyXmlFailureMessageBytes = 512;

enum class LegacyXmlStatus
{
	Success,
	InvalidInput,
	NotFound,
	TooLarge,
	ReadError,
	OutOfMemory,
	ParserUnavailable,
	CallbackError,
	Malformed
};

using LegacyXmlBeforeParseHandler = void (*)(void* userData);
using LegacyXmlParserReadyHandler =
	void (*)(XML_Parser parser, void* userData);

struct LegacyXmlCallbacks
{
	void* userData = nullptr;
	XML_StartElementHandler startElement = nullptr;
	XML_EndElementHandler endElement = nullptr;
	XML_CharacterDataHandler characterData = nullptr;
	// Runs once after input validation, asset reading, and parser creation, but
	// before Expat emits any document callback. It is not invoked when an asset
	// cannot be read, allowing legacy table replacement to remain transactional
	// with respect to missing and I/O-failed files.
	LegacyXmlBeforeParseHandler beforeParse = nullptr;
	// Runs after the default callbacks are installed and before beforeParse.
	// Object-oriented legacy readers may rebind the parser here, but ownership
	// remains with this adapter for the complete parse lifetime.
	LegacyXmlParserReadyHandler parserReady = nullptr;
};

struct LegacyXmlResult
{
	LegacyXmlStatus status = LegacyXmlStatus::InvalidInput;
	XML_Error parserError = XML_ERROR_NONE;
	std::uint64_t line = 0;
	std::uint64_t column = 0;
	std::size_t byteCount = 0;
	std::size_t byteLimit = 0;

	explicit operator bool() const noexcept
	{
		return status == LegacyXmlStatus::Success;
	}
};

// Parses a complete in-memory document with a fresh parser. The caller retains
// ownership of both the bytes and callback state for the duration of the call.
LegacyXmlResult ParseLegacyXmlBytes(const void* bytes, std::size_t byteCount,
	const LegacyXmlCallbacks& callbacks) noexcept;

// AssetSource overload keeps legacy table parsing usable by package, memory,
// and composite sources instead of hard-wiring it to FileMan.
LegacyXmlResult ParseLegacyXmlAsset(const AssetSource& assets,
	const std::string& logicalPath, const LegacyXmlCallbacks& callbacks,
	std::size_t maximumBytes = DefaultLegacyXmlReadLimit) noexcept;

// Compatibility entry point for existing VFS-backed game loaders.
LegacyXmlResult ParseLegacyXmlFile(const char* logicalPath,
	const LegacyXmlCallbacks& callbacks,
	std::size_t maximumBytes = DefaultLegacyXmlReadLimit) noexcept;

// Allocation-free, always-terminated diagnostic suitable for LiveMessage.
std::array<char, LegacyXmlFailureMessageBytes> FormatLegacyXmlFailure(
	const char* logicalPath, const LegacyXmlResult& result) noexcept;

#endif
