#include <Engine/Core/RuntimeCapabilities.h>

#include <algorithm>
#include <unordered_set>
#include <utility>

#include <Engine/Core/Identifier.h>

bool RuntimeCapabilities::add(const std::string& capability)
{
	if (!IsValidEngineIdentifier(capability) || contains(capability)) return false;
	capabilities_.push_back(capability);
	return true;
}

bool RuntimeCapabilities::addAll(const std::vector<std::string>& capabilities)
{
	if (!isValidList(capabilities)) return false;
	RuntimeCapabilities staged(*this);
	for (const std::string& capability : capabilities)
	{
		if (staged.contains(capability)) continue;
		staged.capabilities_.push_back(capability);
	}
	capabilities_ = std::move(staged.capabilities_);
	return true;
}

bool RuntimeCapabilities::contains(const std::string& capability) const
{
	return std::find(capabilities_.begin(), capabilities_.end(), capability) !=
		capabilities_.end();
}

bool RuntimeCapabilities::isValidList(const std::vector<std::string>& capabilities)
{
	std::unordered_set<std::string> unique;
	unique.reserve(capabilities.size());
	for (const std::string& capability : capabilities)
	{
		if (!IsValidEngineIdentifier(capability) || !unique.insert(capability).second)
			return false;
	}
	return true;
}
