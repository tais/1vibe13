#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "DataBoundaryModel.h"

#include <vfs/Core/vfs_types.h>
#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_file_raii.h>
#include <vfs/Core/File/vfs_file.h>

#include <vfs/Tools/vfs_tools.h>
#include <vfs/Tools/vfs_property_container.h>

#include "XML_Parser.h"
#include "XMLWriter.h"
#include "DEBUG.H"

#include <cstring>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

vfs::PropertyContainer::TagMap::TagMap()
{
	// setup default map
	_map[L"Container"] = L"Container";
	_map[L"Section"] = L"Section";
	_map[L"SectionID"] = L"name";
	_map[L"Key"] = L"Key";
	_map[L"KeyID"] = L"name";
}
vfs::String const& vfs::PropertyContainer::TagMap::container(vfs::String::char_t* container)
{
	if(container)
	{
		_map[L"Container"] = container;
	}
	return _map[L"Container"];
}
vfs::String const& vfs::PropertyContainer::TagMap::section(vfs::String::char_t* section)
{
	if(section)
	{
		_map[L"Section"] = section;
	}
	return _map[L"Section"];
}
vfs::String const& vfs::PropertyContainer::TagMap::sectionID(vfs::String::char_t* section_id)
{
	if(section_id)
	{
		_map[L"SectionID"] = section_id;
	}
	return _map[L"SectionID"];
}
vfs::String const& vfs::PropertyContainer::TagMap::key(vfs::String::char_t* key)
{
	if(key)
	{
		_map[L"Key"] = key;
	}
	return _map[L"Key"];
}
vfs::String const& vfs::PropertyContainer::TagMap::keyID(vfs::String::char_t* key_id)
{
	if(key_id)
	{
		_map[L"KeyID"] = key_id;
	}
	return _map[L"KeyID"];
}

bool vfs::PropertyContainer::writeToXMLFile(vfs::Path const& sFileName, vfs::PropertyContainer::TagMap& tagmap)
{
	XMLWriter xmlw;

	xmlw.openNode(tagmap.container());

	tSections::iterator sit = m_mapProps.begin();
	for(; sit != m_mapProps.end(); ++sit)
	{
		xmlw.addAttributeToNextValue(tagmap.sectionID(),sit->first.utf8());
		xmlw.openNode(tagmap.section());

		vfs::PropertyContainer::Section& section = sit->second;
		vfs::PropertyContainer::Section::tProps::iterator kit = section.mapProps.begin();
		for(; kit != section.mapProps.end(); ++kit)
		{
			xmlw.addAttributeToNextValue(tagmap.keyID(), kit->first.utf8());
			xmlw.addValue(tagmap.key(), kit->second.utf8());
		}

		xmlw.closeNode();

	}

	xmlw.closeNode();

	return xmlw.writeToFile(sFileName);
}

/*********************************************************************************/
/*********************************************************************************/

class CPropertyXMLParser : public IXMLParser
{
	enum DOM_OBJECT
	{
		DO_ELEMENT_Container,
		DO_ELEMENT_Section,
		DO_ELEMENT_Key,
		DO_ELEMENT_NONE,
	};
public:
	CPropertyXMLParser(
			vfs::PropertyContainer& container,
			vfs::PropertyContainer::TagMap& tagmap,
			XML_Parser &parser,
			IXMLParser* caller = NULL)
		: IXMLParser("",&parser,caller), 
		_container(container),
		_tagmap(tagmap),
		current_state(DO_ELEMENT_NONE) // doesn't matter where we come from, we start fresh
	{};
	virtual void onStartElement(const XML_Char* name, const XML_Char** atts);
	virtual void onEndElement(const XML_Char* name);
	virtual void onTextElement(const XML_Char *str, int len);
	bool complete() const
	{
		return valid && saw_container && closed_container &&
			current_state == DO_ELEMENT_NONE;
	}
private:
	bool readRequiredAttribute(vfs::String const& attribute,
		const XML_Char** atts, vfs::String& value);
	vfs::PropertyContainer&				_container;
	vfs::PropertyContainer::TagMap&		_tagmap;
	DOM_OBJECT						current_state;
	vfs::String						current_section;
	vfs::String						current_key;
	bool							valid = true;
	bool							saw_container = false;
	bool							closed_container = false;
	UtilsDataBoundaryModel::UnknownXmlSubtree unknown_subtree;
};

struct PropertyParserContext
{
	PropertyParserContext(vfs::PropertyContainer& container,
		vfs::PropertyContainer::TagMap& tagmap)
		: document(container, tagmap, parser, NULL)
	{
	}

	XML_Parser parser = NULL;
	CPropertyXMLParser document;
};

static void PreparePropertyParser(XML_Parser parser, void *userData)
{
	PropertyParserContext *context =
		(PropertyParserContext *)userData;
	context->parser = parser;
	context->document.grabParser();
}


