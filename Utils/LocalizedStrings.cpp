#include "LocalizedStrings.h"
#include "TextInfrastructureModel.h"

#include <vfs/Tools/vfs_tools.h>
#include <vfs/Tools/vfs_property_container.h>

#include <array>

namespace
{
	// initFromXMLFile takes its TagMap argument by non-const reference;
	// MSVC accepted a TagMap() rvalue, clang doesn't. Bind a single
	// per-TU empty instance.
	vfs::PropertyContainer::TagMap& EmptyTagMap()
	{
		static vfs::PropertyContainer::TagMap m;
		return m;
	}
}

bool g_bUseXML_Strings = false;

namespace Loc
{
	bool Init(Topic t, vfs::String const& section);
	void Clear(Topic t);
	void ClearAll();

	class _Strings
	{
	public:
		_Strings() : initialized(false) {};
		bool					initialized;
		vfs::PropertyContainer	stringMap;
	};
	class _PropState
	{
	public:
		vfs::Path	filename;
		TextInfrastructureModel::LazyLoadState lifecycle;
	};

	typedef std::map<vfs::String,_PropState,vfs::String::Less> tSectionState;

	static std::array<vfs::PropertyContainer, TOPIC_COUNT> _localizedStrings;
	static std::array<tSectionState, TOPIC_COUNT> _topicFiles;
};

namespace
{
	bool IsValidTopic(Loc::Topic topic)
	{
		return TextInfrastructureModel::IsValidIndex(
			Loc::TOPIC_COUNT, static_cast<int>(topic));
	}

	std::size_t TopicIndex(Loc::Topic topic)
	{
		return static_cast<std::size_t>(topic);
	}

	bool LoadLocalizationFile(vfs::PropertyContainer& strings,
		Loc::_PropState& state)
	{
		if (!TextInfrastructureModel::NeedsLoad(state.lifecycle)) return true;
		const bool loaded = strings.initFromXMLFile(
			state.filename, EmptyTagMap());
		TextInfrastructureModel::RecordLoadResult(state.lifecycle, loaded);
		return loaded;
	}

	const vfs::String& EmptyLocalizedString()
	{
		static const vfs::String empty;
		return empty;
	}
}



bool Loc::AssociateWithFile(Loc::Topic t, vfs::Path const& sFilename)
{
	if (!IsValidTopic(t) || sFilename.empty()) return false;
	_PropState& state = _topicFiles[TopicIndex(t)][L"_ALL"];
	state.filename = sFilename;
	TextInfrastructureModel::Associate(state.lifecycle);
	return true;
}

bool Loc::AssociateWithFile(Topic t, vfs::Path const& sFilename, vfs::String const& section)
{
	if (!IsValidTopic(t) || sFilename.empty() || section.empty()) return false;
	_PropState& state = _topicFiles[TopicIndex(t)][section];
	state.filename = sFilename;
	TextInfrastructureModel::Associate(state.lifecycle);
	return true;
}


bool Loc::GetString(Loc::Topic t, vfs::String const& section, vfs::String const& key, vfs::String& value)
{
	if (!Init(t,section))
	{
		value = vfs::String();
		return false;
	}
	return _localizedStrings[TopicIndex(t)].getStringProperty(section, key, value);
}
bool Loc::GetString(Loc::Topic t, vfs::String const& section, int key, vfs::String& value)
{
	return GetString(t, section, vfs::toString<wchar_t>(key), value);
}

bool Loc::GetString(Topic t, vfs::String const& section, vfs::String const& key, vfs::String::char_t* value, vfs::UInt32 len)
{
	if (!value || len == 0 || !Init(t,section))
	{
		if (value && len > 0) value[0] = L'\0';
		return false;
	}
	return _localizedStrings[TopicIndex(t)].getStringProperty(section, key, value, len);
}
bool Loc::GetString(Topic t, vfs::String const& section, int key, vfs::String::char_t* value, vfs::UInt32 len)
{
	return GetString(t, section, vfs::toString<wchar_t>(key), value, len);
}

vfs::String const& Loc::GetString(Topic t, vfs::String const& section, vfs::String const& key)
{
	if (!Init(t,section)) return EmptyLocalizedString();
	return _localizedStrings[TopicIndex(t)].getStringProperty(section, key);
}
vfs::String const& Loc::GetString(Topic t, vfs::String const& section, int key)
{
	return GetString(t,section,vfs::toString<wchar_t>(key));
}



bool Loc::Init(Topic t, vfs::String const& section)
{
	if (!IsValidTopic(t)) return false;
	const std::size_t index = TopicIndex(t);
	tSectionState& files = _topicFiles[index];
	bool loaded = true;

	const auto common = files.find(L"_ALL");
	if (common != files.end())
		loaded = LoadLocalizationFile(_localizedStrings[index], common->second);

	if (!vfs::String::equalCase(section.c_str(), L"_ALL"))
	{
		const auto specific = files.find(section);
		if (specific != files.end())
		{
			const bool sectionLoaded = LoadLocalizationFile(
				_localizedStrings[index], specific->second);
			loaded = sectionLoaded && loaded;
		}
	}
	return loaded;
}

void Loc::Clear(Topic t)
{
	if (!IsValidTopic(t)) return;
	const std::size_t index = TopicIndex(t);
	_localizedStrings[index].clearContainer();
	for (auto& entry : _topicFiles[index])
	{
		TextInfrastructureModel::Reset(entry.second.lifecycle);
	}
}

void Loc::ClearAll()
{
	for (auto& topic : _topicFiles)
	{
		for (auto& entry : topic)
		{
			TextInfrastructureModel::Reset(entry.second.lifecycle);
		}
	}
	for (auto& strings : _localizedStrings) strings.clearContainer();
}




