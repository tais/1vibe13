#include "PackageHost.h"

#include "GameContext.h"

#include <Engine/Core/AssetSource.h>
#include <Engine/Core/Identifier.h>
#include <Engine/Core/PackageApi.h>
#include <Engine/Core/PackageContentLoader.h>

#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_init.h>
#include <vfs/Core/vfs_profile.h>
#include <vfs/Tools/vfs_property_container.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <list>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
constexpr std::uintmax_t MaximumManifestBytes = 64u * 1024u;
constexpr std::size_t MaximumPackageRoots = 32;
constexpr std::size_t MaximumPackages = 4096;
constexpr std::size_t MaximumRequirements = 128;
constexpr std::size_t MaximumIndexedFiles = 250000;
constexpr std::size_t MaximumTotalIndexedFiles = 1000000;
constexpr std::size_t MaximumIdentifierLength = 128;
constexpr std::size_t MaximumVersionLength = 128;
constexpr std::size_t MaximumLogicalPathLength = 1024;
constexpr std::size_t MaximumDeclaredContentSources = 128;

std::string LowerAscii(std::string value)
{
	for (char& character : value)
	{
		if (character >= 'A' && character <= 'Z')
			character = static_cast<char>(character - 'A' + 'a');
	}
	return value;
}

std::string TrimAscii(const std::string& value)
{
	std::size_t first = 0;
	while (first < value.size() &&
		std::isspace(static_cast<unsigned char>(value[first]))) ++first;
	std::size_t last = value.size();
	while (last > first &&
		std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
	return value.substr(first, last - first);
}

std::vector<std::string> SplitCommaList(const std::string& value)
{
	std::vector<std::string> values;
	std::size_t first = 0;
	while (first <= value.size())
	{
		const std::size_t comma = value.find(',', first);
		values.push_back(TrimAscii(value.substr(first,
			comma == std::string::npos ? std::string::npos : comma - first)));
		if (comma == std::string::npos) break;
		first = comma + 1;
	}
	return values;
}

bool IsSeparateOptionValue(const char* value)
{
	if (!value || value[0] == '\0' || value[0] == '-') return false;
#ifdef _WIN32
	// Preserve the legacy /option spelling on Windows. Drive-letter and UNC
	// paths do not begin with a forward slash and remain valid separate values.
	if (value[0] == '/') return false;
#endif
	return true;
}

bool ParseUnsigned16(const std::string& text, std::uint16_t& value)
{
	if (text.empty()) return false;
	std::uint32_t parsed = 0;
	for (char character : text)
	{
		if (character < '0' || character > '9') return false;
		parsed = parsed * 10u + static_cast<std::uint32_t>(character - '0');
		if (parsed > std::numeric_limits<std::uint16_t>::max()) return false;
	}
	value = static_cast<std::uint16_t>(parsed);
	return true;
}

bool ParseUnsigned32(const std::string& text, std::uint32_t& value)
{
	if (text.empty()) return false;
	std::uint64_t parsed = 0;
	for (char character : text)
	{
		if (character < '0' || character > '9') return false;
		parsed = parsed * 10u + static_cast<std::uint64_t>(character - '0');
		if (parsed > std::numeric_limits<std::uint32_t>::max()) return false;
	}
	if (parsed == 0) return false;
	value = static_cast<std::uint32_t>(parsed);
	return true;
}

bool ParseContentApiVersion(const std::string& text, ContentApiVersion& version)
{
	const std::size_t separator = text.find('.');
	if (separator == std::string::npos || separator == 0 ||
		separator + 1 == text.size() || text.find('.', separator + 1) != std::string::npos)
		return false;
	return ParseUnsigned16(text.substr(0, separator), version.major) &&
		ParseUnsigned16(text.substr(separator + 1), version.minor);
}

bool IsPortableVersion(const std::string& version)
{
	if (version.empty() || version.size() > MaximumVersionLength) return false;
	for (const unsigned char byte : version)
	{
		const bool valid = (byte >= 'a' && byte <= 'z') ||
			(byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
			byte == '.' || byte == '_' || byte == '-' || byte == '+';
		if (!valid) return false;
	}
	return true;
}

bool IsLowercaseIdentifier(const std::string& identifier)
{
	return IsValidEngineIdentifier(identifier) &&
		identifier.size() <= MaximumIdentifierLength && identifier == LowerAscii(identifier);
}

bool IsWindowsDeviceName(const std::string& component)
{
	std::string base = LowerAscii(component.substr(0, component.find('.')));
	if (base == "con" || base == "prn" || base == "aux" || base == "nul") return true;
	if (base.size() != 4) return false;
	return ((base.compare(0, 3, "com") == 0 || base.compare(0, 3, "lpt") == 0) &&
		base[3] >= '1' && base[3] <= '9');
}

bool IsPortableLogicalPath(const std::string& path, std::string& normalized)
{
	if (path.size() > MaximumLogicalPathLength ||
		path.find('\\') != std::string::npos ||
		!NormalizeAssetPath(path, normalized)) return false;

	std::string component;
	for (std::size_t index = 0; index <= path.size(); ++index)
	{
		const unsigned char byte = index == path.size() ? '/' :
			static_cast<unsigned char>(path[index]);
		if (byte != '/' && byte != '\\')
		{
			if (byte == 127 || byte == '<' || byte == '>' || byte == '"' ||
				byte == '|' || byte == '?' || byte == '*') return false;
			component.push_back(static_cast<char>(byte));
			continue;
		}
		if (component.empty() || component == "." || component == ".." ||
			component.size() > 255 || component.back() == '.' || component.back() == ' ' ||
			IsWindowsDeviceName(component)) return false;
		component.clear();
	}
	return true;
}

bool IsWithin(const std::filesystem::path& parent, const std::filesystem::path& child)
{
	auto parentPart = parent.begin();
	auto childPart = child.begin();
	for (; parentPart != parent.end(); ++parentPart, ++childPart)
	{
		if (childPart == child.end() || *parentPart != *childPart) return false;
	}
	return true;
}

std::string PathText(const std::filesystem::path& path)
{
	return path.generic_u8string();
}

PackageHostResult Failure(PackageHostError error, std::string message,
	const std::filesystem::path& path = {}, const std::string& packageId = {})
{
	PackageHostResult result;
	result.error = error;
	result.path = path;
	result.packageId = packageId;
	result.message = std::move(message);
	return result;
}

class DirectoryAssetSource final : public AssetSource
{
public:
	static std::unique_ptr<DirectoryAssetSource> create(
		const std::filesystem::path& root, const std::string& provenance,
		std::size_t remainingTotalFiles, PackageHostResult& error)
	{
		std::error_code filesystemError;
		const std::filesystem::file_status rootStatus =
			std::filesystem::symlink_status(root, filesystemError);
		if (filesystemError || std::filesystem::is_symlink(rootStatus) ||
			!std::filesystem::is_directory(rootStatus))
		{
			error = Failure(PackageHostError::AssetIndexFailed,
				"asset root is not a real, readable directory", root, provenance);
			return nullptr;
		}

		std::unique_ptr<DirectoryAssetSource> source(
			new DirectoryAssetSource(root, provenance));
		struct Candidate
		{
			std::string normalized;
			std::string relative;
			std::filesystem::path actual;
		};
		std::vector<Candidate> candidates;
		std::filesystem::recursive_directory_iterator iterator(
			root, std::filesystem::directory_options::none, filesystemError);
		const std::filesystem::recursive_directory_iterator end;
		while (!filesystemError && iterator != end)
		{
			const std::filesystem::directory_entry& entry = *iterator;
			const std::filesystem::file_status status = entry.symlink_status(filesystemError);
			if (filesystemError) break;
			if (std::filesystem::is_symlink(status))
			{
				error = Failure(PackageHostError::AssetIndexFailed,
					"symbolic links are not allowed in package assets", entry.path(), provenance);
				return nullptr;
			}
			if (std::filesystem::is_directory(status))
			{
				iterator.increment(filesystemError);
				continue;
			}
			if (!std::filesystem::is_regular_file(status))
			{
				error = Failure(PackageHostError::AssetIndexFailed,
					"package assets must contain regular files only", entry.path(), provenance);
				return nullptr;
			}
			if (candidates.size() >= MaximumIndexedFiles)
			{
				error = Failure(PackageHostError::AssetIndexFailed,
					"package contains too many files", root, provenance);
				return nullptr;
			}
			if (candidates.size() >= remainingTotalFiles)
			{
				error = Failure(PackageHostError::AssetIndexFailed,
					"all packages contain more than 1,000,000 indexed files",
					root, provenance);
				return nullptr;
			}
			const std::filesystem::path relativePath = entry.path().lexically_relative(root);
			const std::string relative = relativePath.generic_u8string();
			std::string normalized;
			if (!IsPortableLogicalPath(relative, normalized))
			{
				error = Failure(PackageHostError::AssetIndexFailed,
					"package contains a non-portable logical path: " + relative,
					entry.path(), provenance);
				return nullptr;
			}
			candidates.push_back(Candidate{std::move(normalized), relative, entry.path()});
			iterator.increment(filesystemError);
		}
		if (filesystemError)
		{
			error = Failure(PackageHostError::AssetIndexFailed,
				"failed while enumerating package assets: " + filesystemError.message(),
				root, provenance);
			return nullptr;
		}

		std::sort(candidates.begin(), candidates.end(),
			[](const Candidate& left, const Candidate& right)
			{
				return left.relative < right.relative;
			});
		for (Candidate& candidate : candidates)
		{
			const auto inserted = source->files_.emplace(
				candidate.normalized, std::move(candidate.actual));
			if (!inserted.second)
			{
				error = Failure(PackageHostError::AssetIndexFailed,
					"case-insensitive asset collision: " + candidate.relative,
					root, provenance);
				return nullptr;
			}
		}
		return source;
	}

	std::size_t fileCount() const { return files_.size(); }

protected:
	bool existsNormalized(const std::string& logicalPath) const override
	{
		return files_.find(logicalPath) != files_.end();
	}

	AssetReadResult readNormalized(const std::string& logicalPath, AssetData& asset,
		std::size_t maximumBytes) const override
	{
		const auto found = files_.find(logicalPath);
		if (found == files_.end()) return AssetReadResult::NotFound;
		try
		{
			std::ifstream input(found->second, std::ios::binary | std::ios::ate);
			if (!input) return AssetReadResult::IoError;
			const std::ifstream::pos_type end = input.tellg();
			if (end < 0) return AssetReadResult::IoError;
			const std::uintmax_t size = static_cast<std::uintmax_t>(end);
			if (size > maximumBytes ||
				size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
				size > static_cast<std::uintmax_t>(
					std::numeric_limits<std::streamsize>::max()))
				return AssetReadResult::TooLarge;
			std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
			input.seekg(0, std::ios::beg);
			if (!bytes.empty())
			{
				input.read(reinterpret_cast<char*>(bytes.data()),
					static_cast<std::streamsize>(bytes.size()));
				if (!input || static_cast<std::size_t>(input.gcount()) != bytes.size())
					return AssetReadResult::IoError;
			}
			if (input.peek() != std::ifstream::traits_type::eof())
				return AssetReadResult::IoError;
			asset.provenance = provenance_;
			asset.bytes = std::move(bytes);
			return AssetReadResult::Success;
		}
		catch (...)
		{
			return AssetReadResult::IoError;
		}
	}

private:
	DirectoryAssetSource(std::filesystem::path root, std::string provenance)
		: root_(std::move(root)), provenance_(std::move(provenance)) {}

	std::filesystem::path root_;
	std::string provenance_;
	std::unordered_map<std::string, std::filesystem::path> files_;
};

bool ReadManifestString(vfs::PropertyContainer& properties, const wchar_t* key,
	std::string& value)
{
	vfs::String text;
	if (!properties.getStringProperty(L"Package", key, text)) return false;
	value = TrimAscii(text.utf8());
	return true;
}

bool ReadRequirementList(vfs::PropertyContainer& properties, const wchar_t* key,
	const char* keyName, const std::string& packageId,
	std::unordered_set<std::string>& relationshipIds,
	std::vector<ContentRequirement>& requirements,
	const std::filesystem::path& manifestPath, PackageHostResult& error)
{
	std::list<vfs::String> values;
	if (!properties.getStringListProperty(L"Package", key, values, L"")) return true;
	if (values.size() > MaximumRequirements ||
		relationshipIds.size() + values.size() > MaximumRequirements)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"package declares too many dependency-policy relationships", manifestPath, packageId);
		return false;
	}
	for (const vfs::String& value : values)
	{
		const std::string text = TrimAscii(value.utf8());
		const std::size_t at = text.find('@');
		if (text.empty() || (at != std::string::npos &&
			(at + 1 == text.size() || text.find('@', at + 1) != std::string::npos)))
		{
			error = Failure(PackageHostError::InvalidManifest,
				"invalid " + std::string(keyName) + " entry: " + text,
				manifestPath, packageId);
			return false;
		}
		const std::string dependencyId = text.substr(0, at);
		std::string exactVersion = at == std::string::npos ? "" : text.substr(at + 1);
		if (exactVersion == "*") exactVersion.clear();
		if (!IsLowercaseIdentifier(dependencyId) || dependencyId == packageId ||
			!relationshipIds.insert(dependencyId).second ||
			(!exactVersion.empty() && !IsPortableVersion(exactVersion)))
		{
			error = Failure(PackageHostError::InvalidManifest,
				"invalid, duplicate, or self-referential " + std::string(keyName) +
				" entry: " + text, manifestPath, packageId);
			return false;
		}
		requirements.push_back(ContentRequirement{dependencyId, exactVersion});
	}
	return true;
}

