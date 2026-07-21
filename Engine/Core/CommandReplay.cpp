#include <Engine/Core/CommandReplay.h>

#include <utility>

namespace
{
constexpr std::uint32_t ReplayMagic = 0x314c5052u; // "RPL1"
constexpr std::uint16_t ReplayVersion = 1;

CommandReplaySaveResult MapSaveResult(PersistenceSaveResult result)
{
	switch (result)
	{
		case PersistenceSaveResult::Success:
			return CommandReplaySaveResult::Success;
		case PersistenceSaveResult::TooLarge:
			return CommandReplaySaveResult::TooLarge;
		case PersistenceSaveResult::InvalidRequest:
			return CommandReplaySaveResult::Invalid;
		case PersistenceSaveResult::StorageError:
			return CommandReplaySaveResult::StorageError;
	}
	return CommandReplaySaveResult::StorageError;
}

CommandReplayLoadResult MapLoadResult(PersistenceLoadResult result)
{
	switch (result)
	{
		case PersistenceLoadResult::Success:
			return CommandReplayLoadResult::Success;
		case PersistenceLoadResult::NotFound:
			return CommandReplayLoadResult::NotFound;
		case PersistenceLoadResult::InvalidOrUnsupported:
			return CommandReplayLoadResult::Invalid;
		case PersistenceLoadResult::TooLarge:
			return CommandReplayLoadResult::TooLarge;
		case PersistenceLoadResult::IntegrityFailure:
			return CommandReplayLoadResult::IntegrityFailure;
		case PersistenceLoadResult::StorageError:
			return CommandReplayLoadResult::StorageError;
	}
	return CommandReplayLoadResult::StorageError;
}
}

CommandReplaySaveResult CommandReplayService::save(
	const std::string& path, const SimulationCommandReplay& replay) const noexcept
{
	try
	{
		std::vector<std::uint8_t> payload;
		if (!EncodeSimulationCommandJournal(
				replay.records, replay.droppedCount, payload))
			return CommandReplaySaveResult::Invalid;
		return MapSaveResult(persistence_.saveEnvelope(
			path, PersistenceHeader{ReplayMagic, ReplayVersion}, payload));
	}
	catch (...)
	{
		return CommandReplaySaveResult::StorageError;
	}
}

CommandReplayLoadResult CommandReplayService::load(
	const std::string& path, SimulationCommandReplay& replay) const noexcept
{
	try
	{
		PersistenceHeader header{};
		std::vector<std::uint8_t> payload;
		const PersistenceLoadResult persistenceResult = persistence_.loadEnvelope(
			path, ReplayMagic, ReplayVersion, ReplayVersion, header, payload);
		if (persistenceResult != PersistenceLoadResult::Success)
			return MapLoadResult(persistenceResult);

		SimulationCommandReplay decoded;
		const SimulationCommandJournalDecodeResult codecResult =
			DecodeSimulationCommandJournal(
				payload, decoded.records, decoded.droppedCount);
		switch (codecResult)
		{
			case SimulationCommandJournalDecodeResult::Success:
				replay = std::move(decoded);
				return CommandReplayLoadResult::Success;
			case SimulationCommandJournalDecodeResult::UnsupportedVersion:
				return CommandReplayLoadResult::UnsupportedVersion;
			case SimulationCommandJournalDecodeResult::TooManyRecords:
				return CommandReplayLoadResult::TooLarge;
			case SimulationCommandJournalDecodeResult::Invalid:
				return CommandReplayLoadResult::Invalid;
		}
		return CommandReplayLoadResult::Invalid;
	}
	catch (...)
	{
		return CommandReplayLoadResult::StorageError;
	}
}
