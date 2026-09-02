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

	// The VFS must discover these two fixed entries during its initial scan.
	// Preparation therefore creates or truncates the existing single-link
	// regular files in place; it never follows or replaces a link.
	bool prepareLogicalScratchFiles() noexcept;

	// Materialize an already authenticated native checkpoint into the fixed
	// VFS-visible scratch entry without replacing that entry. Both source and
	// destination are revalidated by native handles, and the copied bytes must
	// still match the manifest envelope exactly.
	bool materializeCheckpoint(DedicatedCampaignSlot slot,
		const std::filesystem::path& sourceCheckpoint,
		std::uint64_t expectedSize,
		const DedicatedCampaignCheckpointSha256& expectedSha256) noexcept;

	const std::filesystem::path& profileDirectory() const noexcept;
	std::filesystem::path logicalScratchPath(
		DedicatedCampaignSlot slot) const noexcept;

private:
	std::filesystem::path profileDirectory_;
};

#endif
