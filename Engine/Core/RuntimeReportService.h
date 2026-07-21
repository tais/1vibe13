#ifndef ENGINE_CORE_RUNTIME_REPORT_SERVICE_H
#define ENGINE_CORE_RUNTIME_REPORT_SERVICE_H

#include <cstddef>
#include <string>

#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeReport.h>

enum class RuntimeReportSaveError
{
	None,
	InvalidRequest,
	InvalidReport,
	TooLarge,
	AllocationFailure,
	StorageError
};

// Writes human/tool-readable reports as plain UTF-8 JSON. Save games and
// engine-owned records continue to use checksummed persistence envelopes.
class RuntimeReportService
{
public:
	explicit RuntimeReportService(
		PersistenceService& persistence,
		std::size_t maximumBytes = 4u * 1024u * 1024u)
		: persistence_(persistence), maximumBytes_(maximumBytes) {}

	RuntimeReportSaveError save(
		const std::string& path, const RuntimeReport& report) const noexcept;

	std::size_t maximumBytes() const { return maximumBytes_; }

private:
	PersistenceService& persistence_;
	std::size_t maximumBytes_;
};

#endif
