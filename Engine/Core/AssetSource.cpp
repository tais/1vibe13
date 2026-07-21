#include <Engine/Core/AssetSource.h>

bool NormalizeAssetPath(const std::string& input, std::string& normalized)
{
	normalized.clear();
	if (input.empty() || input.front() == '/' || input.front() == '\\') return false;

	std::string component;
	for (std::size_t index = 0; index <= input.size(); ++index)
	{
		char value = index == input.size() ? '/' : input[index];
		if (value != '/' && value != '\\')
		{
			const unsigned char byte = static_cast<unsigned char>(value);
			if (byte < 32 || value == ':') return false;
			// bfVFS historically treats logical paths case-insensitively. Keep
			// package/headless sources compatible on case-sensitive hosts.
			if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
			component.push_back(value);
			continue;
		}

		if (component.empty() || component == ".")
		{
			component.clear();
			continue;
		}
		if (component == "..") return false;
		if (!normalized.empty()) normalized.push_back('/');
		normalized += component;
		component.clear();
	}
	return !normalized.empty();
}

bool IsValidAssetProvenance(const std::string& provenance)
{
	return IsValidEngineIdentifier(provenance);
}
