#include "DedicatedCampaignBoot.h"

#include <algorithm>
#include <utility>

namespace
{
const std::filesystem::path& EmptyPath() noexcept
{
	static const std::filesystem::path empty;
	return empty;
}

bool ZeroDigest(
	const DedicatedCampaignContentManifestSha256& digest) noexcept
{
	return std::all_of(digest.begin(), digest.end(),
		[](std::uint8_t value) { return value == 0; });
}

bool SameFingerprint(const DedicatedCampaignRuntimeFingerprint& left,
	const DedicatedCampaignRuntimeFingerprint& right) noexcept
{
	return left.schema == right.schema && left.high == right.high &&
		left.low == right.low;
}

bool SameIdentity(const DedicatedCampaignIdentity& left,
	const DedicatedCampaignIdentity& right) noexcept
{
	return left.campaignId == right.campaignId && left.mode == right.mode &&
		SameFingerprint(left.runtimeFingerprint, right.runtimeFingerprint) &&
		left.contentManifestSha256 == right.contentManifestSha256 &&
		left.campaignSeed == right.campaignSeed;
}

DedicatedCampaignBootResult Failure(DedicatedCampaignBootError error) noexcept
{
	DedicatedCampaignBootResult result;
	result.error = error;
	return result;
}
}

DedicatedCampaignBoot::DedicatedCampaignBoot() noexcept
	: backend_(&ownedBackend_)
{
}

DedicatedCampaignBoot::DedicatedCampaignBoot(
	DedicatedCampaignFilesystemBackend& backend) noexcept
	: backend_(&backend)
{
}

DedicatedCampaignBoot::~DedicatedCampaignBoot() noexcept
{
	close();
}

DedicatedCampaignBootResult DedicatedCampaignBoot::prepare(
	const DedicatedServerOptions& options) noexcept
{
	if (state_ != DedicatedCampaignBootState::Fresh)
		return Failure(DedicatedCampaignBootError::InvalidState);
	if (!options.enabled || options.mode != DedicatedServerMode::Coop ||
		(options.campaignAction != DedicatedCampaignAction::Create &&
			options.campaignAction != DedicatedCampaignAction::Resume) ||
		options.campaignId.empty() || options.stateDirectory.empty())
		return poison(DedicatedCampaignBootError::InvalidOptions);

	try
	{
		const DedicatedCampaignFilesystemError opened = backend_->open(
			std::filesystem::u8path(options.stateDirectory.begin(),
				options.stateDirectory.end()), options.campaignId);
		if (opened != DedicatedCampaignFilesystemError::None)
			return poison(DedicatedCampaignBootError::FilesystemOpenFailed,
				opened);
		// Recovery must never inspect or rotate a profile while checkpoint
		// ownership is already ambiguous. Reject injected/pre-bound backends
		// before touching any campaign state.
		if (backend_->checkpointWriterBound())
			return poison(
				DedicatedCampaignBootError::CheckpointWriterBindFailed);

		const DedicatedCampaignProfileDirectoryState profileState =
			backend_->profileDirectoryState();
		if (profileState == DedicatedCampaignProfileDirectoryState::Failure)
			return poison(DedicatedCampaignBootError::ProfileInspectionFailed);
		if (options.campaignAction == DedicatedCampaignAction::Create)
		{
			const DedicatedCampaignProfileRecoveryResult recovered =
				backend_->recoverProfileForNewCampaign();
			if (recovered ==
				DedicatedCampaignProfileRecoveryResult::CommittedStatePresent)
				return poison(
					DedicatedCampaignBootError::CreateProfileNotEmpty);
			if (recovered != DedicatedCampaignProfileRecoveryResult::Ready)
				return poison(
					DedicatedCampaignBootError::CreateProfileRecoveryFailed);
		}

		store_.reset(new DedicatedCampaignStore(*backend_));
		std::uint64_t immutableSeed = options.campaignSeed;
		if (options.campaignAction == DedicatedCampaignAction::Resume)
		{
			const DedicatedCampaignStoreError inspected =
				store_->inspectCampaignSeedForResume(options.campaignId,
					DedicatedCampaignMode::Coop, immutableSeed);
			if (inspected != DedicatedCampaignStoreError::None)
				return poison(
					DedicatedCampaignBootError::ResumeSeedInspectionFailed,
					DedicatedCampaignFilesystemError::None, inspected);
		}

		saveAdapter_.reset(
			new DedicatedCampaignSaveAdapter(backend_->profileDirectory()));
		if (!saveAdapter_->prepareLogicalScratchFiles())
			return poison(
				DedicatedCampaignBootError::ScratchPreparationFailed);
		if (!backend_->bindCheckpointWriter(*saveAdapter_))
			return poison(
				DedicatedCampaignBootError::CheckpointWriterBindFailed);

		campaignId_ = options.campaignId;
		campaignSeed_ = immutableSeed;
	state_ = options.campaignAction == DedicatedCampaignAction::Create
			? DedicatedCampaignBootState::PreparedCreate
			: DedicatedCampaignBootState::PreparedResume;
		// VFS may mount profileDirectory() as soon as prepare() returns. From
		// this point onward every failure must retain the process lease until
		// the owner tears VFS down and explicitly closes this coordinator.
		retainLeaseUntilClose_ = true;
		return {};
	}
	catch (...)
	{
		return poison(DedicatedCampaignBootError::ResourceFailure);
	}
}

