#include <Engine/Core/RuntimeReportJson.h>

#include <charconv>
#include <cmath>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace
{
class JsonWriter
{
public:
	explicit JsonWriter(std::size_t maximumBytes) : maximumBytes_(maximumBytes)
	{
		output_.reserve(maximumBytes < 4096 ? maximumBytes : 4096);
	}

	bool beginObject() { return beginValue() && append("{") && push('o'); }
	bool endObject() { return pop('o') && append("}"); }
	bool beginArray() { return beginValue() && append("[") && push('a'); }
	bool endArray() { return pop('a') && append("]"); }

	bool key(std::string_view key)
	{
		if (stack_.empty() || stack_.back().kind != 'o' || stack_.back().expectsValue)
			return invalidate();
		if (!stack_.back().first && !append(",")) return false;
		stack_.back().first = false;
		if (!escaped(key) || !append(":")) return false;
		stack_.back().expectsValue = true;
		return true;
	}

	bool string(std::string_view value) { return beginValue() && escaped(value); }
	bool boolean(bool value) { return beginValue() && append(value ? "true" : "false"); }
	bool nullValue() { return beginValue() && append("null"); }
	bool unsignedNumber(std::uint64_t value) { return number(value); }
	bool signedNumber(std::int64_t value) { return number(value); }
	bool realNumber(double value)
	{
		if (!std::isfinite(value)) return invalidate();
		if (!beginValue()) return false;
		char buffer[64];
		const auto converted = std::to_chars(
			buffer, buffer + sizeof(buffer), value, std::chars_format::general);
		return converted.ec == std::errc{}
			? append(std::string_view(buffer,
				static_cast<std::size_t>(converted.ptr - buffer)))
			: invalidate();
	}

	RuntimeReportJsonError error() const { return error_; }
	std::string take() { return std::move(output_); }

private:
	struct Scope
	{
		char kind;
		bool first = true;
		bool expectsValue = false;
	};

	template<typename Integer>
	bool number(Integer value)
	{
		if (!beginValue()) return false;
		char buffer[32];
		const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
		return converted.ec == std::errc{}
			? append(std::string_view(buffer,
				static_cast<std::size_t>(converted.ptr - buffer)))
			: invalidate();
	}

	bool beginValue()
	{
		if (error_ != RuntimeReportJsonError::None) return false;
		if (stack_.empty())
		{
			if (rootWritten_) return invalidate();
			rootWritten_ = true;
			return true;
		}
		Scope& parent = stack_.back();
		if (parent.kind == 'o')
		{
			if (!parent.expectsValue) return invalidate();
			parent.expectsValue = false;
			return true;
		}
		if (!parent.first && !append(",")) return false;
		parent.first = false;
		return true;
	}

	bool push(char kind)
	{
		stack_.push_back(Scope{kind});
		return true;
	}

	bool pop(char kind)
	{
		if (stack_.empty() || stack_.back().kind != kind ||
			stack_.back().expectsValue) return invalidate();
		stack_.pop_back();
		return true;
	}

	bool append(std::string_view text)
	{
		if (error_ != RuntimeReportJsonError::None) return false;
		if (text.size() > maximumBytes_ - output_.size())
		{
			error_ = RuntimeReportJsonError::TooLarge;
			return false;
		}
		output_.append(text.data(), text.size());
		return true;
	}

	bool escaped(std::string_view value)
	{
		if (!append("\"")) return false;
		for (std::size_t index = 0; index < value.size(); ++index)
		{
			const unsigned char byte = static_cast<unsigned char>(value[index]);
			switch (byte)
			{
				case '"': if (!append("\\\"")) return false; continue;
				case '\\': if (!append("\\\\")) return false; continue;
				case '\b': if (!append("\\b")) return false; continue;
				case '\f': if (!append("\\f")) return false; continue;
				case '\n': if (!append("\\n")) return false; continue;
				case '\r': if (!append("\\r")) return false; continue;
				case '\t': if (!append("\\t")) return false; continue;
				default: break;
			}
			if (byte < 0x20u)
			{
				const char hex[] = "0123456789abcdef";
				char escapedControl[6] = {'\\', 'u', '0', '0',
					hex[(byte >> 4u) & 0x0fu], hex[byte & 0x0fu]};
				if (!append(std::string_view(escapedControl, sizeof(escapedControl)))) return false;
				continue;
			}
			if (byte < 0x80u)
			{
				if (!append(std::string_view(value.data() + index, 1))) return false;
				continue;
			}
			const std::size_t sequence = validUtf8Sequence(value, index);
			if (sequence == 0)
			{
				if (!append("\\ufffd")) return false;
				continue;
			}
			if (!append(value.substr(index, sequence))) return false;
			index += sequence - 1;
		}
		return append("\"");
	}

	static bool continuation(unsigned char byte)
	{
		return (byte & 0xc0u) == 0x80u;
	}

	static std::size_t validUtf8Sequence(std::string_view text, std::size_t index)
	{
		const unsigned char first = static_cast<unsigned char>(text[index]);
		if (first >= 0xc2u && first <= 0xdfu)
			return index + 1 < text.size() &&
				continuation(static_cast<unsigned char>(text[index + 1])) ? 2 : 0;
		if (first >= 0xe0u && first <= 0xefu)
		{
			if (index + 2 >= text.size()) return 0;
			const unsigned char second = static_cast<unsigned char>(text[index + 1]);
			return continuation(second) &&
				continuation(static_cast<unsigned char>(text[index + 2])) &&
				!(first == 0xe0u && second < 0xa0u) &&
				!(first == 0xedu && second >= 0xa0u) ? 3 : 0;
		}
		if (first >= 0xf0u && first <= 0xf4u)
		{
			if (index + 3 >= text.size()) return 0;
			const unsigned char second = static_cast<unsigned char>(text[index + 1]);
			return continuation(second) &&
				continuation(static_cast<unsigned char>(text[index + 2])) &&
				continuation(static_cast<unsigned char>(text[index + 3])) &&
				!(first == 0xf0u && second < 0x90u) &&
				!(first == 0xf4u && second >= 0x90u) ? 4 : 0;
		}
		return 0;
	}

	bool invalidate()
	{
		if (error_ == RuntimeReportJsonError::None)
			error_ = RuntimeReportJsonError::InvalidValue;
		return false;
	}

	std::size_t maximumBytes_;
	std::string output_;
	std::vector<Scope> stack_;
	RuntimeReportJsonError error_ = RuntimeReportJsonError::None;
	bool rootWritten_ = false;
};

const char* LifecycleName(EngineLifecycle lifecycle)
{
	switch (lifecycle)
	{
		case EngineLifecycle::Stopped: return "stopped";
		case EngineLifecycle::Initializing: return "initializing";
		case EngineLifecycle::Running: return "running";
		case EngineLifecycle::ShuttingDown: return "shutting-down";
	}
	return "unknown";
}

const char* PackageKindName(PackageKind kind)
{
	switch (kind)
	{
		case PackageKind::Campaign: return "campaign";
		case PackageKind::Rules: return "rules";
		case PackageKind::Extension: return "extension";
		case PackageKind::Tool: return "tool";
	}
	return "unknown";
}

const char* FaultKindName(RuntimeFaultKind kind)
{
	switch (kind)
	{
		case RuntimeFaultKind::ServiceContract: return "service-contract";
		case RuntimeFaultKind::CapabilityContract: return "capability-contract";
		case RuntimeFaultKind::DeferredTask: return "deferred-task";
		case RuntimeFaultKind::Bootstrap: return "bootstrap";
		case RuntimeFaultKind::Shutdown: return "shutdown";
		case RuntimeFaultKind::Input: return "input";
		case RuntimeFaultKind::RuntimeUpdate: return "runtime-update";
		case RuntimeFaultKind::SimulationTick: return "simulation-tick";
		case RuntimeFaultKind::Message: return "message";
		case RuntimeFaultKind::SaveState: return "save-state";
		case RuntimeFaultKind::LoadState: return "load-state";
	}
	return "unknown";
}

bool WriteStringArray(JsonWriter& writer, const std::vector<std::string>& values)
{
	if (!writer.beginArray()) return false;
	for (const std::string& value : values)
		if (!writer.string(value)) return false;
	return writer.endArray();
}

bool WriteResources(JsonWriter& writer, const PackageResourceUsage& usage)
{
	return writer.beginObject() &&
		writer.key("localizationEntries") && writer.unsignedNumber(usage.localizationEntries) &&
		writer.key("localizationTextBytes") && writer.unsignedNumber(usage.localizationTextBytes) &&
		writer.key("definitionEntries") && writer.unsignedNumber(usage.definitionEntries) &&
		writer.key("definitionPayloadBytes") && writer.unsignedNumber(usage.definitionPayloadBytes) &&
		writer.key("entities") && writer.unsignedNumber(usage.entities) &&
		writer.key("audioPlaybacks") && writer.unsignedNumber(usage.audioPlaybacks) &&
		writer.key("deferredTasks") && writer.unsignedNumber(usage.deferredTasks) &&
		writer.key("randomStreams") && writer.unsignedNumber(usage.randomStreams) &&
		writer.key("randomValuesGenerated") && writer.unsignedNumber(usage.randomValuesGenerated) &&
		writer.endObject();
}

bool WriteHealth(JsonWriter& writer, const PackageRuntimeHealth& health)
{
	return writer.beginObject() &&
		writer.key("healthy") && writer.boolean(health.healthy()) &&
		writer.key("inputCallbacks") && writer.unsignedNumber(health.inputCallbacks) &&
		writer.key("inputFailures") && writer.unsignedNumber(health.inputFailures) &&
		writer.key("runtimeUpdateCallbacks") && writer.unsignedNumber(health.runtimeUpdateCallbacks) &&
		writer.key("runtimeUpdateFailures") && writer.unsignedNumber(health.runtimeUpdateFailures) &&
		writer.key("simulationTickCallbacks") && writer.unsignedNumber(health.simulationTickCallbacks) &&
		writer.key("simulationTickFailures") && writer.unsignedNumber(health.simulationTickFailures) &&
		writer.key("messageCallbacks") && writer.unsignedNumber(health.messageCallbacks) &&
		writer.key("messageFailures") && writer.unsignedNumber(health.messageFailures) &&
		writer.key("filteredMessages") && writer.unsignedNumber(health.filteredMessages) &&
		writer.key("suppressedFailureLogs") && writer.unsignedNumber(health.suppressedFailureLogs) &&
		writer.endObject();
}

bool WriteConfigurationValue(JsonWriter& writer, const RuntimeConfigurationValue& value)
{
	if (const bool* item = std::get_if<bool>(&value)) return writer.boolean(*item);
	if (const std::int64_t* item = std::get_if<std::int64_t>(&value))
		return writer.signedNumber(*item);
	if (const double* item = std::get_if<double>(&value)) return writer.realNumber(*item);
	if (const std::string* item = std::get_if<std::string>(&value)) return writer.string(*item);
	return false;
}
}

