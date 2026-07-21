#include <Engine/Core/RuntimeReportService.h>

#include <Engine/Core/RuntimeReportJson.h>

#include <cstdint>
#include <vector>

RuntimeReportSaveError RuntimeReportService::save(
	const std::string& path, const RuntimeReport& report) const noexcept
{
	if (path.empty()) return RuntimeReportSaveError::InvalidRequest;
	if (maximumBytes_ == 0) return RuntimeReportSaveError::TooLarge;
	try
	{
		RuntimeReportJsonResult serialized =
			SerializeRuntimeReportJson(report, maximumBytes_ - 1);
		if (!serialized)
		{
			switch (serialized.error)
			{
				case RuntimeReportJsonError::InvalidValue:
					return RuntimeReportSaveError::InvalidReport;
				case RuntimeReportJsonError::TooLarge:
					return RuntimeReportSaveError::TooLarge;
				case RuntimeReportJsonError::AllocationFailure:
					return RuntimeReportSaveError::AllocationFailure;
				case RuntimeReportJsonError::None: break;
			}
			return RuntimeReportSaveError::InvalidReport;
		}
		std::vector<std::uint8_t> bytes;
		bytes.reserve(serialized.json.size() + 1);
		bytes.assign(serialized.json.begin(), serialized.json.end());
		bytes.push_back(static_cast<std::uint8_t>('\n'));
		return persistence_.saveRaw(path, bytes)
			? RuntimeReportSaveError::None
			: RuntimeReportSaveError::StorageError;
	}
	catch (...)
	{
		return RuntimeReportSaveError::AllocationFailure;
	}
}
