#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_file_raii.h>
#include <vfs/Core/vfs_init.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <list>
#include <string>

namespace
{
void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

class TemporaryRoot
{
public:
	TemporaryRoot()
	{
		const auto seed = static_cast<unsigned long long>(
			std::chrono::steady_clock::now().time_since_epoch().count());
		for (unsigned attempt = 0; attempt < 256; ++attempt)
		{
			path_ = std::filesystem::temp_directory_path() /
				("ja2-dedicated-vfs-test-" + std::to_string(seed) + "-" +
					std::to_string(attempt));
			std::error_code error;
			if (std::filesystem::create_directory(path_, error)) return;
		}
		Check(false, "temporary root can be created");
	}

	~TemporaryRoot()
	{
		vfs::CVirtualFileSystem::shutdownVFS();
		std::error_code error;
		std::filesystem::remove_all(path_, error);
	}

	const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_;
};

vfs::String NativeString(const std::filesystem::path& path)
{
	return vfs::String(path.c_str());
}

void WriteNative(const std::filesystem::path& path, const std::string& bytes)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	output.close();
	Check(static_cast<bool>(output), "native fixture writes completely");
}

std::string ReadNative(const std::filesystem::path& path)
{
	std::ifstream input(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>());
}

std::string ReadVirtual(const char* path)
{
	vfs::tReadableFile* file = getVFS()->getReadFile(vfs::Path(path));
	Check(file && file->openRead(), "virtual fixture opens for read");
	const vfs::size_t size = file->getSize();
	std::string bytes(size, '\0');
	const vfs::size_t read = size
		? file->read(reinterpret_cast<vfs::Byte*>(&bytes[0]), size) : 0;
	file->close();
	Check(read == size, "virtual fixture reads completely");
	return bytes;
}

void WriteVirtual(const char* path, const std::string& bytes)
{
	const vfs::Path logical(path);
	vfs::COpenWriteFile file(logical, true, true,
		vfs::CVirtualFile::SF_STOP_ON_WRITABLE_PROFILE);
	const vfs::size_t written = bytes.empty() ? 0 : file->write(
		reinterpret_cast<const vfs::Byte*>(bytes.data()), bytes.size());
	Check(written == bytes.size(), "virtual fixture writes completely");
}

void AddDirectoryProfile(vfs_init::VfsConfig& config, const wchar_t* name,
	const std::filesystem::path& root, bool writable)
{
	vfs_init::Profile* profile = new vfs_init::Profile();
	profile->m_name = name;
	profile->m_root = NativeString(root);
	profile->m_writable = writable;
	vfs_init::Location* location = new vfs_init::Location();
	location->m_type = L"DIRECTORY";
	location->m_path = L"";
	location->m_mount_point = L"";
	profile->addLocation(location, true);
	config.addProfile(profile, true);
}

void ConfigureProperties(vfs::PropertyContainer& properties,
	const std::filesystem::path& data,
	const std::filesystem::path& oldWritable)
{
	properties.setStringListProperty(L"vfs_config", L"PROFILES",
		std::list<vfs::String>{L"DATA", L"USER"});
	properties.setStringProperty(L"PROFILE_DATA", L"NAME", L"DATA");
	properties.setStringProperty(L"PROFILE_DATA", L"PROFILE_ROOT",
		NativeString(data));
	properties.setBoolProperty(L"PROFILE_DATA", L"WRITE", false);
	properties.setStringListProperty(L"PROFILE_DATA", L"LOCATIONS",
		std::list<vfs::String>{L"DATA_ROOT"});
	properties.setStringProperty(L"LOC_DATA_ROOT", L"TYPE", L"DIRECTORY");
	properties.setStringProperty(L"LOC_DATA_ROOT", L"PATH", L"");
	properties.setStringProperty(L"LOC_DATA_ROOT", L"MOUNT_POINT", L"");

	properties.setStringProperty(L"PROFILE_USER", L"NAME", L"OLD_USER");
	properties.setStringProperty(L"PROFILE_USER", L"PROFILE_ROOT",
		NativeString(oldWritable));
	properties.setBoolProperty(L"PROFILE_USER", L"WRITE", true);
	properties.setStringListProperty(L"PROFILE_USER", L"LOCATIONS",
		std::list<vfs::String>{L"IGNORED_MISSING_LOCATION"});
	// The displaced profile deliberately names a malformed location. Successful
	// initialization proves that it was skipped before any root/location scan.
	properties.setStringProperty(L"LOC_IGNORED_MISSING_LOCATION", L"TYPE",
		L"NOT_FOUND");
}

