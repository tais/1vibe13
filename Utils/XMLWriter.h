#ifndef _XMLWRITER_H_
#define _XMLWRITER_H_

#include "DataBoundaryModel.h"
#include "FileMan.h"
#include <vfs/Core/Interface/vfs_file_interface.h>
#include <vfs/Core/vfs_string.h>

#include <stack>
#include <string>
#include <locale>
#include <sstream>
#include <vector>

class XMLWriter
{
public:
	typedef std::pair<std::string, std::string> attribute_type;
public:
	XMLWriter() : m_iIndentLevel(0)  
	{
		m_ssBuffer << "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" ;
	};
	~XMLWriter()
	{};
	
	template<typename ValueType>
	void addAttributeToNextValue(vfs::String const& attribute, ValueType const& value)
	{
		std::stringstream temp_buffer;
		temp_buffer.imbue(std::locale::classic());
		if (!(temp_buffer << value))
		{
			m_isValid = false;
			return;
		}
		addEscapedAttribute(attribute, temp_buffer.str());
	}

	template<typename ValueType>
	void addValue(vfs::String const& key, ValueType const& value)
	{
		std::stringstream temp_buffer;
		temp_buffer.imbue(std::locale::classic());
		if (!(temp_buffer << value))
		{
			m_isValid = false;
			return;
		}
		addEscapedValue(key, temp_buffer.str());
	}

	void		addValue(vfs::String const& key);
	void		addComment(vfs::String const& comment);
	void		addFlag(UINT32 const& flags, UINT32 const& flag, vfs::String strFlag);

	void		openNode(vfs::String const& key);
	bool		closeNode();

	bool		writeToFile(vfs::Path const& sFileName);
	// Exact-transfer stream seam. The file must be closed on entry; this opens
	// it fresh with truncation and explicitly closes it. Failure is reported but
	// caller-owned storage cannot be rolled back, so path replacement should use
	// the atomic overload above.
	bool		writeToFile(vfs::tWritableFile* pFile);

private:
	std::string	indent();
	void		addEscapedAttribute(vfs::String const& attribute,
				std::string const& value);
	void		addEscapedValue(vfs::String const& key,
				std::string const& value);
	void		insertAttributesIntoBuffer();
	bool		isComplete() const;
private:
	std::stringstream				m_ssBuffer;
	std::stack<std::string>			m_stOpenNodes;
	std::vector<attribute_type>		m_stNextValAttributes;
	int								m_iIndentLevel;
	bool							m_isValid = true;
};


void testMXLWriter();

#endif // _XMLWRITER_H_

