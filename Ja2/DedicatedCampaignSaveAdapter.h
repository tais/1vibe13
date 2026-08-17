#ifndef JA2_DEDICATED_CAMPAIGN_SAVE_ADAPTER_H
#define JA2_DEDICATED_CAMPAIGN_SAVE_ADAPTER_H

#include "DedicatedCampaignFilesystem.h"

#include <filesystem>

class DedicatedCampaignSaveAdapter final
	: public DedicatedCampaignCheckpointWriter
{
public:
	explicit DedicatedCampaignSaveAdapter(
		std::filesystem::path profileDirectory) noexcept;

	DedicatedCampaignSaveAdapter(const DedicatedCampaignSaveAdapter&) = delete;
	DedicatedCampaignSaveAdapter& operator=(
		const DedicatedCampaignSaveAdapter&) = delete;

	bool writeCheckpoint(DedicatedCampaignSlot slot,
		const std::filesystem::path& reservedStagingPath) noexcept override;

	const std::filesystem::path& profileDirectory() const noexcept;
	std::filesystem::path logicalScratchPath(
		DedicatedCampaignSlot slot) const noexcept;

private:
	std::filesystem::path profileDirectory_;
};

#endif
