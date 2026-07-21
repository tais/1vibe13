#ifndef ENGINE_CORE_LOCALIZATION_DOCUMENT_H
#define ENGINE_CORE_LOCALIZATION_DOCUMENT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct LocalizationDocumentEntry
{
	std::string key;
	std::string text;
};

enum class LocalizationDocumentError
{
	None,
	TooLarge,
	InvalidUtf8,
	MissingOrUnsupportedHeader,
	InvalidRecord,
	InvalidKey,
	DuplicateKey,
	EmptyText,
	TextTooLarge,
	TooManyEntries,
	AllocationFailure
};

struct LocalizationDocumentResult
{
	LocalizationDocumentError error = LocalizationDocumentError::None;
	std::size_t line = 0;

	explicit operator bool() const { return error == LocalizationDocumentError::None; }
};

// Portable line-oriented localization format:
//
//   JA2-LOCALIZATION 1
//   ui.ready = Ready
//
// Blank lines and lines beginning with # or ; are ignored. Values support
// \\, \n, \r, \t, and \= escapes. Parsing is bounded and transactional: a
// failure leaves the caller's existing entries unchanged.
LocalizationDocumentResult ParseLocalizationDocument(
	const std::vector<std::uint8_t>& bytes,
	std::vector<LocalizationDocumentEntry>& entries,
	std::size_t maximumDocumentBytes = 4u * 1024u * 1024u,
	std::size_t maximumEntries = 65536,
	std::size_t maximumTextBytes = 16u * 1024u) noexcept;

#endif
