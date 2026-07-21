#ifndef ENGINE_CORE_PACKAGE_AUDIO_H
#define ENGINE_CORE_PACKAGE_AUDIO_H

#include <cstdint>
#include <string>
#include <utility>

#include <Engine/Core/AudioGroupService.h>

class PackageAudio
{
public:
	PackageAudio(std::string packageId, AudioGroupService& service)
		: packageId_(std::move(packageId)), service_(service) {}

	const std::string& packageId() const { return packageId_; }
	PackageAudioPlayResult play(std::string group, AudioPlaybackRequest request) const noexcept
	{
		return service_.play(packageId_, std::move(group), std::move(request));
	}
	bool stop(AudioPlaybackId playback) const noexcept
	{
		return service_.stop(packageId_, playback);
	}
	PackageAudioOperationResult stopGroup(const std::string& group) const noexcept
	{
		return service_.stopGroup(packageId_, group);
	}
	PackageAudioOperationResult setGroupVolume(
		const std::string& group, std::uint32_t volume) const noexcept
	{
		return service_.setGroupVolume(packageId_, group, volume);
	}

private:
	std::string packageId_;
	AudioGroupService& service_;
};

#endif
