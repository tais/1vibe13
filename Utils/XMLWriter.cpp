#include "XMLWriter.h"
#include "sgp_logger.h"

#include <vfs/Core/vfs.h>

#include <exception>

namespace
{
	void ReportXmlWriteFailure(const char* message) noexcept
	{
		try { SGP_ERROR(message); } catch (...) {}
	}

	bool WriteFreshXmlStream(
		vfs::tWritableFile* file, const std::string& document)
	{
		if (!file || file->isOpenRead() || file->isOpenWrite()) return false;
		bool opened = false;
		bool transferred = false;
		try
		{
			if (file->openWrite(true, true))
			{
				opened = true;
				const vfs::size_t requested =
					document.length() * sizeof(std::string::value_type);
				const vfs::size_t written = file->write(
					document.c_str(), requested);
				transferred = UtilsDataBoundaryModel::IsExactTransfer(
					requested, written);
			}
			else
			{
				// An implementation may still have acquired its native handle
				// even when it reports failure.  Observe and contain it below.
				opened = file->isOpenRead() || file->isOpenWrite();
			}
		}
		catch (const std::exception& ex)
		{
			ReportXmlWriteFailure(ex.what());
		}
		catch (...)
		{
			ReportXmlWriteFailure("Unknown XML stream write failure");
		}

		bool needsClose = opened;
		if (!needsClose)
		{
			try { needsClose = file->isOpenRead() || file->isOpenWrite(); }
			catch (...) { needsClose = true; }
		}
		bool closed = !needsClose;
		if (needsClose)
		{
			try
			{
				file->close();
				closed = true;
			}
			catch (const std::exception& ex)
			{
				ReportXmlWriteFailure(ex.what());
			}
			catch (...)
			{
				ReportXmlWriteFailure("Unknown XML stream close failure");
			}
		}
		return transferred && closed;
	}

}

void XMLWriter::addValue(vfs::String const& key)
{
	m_ssBuffer << indent() <<  "<" << key.utf8();
	insertAttributesIntoBuffer();
	m_ssBuffer << " />\n";
}

void XMLWriter::addComment(vfs::String const& comment)
{
	std::string sanitized;
	if (!UtilsDataBoundaryModel::SanitizeXmlComment(
		comment.utf8(), sanitized))
	{
		m_isValid = false;
		return;
	}
	m_ssBuffer << indent() << "<!-- " << sanitized << " -->\n";
}

void XMLWriter::addFlag(UINT32 const& flags, UINT32 const& flag, vfs::String strFlag)
{
	if( ( flags & flag) == flag )
	{
		this->addValue(strFlag);
	}
}

void XMLWriter::openNode(vfs::String const& key)
{
	std::string utf8key = key.utf8();
	m_ssBuffer << indent() << "<" << utf8key;
	insertAttributesIntoBuffer();
	m_ssBuffer << ">\n";
	m_iIndentLevel += 1;
	m_stOpenNodes.push(utf8key);
}

bool XMLWriter::closeNode()
{
	if(m_iIndentLevel < 1 || m_stOpenNodes.empty())
	{
		m_isValid = false;
		return false;
	}
	m_iIndentLevel -= 1;
	m_ssBuffer << indent() << "</" << m_stOpenNodes.top() << ">\n";
	m_stOpenNodes.pop();
	return true;
}

bool XMLWriter::writeToFile(vfs::Path const& sFileName)
{
	if (!isComplete()) return false;
	try
	{
		const std::string document = m_ssBuffer.str();
		vfs::CVirtualFileSystem* const fileSystem = getVFS();
		return fileSystem && fileSystem->replaceFileAtomically(sFileName,
			reinterpret_cast<const vfs::Byte*>(document.data()), document.size());
	}
	catch(const std::exception& ex)
	{
		ReportXmlWriteFailure(ex.what());
	}
	catch (...)
	{
		ReportXmlWriteFailure("Unknown XML path write failure");
	}
	return false;
}

bool XMLWriter::writeToFile(vfs::tWritableFile* pFile)
{
	if (!pFile || !isComplete()) return false;
	try
	{
		return WriteFreshXmlStream(pFile, m_ssBuffer.str());
	}
	catch(const std::exception& ex)
	{
		ReportXmlWriteFailure(ex.what());
	}
	catch (...)
	{
		ReportXmlWriteFailure("Unknown XML stream staging failure");
	}
	return false;
}

std::string XMLWriter::indent()
{
	std::string indent_string;
	for(int i=0; i < m_iIndentLevel; ++i)
	{
		indent_string += "\t";
	}
	return indent_string;
}

void XMLWriter::insertAttributesIntoBuffer()
{
	if(!m_stNextValAttributes.empty())
	{
		std::vector<attribute_type>::iterator it = m_stNextValAttributes.begin();
		for(; it != m_stNextValAttributes.end(); ++it)
		{
			m_ssBuffer << " " << it->first << "=\"" << it->second << "\"";
		}
	}
	m_stNextValAttributes.clear();
}

void XMLWriter::addEscapedAttribute(vfs::String const& attribute,
	std::string const& value)
{
	std::string escaped;
	if (!UtilsDataBoundaryModel::EscapeXml(value, escaped))
	{
		m_isValid = false;
		return;
	}
	m_stNextValAttributes.emplace_back(attribute.utf8(), std::move(escaped));
}

void XMLWriter::addEscapedValue(vfs::String const& key,
	std::string const& value)
{
	std::string escaped;
	if (!UtilsDataBoundaryModel::EscapeXml(value, escaped))
	{
		m_isValid = false;
		return;
	}
	const std::string utf8key = key.utf8();
	m_ssBuffer << indent() << "<" << utf8key;
	insertAttributesIntoBuffer();
	m_ssBuffer << ">" << escaped << "</" << utf8key << ">\n";
}

bool XMLWriter::isComplete() const
{
	return m_isValid && m_ssBuffer.good() && m_iIndentLevel == 0 && m_stOpenNodes.empty() &&
		m_stNextValAttributes.empty();
}

void testMXLWriter()
{
	//XMLWriter<char> xmlw;
	XMLWriter xmlw;
	//xmlw.openNode(L"root");
	xmlw.openNode("root");
	xmlw.addAttributeToNextValue("attr1",10);
	xmlw.addAttributeToNextValue("attr2","string");
	//xmlw.AddValue(L"val1",10);
	xmlw.addValue("val1",10);

	xmlw.addAttributeToNextValue("node_attr",17);
	xmlw.addComment("bbb -->\n <a> comment</a> <!-- aaa");
	xmlw.openNode("test");
	//xmlw.addValue(L"val2",10.5);
	xmlw.addValue("val2",10.5);
	xmlw.closeNode();
	xmlw.closeNode();
	//xmlw.WriteToFile(WideString("xml_output/test.xml")());
	xmlw.writeToFile("xml_output/test.xml");
}