DedicatedCampaignBootResult DedicatedCampaignBoot::openCampaign(
	const DedicatedCampaignRuntimeFingerprint& runtimeFingerprint,
	const DedicatedCampaignContentManifestSha256&
		contentManifestSha256) noexcept
{
	if (!isPrepared())
		return Failure(DedicatedCampaignBootError::InvalidState);
	if (runtimeFingerprint.schema == 0)
		return poison(
			DedicatedCampaignBootError::InvalidRuntimeFingerprint);
	if (ZeroDigest(contentManifestSha256))
		return poison(
			DedicatedCampaignBootError::MissingContentManifestSha256);

	try
	{
		const bool resume =
			state_ == DedicatedCampaignBootState::PreparedResume;
		if (resume)
		{
			std::uint64_t reinspectedSeed = 0;
			const DedicatedCampaignStoreError inspected =
				store_->inspectCampaignSeedForResume(campaignId_,
					DedicatedCampaignMode::Coop, reinspectedSeed);
			if (inspected != DedicatedCampaignStoreError::None)
				return poison(
					DedicatedCampaignBootError::ResumeSeedInspectionFailed,
					DedicatedCampaignFilesystemError::None, inspected);
			if (reinspectedSeed != campaignSeed_)
				return poison(DedicatedCampaignBootError::ResumeSeedChanged);
		}

		DedicatedCampaignIdentity identity;
		identity.campaignId = campaignId_;
		identity.mode = DedicatedCampaignMode::Coop;
		identity.runtimeFingerprint = runtimeFingerprint;
		identity.contentManifestSha256 = contentManifestSha256;
		identity.campaignSeed = campaignSeed_;

		const DedicatedCampaignStoreError opened = resume
			? store_->resume(identity) : store_->create(identity);
		if (opened != DedicatedCampaignStoreError::None)
			return poison(DedicatedCampaignBootError::StoreOpenFailed,
				DedicatedCampaignFilesystemError::None, opened);

		const DedicatedCampaignStoreState* openedState = store_->state();
		if (openedState == nullptr ||
			!SameIdentity(openedState->identity, identity) ||
			(resume && !openedState->hasCheckpoint) ||
			(!resume && openedState->hasCheckpoint))
			return poison(DedicatedCampaignBootError::InvalidStoreState);

		if (resume && !saveAdapter_->materializeCheckpoint(
				openedState->activeSlot,
				backend_->checkpointPath(openedState->activeSlot),
				openedState->checkpointSize,
				openedState->checkpointSha256))
			return poison(
				DedicatedCampaignBootError::CheckpointMaterializationFailed);

		state_ = resume ? DedicatedCampaignBootState::OpenResume
			: DedicatedCampaignBootState::OpenCreate;
		entry_ = resume ? DedicatedCampaignBootEntry::ResumeCheckpoint
			: DedicatedCampaignBootEntry::CreateNewCampaign;
		return {};
	}
	catch (...)
	{
		return poison(DedicatedCampaignBootError::ResourceFailure);
	}
}

DedicatedCampaignBootResult DedicatedCampaignBoot::checkpoint(
	std::uint64_t worldMinutes) noexcept
{
	if (!isOpen()) return Failure(DedicatedCampaignBootError::InvalidState);
	const DedicatedCampaignStoreError checkpointed =
		store_->checkpoint(worldMinutes);
	if (checkpointed != DedicatedCampaignStoreError::None)
		return poison(DedicatedCampaignBootError::CheckpointFailed,
			DedicatedCampaignFilesystemError::None, checkpointed);
	return {};
}