RuntimeReportJsonResult SerializeRuntimeReportJson(
	const RuntimeReport& report, std::size_t maximumBytes) noexcept
{
	try
	{
		JsonWriter writer(maximumBytes);
		bool ok = writer.beginObject() &&
			writer.key("schema") && writer.unsignedNumber(report.schema) &&
			writer.key("healthy") && writer.boolean(report.healthy()) &&
			writer.key("lifecycle") && writer.string(LifecycleName(report.lifecycle)) &&
			writer.key("compatibility") && writer.beginObject() &&
			writer.key("schema") && writer.unsignedNumber(report.compatibility.schema) &&
			writer.key("fingerprint") && writer.string(report.compatibility.hex()) &&
			writer.endObject() &&
			writer.key("progress") && writer.beginObject() &&
			writer.key("completedFrames") && writer.unsignedNumber(report.completedFrames) &&
			writer.key("completedSimulationTicks") && writer.unsignedNumber(report.completedSimulationTicks) &&
			writer.key("queuedMessages") && writer.unsignedNumber(report.queuedMessages) &&
			writer.key("completedBootstrapPhases") && writer.unsignedNumber(report.completedBootstrapPhases) &&
			writer.endObject() &&
			writer.key("packageCounts") && writer.beginObject() &&
			writer.key("registered") && writer.unsignedNumber(report.registeredPackages) &&
			writer.key("active") && writer.unsignedNumber(report.activePackages) &&
			writer.key("unhealthy") && writer.unsignedNumber(report.unhealthyPackages) &&
			writer.endObject() &&
			writer.key("frames") && writer.beginObject() &&
			writer.key("completed") && writer.unsignedNumber(report.frames.completedFrames) &&
			writer.key("presented") && writer.unsignedNumber(report.frames.presentedFrames) &&
			writer.key("totalMicroseconds") && writer.unsignedNumber(report.frames.totalMicroseconds) &&
			writer.key("maximumMicroseconds") && writer.unsignedNumber(report.frames.maximumFrameMicroseconds) &&
			writer.key("inputSourceDrops") && writer.unsignedNumber(report.frames.inputSourceDrops) &&
			writer.key("inputCallbackFailures") && writer.unsignedNumber(report.frames.inputCallbackFailures) &&
			writer.key("runtimeUpdateCallbackFailures") && writer.unsignedNumber(report.frames.runtimeUpdateCallbackFailures) &&
			writer.key("simulationTickCallbackFailures") && writer.unsignedNumber(report.frames.simulationTickCallbackFailures) &&
			writer.key("simulationTicksDropped") && writer.unsignedNumber(report.frames.simulationTicksDropped) &&
			writer.key("messageCallbackFailures") && writer.unsignedNumber(report.frames.messageCallbackFailures) &&
			writer.key("messagesDelivered") && writer.unsignedNumber(report.frames.messagesDelivered) &&
			writer.key("evictedSamples") && writer.unsignedNumber(report.frames.evictedSamples) &&
			writer.key("storageFailures") && writer.unsignedNumber(report.frames.storageFailures) &&
			writer.endObject() &&
			writer.key("assetCache") && writer.beginObject() &&
			writer.key("hits") && writer.unsignedNumber(report.assetCache.hits) &&
			writer.key("misses") && writer.unsignedNumber(report.assetCache.misses) &&
			writer.key("insertions") && writer.unsignedNumber(report.assetCache.insertions) &&
			writer.key("evictions") && writer.unsignedNumber(report.assetCache.evictions) &&
			writer.key("oversizedAssets") && writer.unsignedNumber(report.assetCache.oversizedAssets) &&
			writer.key("allocationFailures") && writer.unsignedNumber(report.assetCache.allocationFailures) &&
			writer.key("entries") && writer.unsignedNumber(report.assetCache.entries) &&
			writer.key("bytes") && writer.unsignedNumber(report.assetCache.bytes) &&
			writer.endObject() &&
			writer.key("faultSummary") && writer.beginObject() &&
			writer.key("observed") && writer.unsignedNumber(report.faultSummary.observed) &&
			writer.key("retained") && writer.unsignedNumber(report.faultSummary.retained) &&
			writer.key("evicted") && writer.unsignedNumber(report.faultSummary.evicted) &&
			writer.key("storageFailures") && writer.unsignedNumber(report.faultSummary.storageFailures) &&
			writer.key("sequenceExhausted") && writer.boolean(report.faultSummary.sequenceExhausted) &&
			writer.endObject() &&
			writer.key("faults") && writer.beginArray();
		for (const RuntimeFaultRecord& fault : report.faults)
			ok = ok && writer.beginObject() &&
				writer.key("sequence") && writer.unsignedNumber(fault.sequence) &&
				writer.key("kind") && writer.string(FaultKindName(fault.kind)) &&
				writer.key("package") && writer.string(fault.packageId) &&
				writer.key("callback") && writer.string(fault.callback) &&
				writer.key("occurrence") && writer.unsignedNumber(fault.occurrence) &&
				writer.endObject();
		ok = ok && writer.endArray() &&
			writer.key("resources") && WriteResources(writer, report.totalResources) &&
			writer.key("unattributedResourceRecords") &&
			writer.unsignedNumber(report.unattributedResourceRecords) &&
			writer.key("capabilities") && WriteStringArray(writer, report.capabilities.ids()) &&
			writer.key("services") && writer.beginArray();
		for (const EngineServiceDescriptor& service : report.services)
			ok = ok && writer.beginObject() &&
				writer.key("id") && writer.string(service.id) &&
				writer.key("major") && writer.unsignedNumber(service.version.major) &&
				writer.key("minor") && writer.unsignedNumber(service.version.minor) &&
				writer.endObject();
		ok = ok && writer.endArray() && writer.key("configuration") && writer.beginArray();
		for (const RuntimeConfigurationEntry& entry : report.configuration)
			ok = ok && writer.beginObject() &&
				writer.key("key") && writer.string(entry.key) &&
				writer.key("value") && WriteConfigurationValue(writer, entry.value) &&
				writer.endObject();
		ok = ok && writer.endArray() && writer.key("packages") && writer.beginArray();
		for (const RuntimeReportPackage& package : report.packages)
		{
			const ContentManifest& content = package.descriptor.content;
			ok = ok && writer.beginObject() &&
				writer.key("id") && writer.string(content.id) &&
				writer.key("version") && writer.string(content.version) &&
				writer.key("contentApi") && writer.beginObject() &&
				writer.key("major") && writer.unsignedNumber(content.requiredApi.major) &&
				writer.key("minor") && writer.unsignedNumber(content.requiredApi.minor) &&
				writer.endObject() &&
				writer.key("kind") && writer.string(PackageKindName(package.descriptor.kind)) &&
				writer.key("active") && writer.boolean(package.active()) &&
				writer.key("assetsMounted") && writer.boolean(package.assetsMounted) &&
				writer.key("activationIndex");
			ok = ok && (package.activationIndex == PackageCatalogEntry::NotActive
				? writer.nullValue() : writer.unsignedNumber(package.activationIndex));
			ok = ok && writer.key("dependents") && WriteStringArray(writer, package.dependents) &&
				writer.key("capabilities") && WriteStringArray(writer, package.descriptor.capabilities) &&
				writer.key("requiredCapabilities") &&
				WriteStringArray(writer, package.descriptor.requiredCapabilities) &&
				writer.key("localizationSources") &&
				writer.unsignedNumber(package.descriptor.localizationSources.size()) &&
				writer.key("definitionSources") &&
				writer.unsignedNumber(package.descriptor.definitionSources.size()) &&
				writer.key("saveStateSchema") &&
				writer.unsignedNumber(package.descriptor.saveStateSchemaVersion) &&
				writer.key("health") && WriteHealth(writer, package.runtimeHealth) &&
				writer.key("resources") && WriteResources(writer, package.resources) &&
				writer.endObject();
		}
		ok = ok && writer.endArray() && writer.endObject();
		if (!ok)
			return {writer.error(), {}};
		return {RuntimeReportJsonError::None, writer.take()};
	}
	catch (...)
	{
		return {RuntimeReportJsonError::AllocationFailure, {}};
	}
}