bool ReadRelationshipList(vfs::PropertyContainer& properties, const wchar_t* key,
	const char* keyName, const std::string& packageId,
	std::unordered_set<std::string>& relationshipIds,
	std::vector<std::string>& relationships,
	const std::filesystem::path& manifestPath, PackageHostResult& error)
{
	std::list<vfs::String> values;
	if (!properties.getStringListProperty(L"Package", key, values, L"")) return true;
	if (values.size() > MaximumRequirements ||
		relationshipIds.size() + values.size() > MaximumRequirements)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"package declares too many dependency-policy relationships", manifestPath, packageId);
		return false;
	}
	for (const vfs::String& value : values)
	{
		const std::string relationshipId = TrimAscii(value.utf8());
		if (!IsLowercaseIdentifier(relationshipId) || relationshipId == packageId ||
			!relationshipIds.insert(relationshipId).second)
		{
			error = Failure(PackageHostError::InvalidManifest,
				"invalid, duplicate, or self-referential " + std::string(keyName) +
				" entry: " + relationshipId, manifestPath, packageId);
			return false;
		}
		relationships.push_back(relationshipId);
	}
	return true;
}

bool ReadCapabilityList(vfs::PropertyContainer& properties,
	const wchar_t* key, const char* keyName, std::vector<std::string>& capabilities,
	const std::filesystem::path& manifestPath, const std::string& packageId,
	PackageHostResult& error)
{
	std::list<vfs::String> values;
	if (!properties.getStringListProperty(L"Package", key, values, L"")) return true;
	if (values.size() > MaximumRequirements)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"package declares too many " + std::string(keyName) + " entries",
			manifestPath, packageId);
		return false;
	}
	std::unordered_set<std::string> unique;
	unique.reserve(values.size());
	for (const vfs::String& value : values)
	{
		const std::string capability = TrimAscii(value.utf8());
		if (!IsLowercaseIdentifier(capability) || !unique.insert(capability).second)
		{
			error = Failure(PackageHostError::InvalidManifest,
				"invalid or duplicate " + std::string(keyName) + " entry: " + capability,
				manifestPath, packageId);
			return false;
		}
		capabilities.push_back(capability);
	}
	return true;
}

