#ifndef ENGINE_CORE_RUNTIME_CONFIGURATION_H
#define ENGINE_CORE_RUNTIME_CONFIGURATION_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <Engine/Core/Identifier.h>

using RuntimeConfigurationValue =
	std::variant<bool, std::int64_t, double, std::string>;

struct RuntimeConfigurationEntry
{
	std::string key;
	RuntimeConfigurationValue value;
};

enum class RuntimeConfigurationSetError
{
	None,
	InvalidKey,
	TypeMismatch,
	Sealed,
	AllocationFailure
};

// Typed startup configuration shared by applications, tools, and packages.
// Mutation is limited to composition time and values retain insertion order in
// snapshots. Sealing before bootstrap lets packages safely retain pointers to
// values without importing a parser, INI implementation, or campaign globals.
class RuntimeConfiguration
{
public:
	RuntimeConfigurationSetError set(
		std::string key, RuntimeConfigurationValue value) noexcept
	{
		if (sealed_) return RuntimeConfigurationSetError::Sealed;
		if (!IsValidEngineIdentifier(key))
			return RuntimeConfigurationSetError::InvalidKey;
		for (RuntimeConfigurationEntry& entry : entries_)
		{
			if (entry.key != key) continue;
			if (entry.value.index() != value.index())
				return RuntimeConfigurationSetError::TypeMismatch;
			try
			{
				entry.value = std::move(value);
			}
			catch (...)
			{
				return RuntimeConfigurationSetError::AllocationFailure;
			}
			return RuntimeConfigurationSetError::None;
		}
		try
		{
			entries_.push_back(RuntimeConfigurationEntry{
				std::move(key), std::move(value)});
		}
		catch (...)
		{
			return RuntimeConfigurationSetError::AllocationFailure;
		}
		return RuntimeConfigurationSetError::None;
	}

	template<typename Value>
	const Value* find(const std::string& key) const
	{
		for (const RuntimeConfigurationEntry& entry : entries_)
		{
			if (entry.key != key) continue;
			return std::get_if<Value>(&entry.value);
		}
		return nullptr;
	}

	const RuntimeConfigurationValue* findValue(const std::string& key) const
	{
		for (const RuntimeConfigurationEntry& entry : entries_)
			if (entry.key == key) return &entry.value;
		return nullptr;
	}

	void seal() { sealed_ = true; }
	bool sealed() const { return sealed_; }
	std::size_t size() const { return entries_.size(); }
	const std::vector<RuntimeConfigurationEntry>& entries() const { return entries_; }

	std::vector<RuntimeConfigurationEntry> snapshot() const { return entries_; }

	static RuntimeConfiguration& disabled()
	{
		static RuntimeConfiguration configuration;
		static const bool sealed = (configuration.seal(), true);
		(void)sealed;
		return configuration;
	}

private:
	std::vector<RuntimeConfigurationEntry> entries_;
	bool sealed_ = false;
};

#endif