void RunIsolationFixture(const std::string& campaignName,
	const std::string& forbiddenOtherCampaignFile)
{
	TemporaryRoot root;
	const std::filesystem::path data = root.path() / "data";
	const std::filesystem::path oldWritable = root.path() / "old-user";
	const std::filesystem::path campaign = root.path() / campaignName;
	const std::filesystem::path package = root.path() / "package";
	std::filesystem::create_directories(data);
	std::filesystem::create_directories(oldWritable);
	std::filesystem::create_directories(campaign);
	std::filesystem::create_directories(package);

	WriteNative(data / "data-only.txt", "data");
	WriteNative(data / "setting.ini", "data-setting");
	WriteNative(data / "DedicatedCheckpointA.sav", "read-only-checkpoint-name");
	WriteNative(oldWritable / "old-only.txt", "old-user");
	WriteNative(oldWritable / "setting.ini", "old-setting");
	WriteNative(campaign / "setting.ini", "campaign-setting");
	WriteNative(campaign / (campaignName + ".txt"), campaignName);
	WriteNative(package / "package-only.txt", "package");
	WriteNative(package / "setting.ini", "package-setting");

	vfs::PropertyContainer properties;
	ConfigureProperties(properties, data, oldWritable);
	vfs_init::WritableProfileOverride replacement;
	replacement.name = L"_DEDICATED_CAMPAIGN";
	replacement.root = NativeString(campaign);
	Check(vfs_init::initVirtualFileSystem(properties, replacement),
		"VFS initializes with a replacement writable profile");

	vfs::CProfileStack* profiles = getVFS()->getProfileStack();
	vfs::CVirtualProfile* writeProfile = profiles->getWriteProfile();
	Check(writeProfile && writeProfile == profiles->topProfile() &&
		writeProfile->cName == L"_DEDICATED_CAMPAIGN" &&
		writeProfile->cRoot == vfs::Path(campaign.c_str()),
		"the campaign profile is the sole top writable profile");
	Check(profiles->getProfile(L"OLD_USER") == nullptr &&
		!getVFS()->fileExists(vfs::Path("old-only.txt")),
		"the configured global writable profile is never mounted or scanned");
	Check(ReadVirtual("data-only.txt") == "data" &&
		ReadVirtual("setting.ini") == "campaign-setting",
		"read-only Data remains visible below campaign-local settings");
	Check(ReadVirtual("DedicatedCheckpointA.sav") ==
		"read-only-checkpoint-name",
		"a read-only layer can contain the fixed campaign scratch name");
	Check(forbiddenOtherCampaignFile.empty() ||
		!getVFS()->fileExists(vfs::Path(forbiddenOtherCampaignFile)),
		"one campaign cannot observe a prior campaign profile");

	WriteVirtual("root-write.txt", "root");
	WriteVirtual("Temp/temp-write.bin", "temp");
	WriteVirtual("SavedGames/save-write.sav", "save");
	WriteVirtual("DedicatedCheckpointA.sav", "campaign-checkpoint");
	Check(std::filesystem::exists(campaign / "root-write.txt") &&
		std::filesystem::exists(campaign / "Temp" / "temp-write.bin") &&
		std::filesystem::exists(
			campaign / "SavedGames" / "save-write.sav") &&
		!std::filesystem::exists(oldWritable / "root-write.txt") &&
		!std::filesystem::exists(oldWritable / "Temp" / "temp-write.bin") &&
		!std::filesystem::exists(
			oldWritable / "SavedGames" / "save-write.sav"),
		"root, Temp, and SavedGames writes land only in the campaign profile");
	Check(ReadVirtual("DedicatedCheckpointA.sav") == "campaign-checkpoint" &&
		std::filesystem::exists(campaign / "DedicatedCheckpointA.sav") &&
		ReadNative(data / "DedicatedCheckpointA.sav") ==
			"read-only-checkpoint-name",
		"create-always semantics shadow a read-only scratch-name collision");

	vfs_init::VfsConfig packageConfig;
	AddDirectoryProfile(packageConfig, L"PACKAGE", package, false);
	Check(vfs_init::initVirtualFileSystem(packageConfig, false),
		"a late read-only package profile can be added");
	Check(profiles->getWriteProfile() == writeProfile &&
		profiles->topProfile() == writeProfile &&
		ReadVirtual("package-only.txt") == "package" &&
		ReadVirtual("setting.ini") == "campaign-setting",
		"late package overlays stay below the campaign write profile");
}
}

int main()
{
	RunIsolationFixture("campaign-one", "campaign-two.txt");
	RunIsolationFixture("campaign-two", "campaign-one.txt");
	std::puts("dedicated campaign VFS profile tests passed");
	return 0;
}