bool ReadLocalizationSourceList(vfs::PropertyContainer& properties,
	std::vector<PackageLocalizationSource>& sources,
	const std::filesystem::path& manifestPath, const std::string& packageId,
	PackageHostResult& error)
{
	std::list<vfs::String> values;
	if (!properties.getStringListProperty(L"Package", L"LOCALIZATION", values, L""))
		return true;
	if (values.size() > MaximumDeclaredContentSources)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"package declares too many LOCALIZATION sources", manifestPath, packageId);
		return false;
	}
	std::unordered_set<std::string> unique;
	unique.reserve(values.size());
	for (const vfs::String& value : values)
	{
		const std::string text = TrimAscii(value.utf8());
		const std::size_t at = text.find('@');
		if (at == std::string::npos || at == 0 || at + 1 == text.size() ||
			text.find('@', at + 1) != std::string::npos)
		{
			error = Failure(PackageHostError::InvalidManifest,
				"LOCALIZATION entries require locale@asset/path: " + text,
				manifestPath, packageId);
			return false;
		}
		const std::string locale = LowerAscii(TrimAscii(text.substr(0, at)));
		const std::string assetPath = TrimAscii(text.substr(at + 1));
		std::string normalized;
		if (!IsLowercaseIdentifier(locale) || !IsPortableLogicalPath(assetPath, normalized) ||
			!unique.insert(locale + "\n" + normalized).second)
		{
			error = Failure(PackageHostError::InvalidManifest,
				"invalid or duplicate LOCALIZATION source: " + text,
				manifestPath, packageId);
			return false;
		}
		sources.push_back(PackageLocalizationSource{locale, assetPath});
	}
	return true;
}

bool ReadDefinitionSourceList(vfs::PropertyContainer& properties,
	std::vector<PackageDefinitionSource>& sources,
	const std::filesystem::path& manifestPath, const std::string& packageId,
	PackageHostResult& error)
{
	std::list<vfs::String> values;
	if (!properties.getStringListProperty(L"Package", L"DEFINITIONS", values, L""))
		return true;
	if (values.size() > MaximumDeclaredContentSources)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"package declares too many DEFINITION sources", manifestPath, packageId);
		return false;
	}
	std::unordered_set<std::string> unique;
	unique.reserve(values.size());
	for (const vfs::String& value : values)
	{
		const std::string text = TrimAscii(value.utf8());
		const std::size_t colon = text.find(':');
		const std::size_t at = text.find('@', colon == std::string::npos ? 0 : colon + 1);
		const std::size_t equals = text.find('=', at == std::string::npos ? 0 : at + 1);
		if (colon == std::string::npos || at == std::string::npos ||
			equals == std::string::npos || colon == 0 || at <= colon + 1 ||
			equals <= at + 1 || equals + 1 == text.size() ||
			text.find(':', colon + 1) != std::string::npos ||
			text.find('@', at + 1) != std::string::npos ||
			text.find('=', equals + 1) != std::string::npos)
		{
			error = Failure(PackageHostError::InvalidManifest,
				"DEFINITIONS entries require type:id@schema=asset/path: " + text,
				manifestPath, packageId);
			return false;
		}
		const std::string type = LowerAscii(TrimAscii(text.substr(0, colon)));
		const std::string definitionId =
			LowerAscii(TrimAscii(text.substr(colon + 1, at - colon - 1)));
		const std::string schemaText = TrimAscii(text.substr(at + 1, equals - at - 1));
		const std::string assetPath = TrimAscii(text.substr(equals + 1));
		std::uint32_t schemaVersion = 0;
		std::string normalized;
		if (!IsLowercaseIdentifier(type) || !IsLowercaseIdentifier(definitionId) ||
			!ParseUnsigned32(schemaText, schemaVersion) ||
			!IsPortableLogicalPath(assetPath, normalized) ||
			!unique.insert(type + "\n" + definitionId).second)
		{
			error = Failure(PackageHostError::InvalidManifest,
				"invalid or duplicate DEFINITIONS source: " + text,
				manifestPath, packageId);
			return false;
		}
		sources.push_back(PackageDefinitionSource{
			type, definitionId, schemaVersion, assetPath});
	}
	return true;
}

