#include <Engine/Core/RuntimeFingerprint.h>

#include <cstring>
#include <unordered_map>

namespace
{
class FingerprintBuilder
{
public:
	void addU64(std::uint64_t value)
	{
		for (unsigned shift = 0; shift < 64; shift += 8)
			addByte(static_cast<std::uint8_t>(value >> shift));
	}

	void addString(const std::string& value)
	{
		addBytes(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
	}

	void addBytes(const std::uint8_t* bytes, std::size_t size)
	{
		addU64(size);
		for (std::size_t index = 0; index < size; ++index) addByte(bytes[index]);
	}

	RuntimeCompatibilityFingerprint finish() const
	{
		return RuntimeCompatibilityFingerprint{
			RuntimeCompatibilityFingerprint::CurrentSchema, high_, low_};
	}

private:
	void addByte(std::uint8_t value)
	{
		low_ = (low_ ^ value) * 1099511628211ull;
		high_ = (high_ ^ value) * 14029467366897019727ull;
	}

	std::uint64_t high_ = 7809847782465536322ull;
	std::uint64_t low_ = 14695981039346656037ull;
};

void AddStrings(FingerprintBuilder& builder, const std::vector<std::string>& values)
{
	builder.addU64(values.size());
	for (const std::string& value : values) builder.addString(value);
}

void AddRequirements(
	FingerprintBuilder& builder, const std::vector<ContentRequirement>& requirements)
{
	builder.addU64(requirements.size());
	for (const ContentRequirement& requirement : requirements)
	{
		builder.addString(requirement.id);
		builder.addString(requirement.exactVersion);
	}
}

void AddServiceRequirements(FingerprintBuilder& builder,
	const std::vector<EngineServiceRequirement>& requirements)
{
	builder.addU64(requirements.size());
	for (const EngineServiceRequirement& requirement : requirements)
	{
		builder.addString(requirement.id);
		builder.addU64(requirement.minimumVersion.major);
		builder.addU64(requirement.minimumVersion.minor);
	}
}

void AddPackage(FingerprintBuilder& builder, const PackageDescriptor& descriptor)
{
	const ContentManifest& manifest = descriptor.content;
	builder.addString(manifest.id);
	builder.addString(manifest.version);
	builder.addU64(manifest.requiredApi.major);
	builder.addU64(manifest.requiredApi.minor);
	AddRequirements(builder, manifest.requirements);
	AddRequirements(builder, manifest.optionalRequirements);
	AddStrings(builder, manifest.conflicts);
	AddStrings(builder, manifest.loadAfter);
	builder.addU64(static_cast<std::uint64_t>(descriptor.kind));
	builder.addU64(descriptor.saveStateSchemaVersion);
	AddStrings(builder, descriptor.capabilities);
	AddStrings(builder, descriptor.messageTopics);
	AddServiceRequirements(builder, descriptor.requiredServices);
	AddStrings(builder, descriptor.requiredCapabilities);
}

void AddConfigurationValue(FingerprintBuilder& builder,
	const RuntimeConfigurationValue& value)
{
	builder.addU64(value.index());
	if (const bool* boolean = std::get_if<bool>(&value))
	{
		builder.addU64(*boolean ? 1 : 0);
		return;
	}
	if (const std::int64_t* integer = std::get_if<std::int64_t>(&value))
	{
		builder.addU64(static_cast<std::uint64_t>(*integer));
		return;
	}
	if (const double* real = std::get_if<double>(&value))
	{
		static_assert(sizeof(double) == sizeof(std::uint64_t),
			"runtime fingerprints require 64-bit IEEE-style doubles");
		std::uint64_t bits = 0;
		std::memcpy(&bits, real, sizeof(bits));
		builder.addU64(bits);
		return;
	}
	builder.addString(std::get<std::string>(value));
}

void AppendHex(std::string& output, std::uint64_t value, unsigned digits)
{
	static constexpr char Hex[] = "0123456789abcdef";
	for (unsigned digit = digits; digit > 0; --digit)
		output.push_back(Hex[(value >> ((digit - 1) * 4)) & 0x0f]);
}
}

std::string RuntimeCompatibilityFingerprint::hex() const
{
	std::string result;
	result.reserve(40);
	AppendHex(result, schema, 8);
	AppendHex(result, high, 16);
	AppendHex(result, low, 16);
	return result;
}

RuntimeCompatibilityFingerprint BuildRuntimeCompatibilityFingerprint(
	const PackageCatalogSnapshot& packages,
	const std::vector<EngineServiceDescriptor>& services,
	const std::vector<RuntimeConfigurationEntry>& configuration,
	const RuntimeCapabilities& capabilities,
	const std::vector<DefinitionRecord>& definitions)
{
	FingerprintBuilder builder;
	builder.addString("ja2-engine-runtime-fingerprint");
	builder.addU64(RuntimeCompatibilityFingerprint::CurrentSchema);
	builder.addU64(packages.supportedApi.major);
	builder.addU64(packages.supportedApi.minor);
	builder.addU64(packages.activationOrder.size());
	std::unordered_map<std::string, const PackageCatalogEntry*> packagesById;
	packagesById.reserve(packages.packages.size());
	for (const PackageCatalogEntry& package : packages.packages)
		packagesById.emplace(package.descriptor.content.id, &package);
	for (const std::string& packageId : packages.activationOrder)
	{
		const auto found = packagesById.find(packageId);
		const PackageCatalogEntry* package = found == packagesById.end()
			? nullptr : found->second;
		builder.addU64(package ? 1 : 0);
		if (package) AddPackage(builder, package->descriptor);
		else builder.addString(packageId);
	}
	builder.addU64(services.size());
	for (const EngineServiceDescriptor& service : services)
	{
		builder.addString(service.id);
		builder.addU64(service.version.major);
		builder.addU64(service.version.minor);
	}
	builder.addU64(configuration.size());
	for (const RuntimeConfigurationEntry& entry : configuration)
	{
		builder.addString(entry.key);
		AddConfigurationValue(builder, entry.value);
	}
	AddStrings(builder, capabilities.ids());
	builder.addU64(definitions.size());
	for (const DefinitionRecord& definition : definitions)
	{
		builder.addString(definition.packageId);
		builder.addString(definition.type);
		builder.addString(definition.id);
		builder.addU64(definition.schemaVersion);
		builder.addBytes(definition.payload.data(), definition.payload.size());
	}
	return builder.finish();
}
