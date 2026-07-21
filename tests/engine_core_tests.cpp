#include <Engine/Core/AssetSource.h>
#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/CommandStream.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeCapabilities.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
int failures = 0;

void check(bool condition, const char* message)
{
	if (!condition)
	{
		++failures;
		std::printf("FAIL  %s\n", message);
		return;
	}
	std::printf("ok    %s\n", message);
}
}

int main()
{
	std::string path;
	check(NormalizeAssetPath("TableData\\Items.XML", path) &&
		path == "tabledata/items.xml",
		"compiled core normalizes portable asset paths");
	check(!NormalizeAssetPath("../Data/secret", path),
		"compiled core rejects traversal paths");
	RuntimeCapabilities capabilities;
	check(capabilities.add("engine.rendering") &&
		capabilities.add("tool.map-editor") &&
		!capabilities.add("engine.rendering") &&
		!capabilities.add("invalid/capability") &&
		capabilities.ids() == std::vector<std::string>({
			"engine.rendering", "tool.map-editor"}),
		"runtime capabilities are portable, unique, and deterministically ordered");

	CommandStream<std::string> commandStream(8);
	check(commandStream.submit(4, "live") == 0 &&
		commandStream.submitRecorded(3, 7, "recorded") &&
		!commandStream.submitRecorded(5, 7, "duplicate"),
		"generic command stream assigns and protects deterministic sequences");
	const std::vector<ScheduledCommand<std::string>> stagedCommands{
		{6, 8, "batch-a"}, {6, 9, "batch-b"}};
	check(commandStream.stageRecordedBatch(stagedCommands) &&
		commandStream.journal().size() == 4 &&
		commandStream.queue().size() == 4,
		"generic command stream stages a complete batch with matching journal records");
	const std::vector<ScheduledCommand<std::string>> conflictingCommands{
		{8, 10, "would-stage"}, {8, 7, "conflict"}};
	check(!commandStream.stageRecordedBatch(conflictingCommands) &&
		commandStream.journal().size() == 4 &&
		commandStream.queue().size() == 4,
		"generic command stream rejects a conflicting batch transactionally");

	BinaryWriter writer;
	WritePersistenceHeader(writer, PersistenceHeader{0x4A413243u, 7});
	writer.writeU32(0x10203040u);
	BinaryReader reader(writer.bytes());
	PersistenceHeader header{};
	std::uint32_t payload = 0;
	check(ReadPersistenceHeader(reader, 0x4A413243u, 6, 7, header) &&
		reader.readU32(payload) && payload == 0x10203040u && reader.remaining() == 0,
		"compiled core preserves versioned little-endian archives");

	MemoryByteStorage persistenceStorage;
	PersistenceService persistence(persistenceStorage, 8);
	const std::vector<std::uint8_t> persisted{1, 3, 3, 7};
	check(persistence.saveEnvelope(
		"engine.record", PersistenceHeader{0x454E4750u, 2}, persisted) ==
		PersistenceSaveResult::Success,
		"compiled persistence writes bounded checksummed envelopes");
	PersistenceHeader persistedHeader{};
	std::vector<std::uint8_t> loadedPersisted;
	check(persistence.loadEnvelope("engine.record", 0x454E4750u, 1, 2,
		persistedHeader, loadedPersisted) == PersistenceLoadResult::Success &&
		persistedHeader.version == 2 && loadedPersisted == persisted,
		"compiled persistence validates and publishes complete envelopes");
	std::vector<std::uint8_t> corrupted;
	persistenceStorage.readAll("engine.record", corrupted);
	corrupted.back() ^= 0xffu;
	persistenceStorage.writeAll("engine.corrupt", corrupted);
	PersistenceHeader unchangedHeader{99, 99};
	std::vector<std::uint8_t> unchangedPayload{9};
	check(persistence.loadEnvelope("engine.corrupt", 0x454E4750u, 1, 2,
		unchangedHeader, unchangedPayload) == PersistenceLoadResult::IntegrityFailure &&
		unchangedHeader.magic == 99 && unchangedHeader.version == 99 &&
		unchangedPayload == std::vector<std::uint8_t>({9}),
		"failed envelope loads leave caller state unchanged");
	check(persistence.saveEnvelope("too-large", PersistenceHeader{1, 1},
		std::vector<std::uint8_t>(9, 0)) == PersistenceSaveResult::TooLarge,
		"compiled persistence rejects payloads above the configured bound");

	ContentRegistry content(ContentApiVersion{1, 2});
	check(content.registerContent(ContentManifest{
		"engine.test", "1", ContentApiVersion{1, 0}, {}}) ==
		ContentRegistrationError::None,
		"compiled core registry links without game or platform libraries");

	return failures == 0 ? 0 : 1;
}