PackageKind ParsePackageKind(const std::string& text, bool& valid)
{
	const std::string kind = LowerAscii(text);
	valid = true;
	if (kind == "rules") return PackageKind::Rules;
	if (kind == "extension") return PackageKind::Extension;
	if (kind == "tool") return PackageKind::Tool;
	valid = false;
	return PackageKind::Extension;
}

class VfsPackageAssetMounter final : public PackageAssetMounter
{
public:
	bool preflight(const std::string& packageId,
		const std::filesystem::path& assetRoot, std::string& error) const override
	{
		const std::string profileName = profileNameFor(packageId);
		if (getVFS()->getProfileStack()->getProfile(vfs::String(profileName.c_str())))
		{
			error = "bfVFS profile already exists: " + profileName;
			return false;
		}
		std::error_code filesystemError;
		if (!std::filesystem::is_directory(assetRoot, filesystemError) || filesystemError)
		{
			error = "asset root is no longer a readable directory";
			return false;
		}
		return true;
	}

	bool mount(const std::string& packageId, const std::filesystem::path& assetRoot,
		std::string& error) override
	{
		try
		{
			vfs_init::VfsConfig config;
			vfs_init::Profile* profile = new vfs_init::Profile();
			profile->m_name = vfs::String(profileNameFor(packageId).c_str());
			profile->m_root = vfs::Path(assetRoot.generic_u8string());
			profile->m_writable = false;
			vfs_init::Location* location = new vfs_init::Location();
			location->m_optional = false;
			location->m_type = L"DIRECTORY";
			location->m_path = vfs::Path();
			location->m_mount_point = vfs::Path();
			profile->addLocation(location, true);
			config.addProfile(profile, true);
			// Late package mounts must consult the profile stack instead of
			// forcibly replacing a higher-priority writable file.
			if (!vfs_init::initVirtualFileSystem(config, false))
			{
				error = "bfVFS rejected the package profile";
				return false;
			}
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
		catch (...)
		{
			error = "unknown bfVFS mount failure";
			return false;
		}
	}

	bool unmount(const std::string& packageId, std::string& error) override
	{
		try
		{
			const std::string profileName = profileNameFor(packageId);
			vfs::CProfileStack* profiles = getVFS()->getProfileStack();
			if (!profiles->getProfile(vfs::String(profileName.c_str())))
				return true;
			if (!profiles->removeProfile(vfs::String(profileName.c_str())))
			{
				error = "bfVFS could not remove package profile: " + profileName;
				return false;
			}
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
		catch (...)
		{
			error = "unknown bfVFS unmount failure";
			return false;
		}
	}

private:
	static std::string profileNameFor(const std::string& packageId)
	{
		return "package." + packageId;
	}
};

std::string JoinIds(const std::vector<std::string>& ids)
{
	std::ostringstream output;
	for (std::size_t index = 0; index < ids.size(); ++index)
	{
		if (index != 0) output << ", ";
		output << ids[index];
	}
	return output.str();
}
}

struct PackageHost::OwnedPackage final : EnginePackage
{
	PackageDescriptor descriptor_;
	std::filesystem::path manifestPath;
	std::filesystem::path assetRoot;
	std::unique_ptr<DirectoryAssetSource> assets;
	bool active = false;

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override
	{
		if (active) return false;
		active = true;
		return true;
	}
	void deactivate() noexcept override { active = false; }
	const AssetSource* assetSource() const noexcept override { return assets.get(); }
	bool bootstrap(PackageBootstrapContext& context, PackageBootstrapPhase phase) override
	{
		if (phase != PackageBootstrapPhase::LoadContent) return true;
		if (!assets) return false;
		const PackageContentLoadResult loaded = LoadDeclaredPackageContent(
			descriptor_, *assets, context.localization, context.definitions);
		if (loaded) return true;
		std::string detail = "content import failed with code " +
			std::to_string(static_cast<int>(loaded.error)) +
			" (detail " + std::to_string(loaded.detail) + ")";
		if (loaded.line != 0) detail += " at line " + std::to_string(loaded.line);
		logContentFailure(context, loaded.assetPath, detail);
		return false;
	}
	void shutdown(PackageBootstrapContext& context, PackageBootstrapPhase phase) override
	{
		if (phase == PackageBootstrapPhase::LoadContent)
			UnloadDeclaredPackageContent(context.localization, context.definitions);
	}

private:
	void logContentFailure(PackageBootstrapContext& context,
		const std::string& source, const std::string& detail) const noexcept
	{
		try
		{
			context.services.log.write(LogRecord{LogSeverity::Error, "packages",
				"Package " + descriptor_.content.id + " could not load " + source +
					" declared by " + PathText(manifestPath) + ": " + detail});
		}
		catch (...)
		{
			// A diagnostic sink cannot alter content bootstrap rollback.
		}
	}
};

std::unique_ptr<PackageHost::OwnedPackage> PackageHost::readPackageManifest(
	const std::filesystem::path& packageDirectory,
	const std::filesystem::path& manifestPath, std::size_t remainingTotalFiles,
	PackageHostResult& error)
{
	std::error_code filesystemError;
	const std::uintmax_t manifestBytes = std::filesystem::file_size(manifestPath, filesystemError);
	if (filesystemError)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"cannot read package manifest: " + filesystemError.message(), manifestPath);
		return nullptr;
	}
	if (manifestBytes > MaximumManifestBytes)
	{
		error = Failure(PackageHostError::ManifestTooLarge,
			"package manifest exceeds 64 KiB", manifestPath);
		return nullptr;
	}

	vfs::PropertyContainer properties;
	try
	{
		if (!properties.initFromIniFile(vfs::Path(manifestPath.generic_u8string())))
		{
			error = Failure(PackageHostError::InvalidManifest,
				"cannot parse package manifest", manifestPath);
			return nullptr;
		}
	}
	catch (const std::exception& exception)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"cannot parse package manifest: " + std::string(exception.what()), manifestPath);
		return nullptr;
	}

	std::string schemaText;
	std::string id;
	std::string version;
	std::string apiText;
	std::string kindText;
	std::string assetRootText;
	if (!ReadManifestString(properties, L"MANIFEST_VERSION", schemaText) ||
		(schemaText != "1" && schemaText != "2" && schemaText != "3") ||
		!ReadManifestString(properties, L"ID", id) ||
		!ReadManifestString(properties, L"VERSION", version) ||
		!ReadManifestString(properties, L"CONTENT_API", apiText) ||
		!ReadManifestString(properties, L"TYPE", kindText) ||
		!ReadManifestString(properties, L"ASSET_ROOT", assetRootText))
	{
		error = Failure(PackageHostError::InvalidManifest,
			"manifest requires MANIFEST_VERSION=1, 2, or 3, ID, VERSION, CONTENT_API, TYPE, and ASSET_ROOT",
			manifestPath, id);
		return nullptr;
	}
	if (!IsLowercaseIdentifier(id))
	{
		error = Failure(PackageHostError::InvalidManifest,
			"package ID must be a lowercase portable engine identifier", manifestPath, id);
		return nullptr;
	}
	if (!IsPortableVersion(version))
	{
		error = Failure(PackageHostError::InvalidManifest,
			"package VERSION is empty, too long, or non-portable", manifestPath, id);
		return nullptr;
	}
	ContentApiVersion api{};
	if (!ParseContentApiVersion(apiText, api) || api.major != CurrentContentApiVersion.major ||
		api.minor < 1 || api.minor > CurrentContentApiVersion.minor)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"data packages require a supported 1.1 or newer CONTENT_API", manifestPath, id);
		return nullptr;
	}
	bool kindValid = false;
	const PackageKind kind = ParsePackageKind(kindText, kindValid);
	if (!kindValid)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"data package TYPE must be rules, extension, or tool", manifestPath, id);
		return nullptr;
	}

	std::string normalizedAssetRoot;
	if (!IsPortableLogicalPath(assetRootText, normalizedAssetRoot))
	{
		error = Failure(PackageHostError::InvalidManifest,
			"ASSET_ROOT must be a portable relative directory", manifestPath, id);
		return nullptr;
	}
	const std::filesystem::path canonicalPackage =
		std::filesystem::canonical(packageDirectory, filesystemError);
	if (filesystemError)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"cannot resolve package directory", packageDirectory, id);
		return nullptr;
	}
	const std::filesystem::path relativeAssetRoot =
		std::filesystem::u8path(assetRootText);
	std::filesystem::path requestedAssetRoot = packageDirectory;
	for (const std::filesystem::path& component : relativeAssetRoot)
	{
		requestedAssetRoot /= component;
		const std::filesystem::file_status componentStatus =
			std::filesystem::symlink_status(requestedAssetRoot, filesystemError);
		if (filesystemError || std::filesystem::is_symlink(componentStatus) ||
			!std::filesystem::is_directory(componentStatus))
		{
			error = Failure(PackageHostError::InvalidManifest,
				"ASSET_ROOT components must be real directories without symbolic links",
				requestedAssetRoot, id);
			return nullptr;
		}
	}
	const std::filesystem::path canonicalAssetRoot =
		std::filesystem::canonical(requestedAssetRoot, filesystemError);
	if (filesystemError || !IsWithin(canonicalPackage, canonicalAssetRoot))
	{
		error = Failure(PackageHostError::InvalidManifest,
			"ASSET_ROOT does not resolve to a directory inside its package",
			requestedAssetRoot, id);
		return nullptr;
	}

	std::vector<ContentRequirement> requirements;
	std::vector<ContentRequirement> optionalRequirements;
	std::vector<std::string> conflicts;
	std::vector<std::string> loadAfter;
	std::vector<std::string> capabilities;
	std::vector<std::string> requiredCapabilities;
	std::vector<PackageLocalizationSource> localizationSources;
	std::vector<PackageDefinitionSource> definitionSources;
	std::unordered_set<std::string> relationshipIds;
	if (!ReadRequirementList(properties, L"REQUIRES", "REQUIRES", id,
			relationshipIds, requirements, manifestPath, error) ||
		!ReadRequirementList(properties, L"OPTIONAL_REQUIRES", "OPTIONAL_REQUIRES", id,
			relationshipIds, optionalRequirements, manifestPath, error) ||
		!ReadRelationshipList(properties, L"CONFLICTS", "CONFLICTS", id,
			relationshipIds, conflicts, manifestPath, error) ||
		!ReadRelationshipList(properties, L"LOAD_AFTER", "LOAD_AFTER", id,
			relationshipIds, loadAfter, manifestPath, error) ||
		!ReadCapabilityList(properties, L"CAPABILITIES", "CAPABILITIES",
			capabilities, manifestPath, id, error) ||
		!ReadCapabilityList(properties, L"REQUIRED_CAPABILITIES", "REQUIRED_CAPABILITIES",
			requiredCapabilities, manifestPath, id, error) ||
		!ReadLocalizationSourceList(properties, localizationSources,
			manifestPath, id, error) ||
		!ReadDefinitionSourceList(properties, definitionSources,
			manifestPath, id, error))
		return nullptr;
	if (localizationSources.size() + definitionSources.size() >
		MaximumDeclaredContentSources)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"package declares too many combined content sources", manifestPath, id);
		return nullptr;
	}
	if (!requirements.empty() && api.minor < PackageRequirementsContentApiVersion.minor)
	{
		error = Failure(PackageHostError::InvalidManifest,
			"REQUIRES needs CONTENT_API 1.2 or newer", manifestPath, id);
		return nullptr;
	}
	const bool hasPolicy = !optionalRequirements.empty() || !conflicts.empty() ||
		!loadAfter.empty() || !capabilities.empty() || !requiredCapabilities.empty();
	if ((schemaText == "1" && hasPolicy) ||
		((schemaText == "2" || schemaText == "3") &&
			(api.major != PackagePolicyContentApiVersion.major ||
			api.minor < PackagePolicyContentApiVersion.minor)))
	{
		error = Failure(PackageHostError::InvalidManifest,
			"dependency policy needs MANIFEST_VERSION=2 or newer and CONTENT_API 1.3 or newer",
			manifestPath, id);
		return nullptr;
	}
	if (((!localizationSources.empty() || !definitionSources.empty()) && schemaText != "3") ||
		(schemaText == "3" &&
			(api.major != PackageDeclaredContentApiVersion.major ||
			 api.minor < PackageDeclaredContentApiVersion.minor)))
	{
		error = Failure(PackageHostError::InvalidManifest,
			"Data Package v3 declared content needs MANIFEST_VERSION=3 and CONTENT_API 1.4 or newer",
			manifestPath, id);
		return nullptr;
	}

	std::unique_ptr<DirectoryAssetSource> assets =
		DirectoryAssetSource::create(
			canonicalAssetRoot, id, remainingTotalFiles, error);
	if (!assets) return nullptr;

	std::unique_ptr<PackageHost::OwnedPackage> package(new PackageHost::OwnedPackage());
	package->descriptor_ = PackageDescriptor{
		ContentManifest{id, version, api, std::move(requirements),
			std::move(optionalRequirements), std::move(conflicts), std::move(loadAfter)}, kind,
		std::move(capabilities), {}, {}, std::move(requiredCapabilities),
		std::move(localizationSources), std::move(definitionSources)};
	package->manifestPath = manifestPath;
	package->assetRoot = canonicalAssetRoot;
	package->assets = std::move(assets);
	return package;
}

