#ifndef ENGINE_CORE_RUNTIME_REPORT_JSON_H
#define ENGINE_CORE_RUNTIME_REPORT_JSON_H

#include <cstddef>
#include <string>

#include <Engine/Core/RuntimeReport.h>

enum class RuntimeReportJsonError
{
	None,
	InvalidValue,
	TooLarge,
	AllocationFailure
};

struct RuntimeReportJsonResult
{
	RuntimeReportJsonError error = RuntimeReportJsonError::None;
	std::string json;

	explicit operator bool() const { return error == RuntimeReportJsonError::None; }
};

// Emits one deterministic UTF-8 JSON document without package payloads. The
// size limit is enforced while writing and failures never publish a prefix.
RuntimeReportJsonResult SerializeRuntimeReportJson(
	const RuntimeReport& report,
	std::size_t maximumBytes = 4u * 1024u * 1024u) noexcept;

#endif