void DedicatedCampaignBoot::close() noexcept
{
	if (state_ == DedicatedCampaignBootState::Closed) return;
	store_.reset();
	if (backend_ != nullptr) backend_->close();
	retainLeaseUntilClose_ = false;
	saveAdapter_.reset();
	campaignId_.clear();
	campaignSeed_ = 0;
	entry_ = DedicatedCampaignBootEntry::None;
	state_ = DedicatedCampaignBootState::Closed;
}

DedicatedCampaignBootState DedicatedCampaignBoot::state() const noexcept
{
	return state_;
}

DedicatedCampaignBootEntry DedicatedCampaignBoot::entry() const noexcept
{
	return entry_;
}

const std::filesystem::path&
DedicatedCampaignBoot::profileDirectory() const noexcept
{
	return (isPrepared() || isOpen()) && backend_ != nullptr
		? backend_->profileDirectory() : EmptyPath();
}

std::uint64_t DedicatedCampaignBoot::campaignSeed() const noexcept
{
	return isPrepared() || isOpen() ? campaignSeed_ : 0;
}

const DedicatedCampaignStoreState*
DedicatedCampaignBoot::campaignState() const noexcept
{
	return isOpen() && store_ ? store_->state() : nullptr;
}

bool DedicatedCampaignBoot::openActiveCheckpointReader(
	DedicatedCampaignCheckpointReader& reader) noexcept
{
	if (!isOpen() || store_ == nullptr || backend_ == nullptr) return false;
	const DedicatedCampaignStoreState* active = store_->state();
	return active != nullptr && active->hasCheckpoint &&
		backend_->openCheckpointReader(*active, reader);
}

DedicatedCampaignBootResult DedicatedCampaignBoot::poison(
	DedicatedCampaignBootError error,
	DedicatedCampaignFilesystemError filesystemError,
	DedicatedCampaignStoreError storeError) noexcept
{
	store_.reset();
	if (!retainLeaseUntilClose_)
	{
		if (backend_ != nullptr) backend_->close();
		saveAdapter_.reset();
	}
	campaignId_.clear();
	campaignSeed_ = 0;
	entry_ = DedicatedCampaignBootEntry::None;
	state_ = DedicatedCampaignBootState::Poisoned;
	DedicatedCampaignBootResult result;
	result.error = error;
	result.filesystemError = filesystemError;
	result.storeError = storeError;
	return result;
}

bool DedicatedCampaignBoot::isPrepared() const noexcept
{
	return state_ == DedicatedCampaignBootState::PreparedCreate ||
		state_ == DedicatedCampaignBootState::PreparedResume;
}

bool DedicatedCampaignBoot::isOpen() const noexcept
{
	return state_ == DedicatedCampaignBootState::OpenCreate ||
		state_ == DedicatedCampaignBootState::OpenResume;
}

const char* DedicatedCampaignBootErrorName(
	DedicatedCampaignBootError error) noexcept
{
	switch (error)
	{
		case DedicatedCampaignBootError::None: return "none";
		case DedicatedCampaignBootError::InvalidState: return "invalid state";
		case DedicatedCampaignBootError::InvalidOptions: return "invalid options";
		case DedicatedCampaignBootError::ResourceFailure: return "resource failure";
		case DedicatedCampaignBootError::FilesystemOpenFailed: return "filesystem open failed";
		case DedicatedCampaignBootError::ProfileInspectionFailed: return "profile inspection failed";
		case DedicatedCampaignBootError::CreateProfileNotEmpty: return "create profile not empty";
		case DedicatedCampaignBootError::CreateProfileRecoveryFailed: return "create profile recovery failed";
		case DedicatedCampaignBootError::ResumeSeedInspectionFailed: return "resume seed inspection failed";
		case DedicatedCampaignBootError::ScratchPreparationFailed: return "scratch preparation failed";
		case DedicatedCampaignBootError::CheckpointWriterBindFailed: return "checkpoint writer bind failed";
		case DedicatedCampaignBootError::InvalidRuntimeFingerprint: return "invalid runtime fingerprint";
		case DedicatedCampaignBootError::MissingContentManifestSha256: return "missing content manifest SHA-256";
		case DedicatedCampaignBootError::ResumeSeedChanged: return "resume seed changed";
		case DedicatedCampaignBootError::StoreOpenFailed: return "campaign store open failed";
		case DedicatedCampaignBootError::InvalidStoreState: return "invalid campaign store state";
		case DedicatedCampaignBootError::CheckpointMaterializationFailed: return "checkpoint materialization failed";
		case DedicatedCampaignBootError::CheckpointFailed: return "checkpoint failed";
	}
	return "unknown";
}
