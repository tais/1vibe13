#include <Engine/Core/Identifier.h>

bool IsValidEngineIdentifier(const std::string& identifier) noexcept
{
	if (identifier.empty() || identifier.size() > MaximumEngineIdentifierBytes)
		return false;
	for (char value : identifier)
	{
		const bool valid = (value >= 'a' && value <= 'z') ||
			(value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
			value == '.' || value == '_' || value == '-';
		if (!valid) return false;
	}
	return true;
}