void CPropertyXMLParser::onStartElement(const XML_Char *name, const XML_Char **atts)
{
	if (!valid) return;
	if (unknown_subtree.active())
	{
		valid = unknown_subtree.enter();
		return;
	}
	vfs::String utf8_name(name);
	if(current_state == DO_ELEMENT_NONE && !saw_container &&
		vfs::StrCmp::Equal(utf8_name,_tagmap.container()))
	{
		current_state = DO_ELEMENT_Container;
		saw_container = true;
	}
	else if(current_state == DO_ELEMENT_Container && vfs::StrCmp::Equal(utf8_name, _tagmap.section()))
	{
		current_state = DO_ELEMENT_Section;
		if (!readRequiredAttribute(
			_tagmap.sectionID(), atts, current_section)) valid = false;
	}
	else if(current_state == DO_ELEMENT_Section && vfs::StrCmp::Equal(utf8_name, _tagmap.key()))
	{
		current_state = DO_ELEMENT_Key;
		if (!readRequiredAttribute(_tagmap.keyID(), atts, current_key))
			valid = false;
	}
	else if (current_state != DO_ELEMENT_NONE)
	{
		valid = unknown_subtree.enter();
		return;
	}
	else valid = false;
	sCharData = "";
}

void CPropertyXMLParser::onEndElement(const XML_Char* name)
{
	if (!valid) return;
	if (unknown_subtree.active())
	{
		valid = unknown_subtree.leave();
		return;
	}
	vfs::String utf8_name(name);
	if(current_state == DO_ELEMENT_Key && vfs::StrCmp::Equal(utf8_name, _tagmap.key()))
	{
		_container.setStringProperty(current_section, current_key, vfs::trimString(sCharData,0,sCharData.length()));
		current_state = DO_ELEMENT_Section;
	}
	else if(current_state == DO_ELEMENT_Section && vfs::StrCmp::Equal(utf8_name, _tagmap.section()))
	{
		current_state = DO_ELEMENT_Container;
	}
	else if(current_state == DO_ELEMENT_Container && vfs::StrCmp::Equal(utf8_name, _tagmap.container()))
	{
		current_state = DO_ELEMENT_NONE;
		closed_container = true;
	}
	else valid = false;
}

void CPropertyXMLParser::onTextElement(const XML_Char *str, int len)
{
	if(!unknown_subtree.active() && current_state == DO_ELEMENT_Key)
	{
		sCharData.append(str,len);
	}
}

bool CPropertyXMLParser::readRequiredAttribute(
	vfs::String const& attribute, const XML_Char** atts, vfs::String& value)
{
	if (!atts) return false;
	const std::string expected = attribute.utf8();
	for (std::size_t index = 0; atts[index] && atts[index + 1]; index += 2)
	{
		if (std::strcmp(atts[index], expected.c_str()) == 0)
		{
			value = atts[index + 1];
			return true;
		}
	}
	return false;
}

namespace
{
	void ThrowPropertyXmlFailure(vfs::Path const& path,
		const wchar_t* reason)
	{
		std::wstringstream message;
		message << L"Could not load property XML ["
			<< vfs::String::as_utf16(path.to_string()) << L"]: " << reason;
		SGP_THROW(message.str().c_str());
	}

	template <typename ReadableFile>
	bool ParsePropertyFile(vfs::PropertyContainer& destination,
		vfs::PropertyContainer::TagMap& tagmap, vfs::Path const& logicalPath,
		ReadableFile& file)
	{
		const vfs::size_t size = file.getSize();
		if (size == std::numeric_limits<vfs::size_t>::max())
			ThrowPropertyXmlFailure(logicalPath, L"file is too large");

		std::vector<vfs::Byte> buffer(size + 1);
		const vfs::size_t bytesRead = file.read(buffer.data(), size);
		if (!UtilsDataBoundaryModel::IsExactTransfer(size, bytesRead))
			ThrowPropertyXmlFailure(logicalPath, L"short read");
		buffer[size] = 0;

		vfs::PropertyContainer staged(destination);
		PropertyParserContext context(staged, tagmap);
		LegacyXmlCallbacks callbacks;
		callbacks.userData = &context;
		callbacks.parserReady = PreparePropertyParser;
		const LegacyXmlResult result =
			ParseLegacyXmlBytes(buffer.data(), size, callbacks);
		if (!result)
		{
			const std::string path = logicalPath.to_string();
			const auto formatted = FormatLegacyXmlFailure(path.c_str(), result);
			std::wstringstream message;
			message << vfs::String::as_utf16(formatted.data());
			SGP_THROW(message.str().c_str());
		}
		if (!context.document.complete())
			ThrowPropertyXmlFailure(logicalPath, L"invalid property structure");

		destination = std::move(staged);
		return true;
	}
}

bool vfs::PropertyContainer::initFromXMLFile(vfs::Path const& sFileName, vfs::PropertyContainer::TagMap& tagmap)
{
	if(getVFS()->fileExists(sFileName))
	{
		vfs::COpenReadFile rfile(sFileName);
		return ParsePropertyFile(*this, tagmap, sFileName, rfile.file());
	}

	vfs::CFile file(sFileName);
	if(!file.openRead())
	{
		return false;
	}
	return ParsePropertyFile(*this, tagmap, sFileName, file);
}