PackageStartupOptions ReadPackageStartupOptions(
	vfs::PropertyContainer& properties, int argc, char* const* argv)
{
	PackageStartupOptions options;
	std::list<vfs::String> values;
	if (properties.getStringListProperty(
		L"Ja2 Settings", L"PACKAGE_ROOTS", values, L""))
	{
		options.enabled = true;
		for (const vfs::String& value : values)
			options.roots.push_back(std::filesystem::u8path(TrimAscii(value.utf8())));
	}
	values.clear();
	if (properties.getStringListProperty(
		L"Ja2 Settings", L"PACKAGE_SELECTION", values, L""))
	{
		options.enabled = true;
		for (const vfs::String& value : values)
			options.selected.push_back(TrimAscii(value.utf8()));
	}

	bool commandLineSelection = false;
	bool commandLineRoots = false;
	for (int index = 1; index < argc; ++index)
	{
		if (!argv || !argv[index]) continue;
		std::string option = argv[index];
		std::size_t prefix = 0;
		if (option.compare(0, 2, "--") == 0) prefix = 2;
		else if (!option.empty() && (option[0] == '-' || option[0] == '/')) prefix = 1;
		else continue;
		option.erase(0, prefix);
		const std::size_t separator = option.find_first_of("=:");
		const std::string key = LowerAscii(option.substr(0, separator));
		if (key != "package" && key != "package-root") continue;
		std::string value = separator == std::string::npos ? "" : option.substr(separator + 1);
		if (value.empty() && index + 1 < argc &&
			IsSeparateOptionValue(argv[index + 1]))
			value = argv[++index];
		options.enabled = true;
		if (key == "package")
		{
			if (!commandLineSelection)
			{
				options.selected.clear();
				commandLineSelection = true;
			}
			const std::vector<std::string> selected = SplitCommaList(value);
			options.selected.insert(options.selected.end(), selected.begin(), selected.end());
		}
		else
		{
			if (!commandLineRoots)
			{
				options.roots.clear();
				commandLineRoots = true;
			}
			for (const std::string& root : SplitCommaList(value))
				options.roots.push_back(std::filesystem::u8path(root));
		}
	}
	if (options.enabled && !options.selected.empty() && options.roots.empty())
		options.roots.push_back(std::filesystem::path("Packages"));
	return options;
}

PackageHost::PackageHost() = default;
PackageHost::~PackageHost() = default;

PackageHostResult PackageHost::initialize(PackageRegistry& registry,
	const PackageStartupOptions& options, PackageAssetMounter& mounter)
{
	if (!options.enabled) return PackageHostResult{};
	if (attempted_)
		return Failure(PackageHostError::AlreadyInitialized,
			"startup package host has already been initialized");
	attempted_ = true;
	struct AttemptGuard
	{
		bool& attempted;
		std::vector<std::unique_ptr<OwnedPackage>>& packages;
		std::vector<std::string>& discovered;
		std::vector<std::string>& registered;
		std::vector<std::string>& activated;
		std::vector<std::string>& mounted;
		bool retain = false;

		~AttemptGuard()
		{
			if (retain) return;
			mounted.clear();
			activated.clear();
			registered.clear();
			discovered.clear();
			packages.clear();
			attempted = false;
		}
	} attempt{attempted_, packages_, discoveredIds_, registeredIds_,
		activatedIds_, mountedIds_};
	if (options.roots.size() > MaximumPackageRoots || options.selected.size() > MaximumPackages)
		return Failure(PackageHostError::InvalidOptions,
			"too many package roots or selected packages");

	std::unordered_set<std::string> selectedIds;
	for (const std::string& id : options.selected)
	{
		if (!IsLowercaseIdentifier(id) || !selectedIds.insert(id).second)
			return Failure(PackageHostError::InvalidOptions,
				"selected package IDs must be unique lowercase identifiers", {}, id);
	}

	std::vector<std::unique_ptr<OwnedPackage>> discoveredPackages;
	std::unordered_set<std::string> discoveredIds;
	std::vector<std::filesystem::path> canonicalRoots;
	std::size_t indexedFiles = 0;
	for (const std::filesystem::path& configuredRoot : options.roots)
	{
		if (configuredRoot.empty())
			return Failure(PackageHostError::InvalidOptions, "package root cannot be empty");
		std::error_code filesystemError;
		const std::filesystem::file_status rootStatus =
			std::filesystem::symlink_status(configuredRoot, filesystemError);
		if (filesystemError || std::filesystem::is_symlink(rootStatus) ||
			!std::filesystem::is_directory(rootStatus))
			return Failure(PackageHostError::RootNotFound,
				"package root is not a real, readable directory", configuredRoot);
		const std::filesystem::path root =
			std::filesystem::canonical(configuredRoot, filesystemError);
		if (filesystemError)
			return Failure(PackageHostError::RootNotFound,
				"cannot resolve package root: " + filesystemError.message(), configuredRoot);
		for (const std::filesystem::path& existingRoot : canonicalRoots)
		{
			std::error_code equivalentError;
			if (root == existingRoot ||
				std::filesystem::equivalent(root, existingRoot, equivalentError))
				return Failure(PackageHostError::InvalidOptions,
					"duplicate package root", configuredRoot);
			if (equivalentError)
				return Failure(PackageHostError::InvalidOptions,
					"cannot compare package roots: " + equivalentError.message(),
					configuredRoot);
		}
		canonicalRoots.push_back(root);

		std::vector<std::filesystem::path> packageDirectories;
		std::filesystem::directory_iterator iterator(root,
			std::filesystem::directory_options::none, filesystemError);
		const std::filesystem::directory_iterator end;
		while (!filesystemError && iterator != end)
		{
			const std::filesystem::directory_entry& entry = *iterator;
			const std::filesystem::file_status status = entry.symlink_status(filesystemError);
			if (filesystemError) break;
			if (std::filesystem::is_symlink(status))
				return Failure(PackageHostError::DiscoveryFailed,
					"symbolic links are not allowed in package roots", entry.path());
			if (std::filesystem::is_directory(status)) packageDirectories.push_back(entry.path());
			iterator.increment(filesystemError);
		}
		if (filesystemError)
			return Failure(PackageHostError::DiscoveryFailed,
				"failed while scanning package root: " + filesystemError.message(), root);
		std::sort(packageDirectories.begin(), packageDirectories.end(),
			[](const std::filesystem::path& left, const std::filesystem::path& right)
			{
				return PathText(left) < PathText(right);
			});

		for (const std::filesystem::path& packageDirectory : packageDirectories)
		{
			const std::filesystem::path manifest = packageDirectory / "package.ini";
			const std::filesystem::file_status manifestStatus =
				std::filesystem::symlink_status(manifest, filesystemError);
			if (filesystemError)
			{
				const bool missing =
					filesystemError == std::errc::no_such_file_or_directory ||
					filesystemError == std::errc::not_a_directory;
				if (missing)
				{
					filesystemError.clear();
					continue;
				}
				return Failure(PackageHostError::DiscoveryFailed,
					"cannot inspect package manifest: " + filesystemError.message(),
					manifest);
			}
			if (!std::filesystem::exists(manifestStatus)) continue;
			if (std::filesystem::is_symlink(manifestStatus) ||
				!std::filesystem::is_regular_file(manifestStatus))
				return Failure(PackageHostError::InvalidManifest,
					"package.ini must be a regular file", manifest);
			if (discoveredPackages.size() >= MaximumPackages)
				return Failure(PackageHostError::DiscoveryFailed,
					"too many packages were discovered", root);
			PackageHostResult manifestError;
			std::unique_ptr<OwnedPackage> package =
				readPackageManifest(packageDirectory, manifest,
					MaximumTotalIndexedFiles - indexedFiles, manifestError);
			if (!package) return manifestError;
			indexedFiles += package->assets->fileCount();
			const std::string& id = package->descriptor_.content.id;
			if (!discoveredIds.insert(id).second)
				return Failure(PackageHostError::DuplicateId,
					"duplicate package ID", manifest, id);
			discoveredPackages.push_back(std::move(package));
		}
	}

	packages_ = std::move(discoveredPackages);
	discoveredIds_.reserve(packages_.size());
	std::unordered_map<std::string, OwnedPackage*> packagesById;
	packagesById.reserve(packages_.size());
	for (const std::unique_ptr<OwnedPackage>& package : packages_)
	{
		const std::string& id = package->descriptor_.content.id;
		discoveredIds_.push_back(id);
		packagesById.emplace(id, package.get());
		if (registry.find(id))
			return Failure(PackageHostError::DuplicateId,
				"package ID is already registered", package->manifestPath, id);
	}

	std::vector<std::string> registeredIds;
	registeredIds.reserve(packages_.size());
	std::vector<std::string> mountedIds;
	mountedIds.reserve(packages_.size());
	auto rollbackStartup = [&](PackageHostResult& failed,
		const std::vector<std::string>& activated)
	{
		for (auto mounted = mountedIds.rbegin(); mounted != mountedIds.rend(); ++mounted)
		{
			std::string unmountError;
			bool unmounted = false;
			try
			{
				unmounted = mounter.unmount(*mounted, unmountError);
			}
			catch (const std::exception& exception)
			{
				unmountError = exception.what();
			}
			catch (...)
			{
				unmountError = "unknown unmount exception";
			}
			if (!unmounted)
				failed.rollbackFailures.push_back(
					"unmount " + *mounted + ": " + unmountError);
		}
		for (auto active = activated.rbegin(); active != activated.rend(); ++active)
		{
			const PackageDeactivationResult deactivation =
				registry.deactivateDetailed(*active);
			if (!deactivation)
				failed.rollbackFailures.push_back(
					"deactivate " + *active + ": code " +
					std::to_string(static_cast<int>(deactivation.error)));
		}
		if (!registeredIds.empty())
		{
			const PackageUnregistrationBatchResult unregistration =
				registry.unregisterPackages(registeredIds);
			if (!unregistration)
				failed.rollbackFailures.push_back(
					"unregister " + unregistration.packageId + ": code " +
					std::to_string(static_cast<int>(unregistration.error)));
		}
		if (!failed.rollbackFailures.empty())
		{
			failed.message += "; rollback incomplete: " + JoinIds(failed.rollbackFailures);
			// Retain stable package/source ownership and enough attempted state
			// for shutdown() to retry every idempotent external boundary.
			registeredIds_ = registeredIds;
			activatedIds_ = activated;
			mountedIds_ = mountedIds;
			attempt.retain = true;
		}
	};
	for (const std::unique_ptr<OwnedPackage>& package : packages_)
	{
		PackageRegistrationError registration = PackageRegistrationError::None;
		try
		{
			registration = registry.registerPackage(*package);
		}
		catch (const std::exception& exception)
		{
			PackageHostResult failed = Failure(PackageHostError::RegistrationFailed,
				"engine package registration threw: " + std::string(exception.what()),
				package->manifestPath, package->descriptor_.content.id);
			rollbackStartup(failed, {});
			return failed;
		}
		catch (...)
		{
			PackageHostResult failed = Failure(PackageHostError::RegistrationFailed,
				"engine package registration threw an unknown exception",
				package->manifestPath, package->descriptor_.content.id);
			rollbackStartup(failed, {});
			return failed;
		}
		if (registration != PackageRegistrationError::None)
		{
			PackageHostResult failed = Failure(PackageHostError::RegistrationFailed,
				"engine rejected package registration with code " +
					std::to_string(static_cast<int>(registration)),
				package->manifestPath, package->descriptor_.content.id);
			rollbackStartup(failed, {});
			return failed;
		}
		registeredIds.push_back(package->descriptor_.content.id);
	}

	PackageHostResult result;
	result.discovered = discoveredIds_;
	if (options.selected.empty())
	{
		registeredIds_ = registeredIds;
		attempt.retain = true;
		return result;
	}
	PackageActivationPlan plan;
	try
	{
		plan = registry.resolveActivation(options.selected);
	}
	catch (const std::exception& exception)
	{
		result.error = PackageHostError::ResolutionFailed;
		result.message = "package dependency resolution threw: " +
			std::string(exception.what());
		rollbackStartup(result, {});
		return result;
	}
	catch (...)
	{
		result.error = PackageHostError::ResolutionFailed;
		result.message = "package dependency resolution threw an unknown exception";
		rollbackStartup(result, {});
		return result;
	}
	if (!plan)
	{
		result.error = PackageHostError::ResolutionFailed;
		result.packageId = plan.packageId;
		result.diagnosticPath = plan.diagnosticPath;
		result.message = "package dependency resolution failed with code " +
			std::to_string(static_cast<int>(plan.error));
		rollbackStartup(result, {});
		return result;
	}
	for (const std::string& id : plan.order)
	{
		const auto package = packagesById.find(id);
		if (package == packagesById.end()) continue;
		std::string mountError;
		bool readyToMount = false;
		try
		{
			readyToMount = mounter.preflight(id, package->second->assetRoot, mountError);
		}
		catch (const std::exception& exception)
		{
			mountError = exception.what();
		}
		catch (...)
		{
			mountError = "unknown mount preflight exception";
		}
		if (!readyToMount)
		{
			result.error = PackageHostError::MountPreflightFailed;
			result.packageId = id;
			result.path = package->second->assetRoot;
			result.message = std::move(mountError);
			rollbackStartup(result, {});
			return result;
		}
	}

	PackageActivationResult activation;
	try
	{
		activation = registry.activateAll(options.selected);
	}
	catch (const std::exception& exception)
	{
		result.error = PackageHostError::ActivationFailed;
		result.message = "package activation threw: " + std::string(exception.what());
		rollbackStartup(result, {});
		return result;
	}
	catch (...)
	{
		result.error = PackageHostError::ActivationFailed;
		result.message = "package activation threw an unknown exception";
		rollbackStartup(result, {});
		return result;
	}
	if (!activation)
	{
		result.error = PackageHostError::ActivationFailed;
		result.packageId = activation.packageId;
		result.diagnosticPath = std::move(activation.diagnosticPath);
		result.message = "package activation failed with code " +
			std::to_string(static_cast<int>(activation.error));
		rollbackStartup(result, {});
		return result;
	}
	for (const std::string& id : activation.activated)
	{
		const auto package = packagesById.find(id);
		if (package == packagesById.end()) continue;
		std::string mountError;
		bool mounted = false;
		// A mounter may acquire partial state before returning false or
		// throwing. Reserve made this non-allocating; rollback now owns every
		// attempted mount regardless of its reported result.
		mountedIds.push_back(id);
		try
		{
			mounted = mounter.mount(id, package->second->assetRoot, mountError);
		}
		catch (const std::exception& exception)
		{
			mountError = exception.what();
		}
		catch (...)
		{
			mountError = "unknown mount exception";
		}
		if (mounted)
		{
			continue;
		}
		result.error = PackageHostError::MountFailed;
		result.packageId = id;
		result.path = package->second->assetRoot;
		result.message = std::move(mountError);
		rollbackStartup(result, activation.activated);
		return result;
	}
	result.activated = std::move(activation.activated);
	registeredIds_ = registeredIds;
	activatedIds_ = result.activated;
	mountedIds_ = mountedIds;
	attempt.retain = true;
	return result;
}

PackageHostShutdownResult PackageHost::shutdown(
	PackageRegistry& registry, PackageAssetMounter& mounter)
{
	PackageHostShutdownResult result;
	if (!attempted_) return result;

	bool mountsComplete = true;
	for (auto mounted = mountedIds_.rbegin(); mounted != mountedIds_.rend(); ++mounted)
	{
		std::string error;
		bool removed = false;
		try
		{
			removed = mounter.unmount(*mounted, error);
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
		}
		catch (...)
		{
			error = "unknown unmount exception";
		}
		if (removed)
		{
			++result.unmounted;
			continue;
		}
		mountsComplete = false;
		result.failures.push_back("unmount " + *mounted + ": " + error);
	}
	if (mountsComplete) mountedIds_.clear();

	bool deactivationComplete = true;
	for (auto active = activatedIds_.rbegin(); active != activatedIds_.rend(); ++active)
	{
		const PackageDeactivationResult deactivation =
			registry.deactivateDetailed(*active);
		if (deactivation ||
			deactivation.error == PackageDeactivationError::NotActive ||
			deactivation.error == PackageDeactivationError::NotFound)
		{
			++result.deactivated;
			continue;
		}
		deactivationComplete = false;
		result.failures.push_back(
			"deactivate " + *active + ": code " +
			std::to_string(static_cast<int>(deactivation.error)));
	}
	if (deactivationComplete) activatedIds_.clear();

	std::vector<std::string> stillRegistered;
	stillRegistered.reserve(registeredIds_.size());
	for (const std::string& id : registeredIds_)
		if (registry.find(id)) stillRegistered.push_back(id);
	if (!stillRegistered.empty())
	{
		const PackageUnregistrationBatchResult unregistration =
			registry.unregisterPackages(stillRegistered);
		if (unregistration)
		{
			result.unregistered = stillRegistered.size();
			registeredIds_.clear();
		}
		else
		{
			result.failures.push_back(
				"unregister " + unregistration.packageId + ": code " +
				std::to_string(static_cast<int>(unregistration.error)));
		}
	}
	else
	{
		registeredIds_.clear();
	}

	if (mountedIds_.empty() && activatedIds_.empty() && registeredIds_.empty())
	{
		discoveredIds_.clear();
		packages_.clear();
		attempted_ = false;
	}
	return result;
}

PackageHost& GetStartupPackageHost()
{
	static PackageHost host;
	return host;
}

PackageHostResult InitializeStartupDataPackages(const PackageStartupOptions& options)
{
	if (!options.enabled) return PackageHostResult{};
	VfsPackageAssetMounter mounter;
	GameContext& game = GetGameContext();
	PackageHostResult result =
		GetStartupPackageHost().initialize(game.packages(), options, mounter);
	if (result)
	{
		game.log().write(LogRecord{LogSeverity::Info, "packages",
			"Discovered data packages: " + JoinIds(result.discovered)});
		game.log().write(LogRecord{LogSeverity::Info, "packages",
			"Activated data packages: " + JoinIds(result.activated)});
	}
	else
	{
		game.log().write(LogRecord{LogSeverity::Error, "packages",
			"Data package startup failed: " + result.message});
	}
	return result;
}

PackageHostShutdownResult ShutdownStartupDataPackages()
{
	VfsPackageAssetMounter mounter;
	GameContext& game = GetGameContext();
	return GetStartupPackageHost().shutdown(game.packages(), mounter);
}
