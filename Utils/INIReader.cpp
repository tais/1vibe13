#include "builddefines.h"
#include "INIReader.h"
#include "DataBoundaryModel.h"
#include "FileMan.h"
#include "DEBUG.H"
#include "Font Control.h"
#include "message.h"
#include <limits>
#include <sstream>

// Kaiden: INI reading function definitions:

#include <vfs/Core/vfs.h>

std::set<vfs::Path,vfs::Path::Less> CIniReader::m_merge_files;
std::stack<std::string> iniErrorMessages;

namespace
{
	const char* SafeString(const char* value)
	{
		return value ? value : "";
	}

	bool LoadIni(vfs::PropertyContainer& properties, vfs::Path const& path)
	{
		return UtilsDataBoundaryModel::PublishTransactionally(properties,
			[&path](vfs::PropertyContainer& staged) {
				return staged.initFromIniFile(path);
			});
	}

	bool LoadIni(vfs::PropertyContainer& properties, vfs::tReadableFile* file)
	{
		return UtilsDataBoundaryModel::PublishTransactionally(properties,
			[file](vfs::PropertyContainer& staged) {
				return staged.initFromIniFile(file);
			});
	}

	bool LoadMergedIni(vfs::PropertyContainer& properties,
		const char* fileName)
	{
		bool loadedAny = false;
		vfs::CProfileStack* profiles = getVFS()->getProfileStack();
		if (!profiles) return false;

		vfs::CProfileStack::Iterator it = profiles->begin();
		std::stack<vfs::CVirtualProfile*> reverseOrder;
		for (; !it.end(); it.next()) reverseOrder.push(it.value());
		while (!reverseOrder.empty())
		{
			vfs::IBaseFile* file = reverseOrder.top()->getFile(fileName);
			if (file)
			{
				loadedAny = LoadIni(properties,
					vfs::tReadableFile::cast(file)) || loadedAny;
			}
			reverseOrder.pop();
		}
		return loadedAny;
	}

	bool ReadPropertyText(vfs::PropertyContainer& properties,
		const char* section, const char* key, std::string& value)
	{
		vfs::String staged;
		if (!properties.getStringProperty(
			SafeString(section), SafeString(key), staged))
		{
			return false;
		}
		value = staged.utf8();
		return true;
	}
}

template<typename ValueType>
void PushErrorMessage(std::string const& filename,
					  std::string const& section,
					  std::string const& key, 
					  ValueType value, ValueType used_value,
					  ValueType minVal, ValueType maxVal)
{
	std::stringstream errMessage;
	errMessage << "The value [" << section << "][" <<  key << "] = \"" << value << "\" "
		<< "in file [" << filename << "] "
		<< "is outside the valid range [" << minVal << " , " << maxVal << "].  "
		<< used_value << " will be used.";
	iniErrorMessages.push(errMessage.str());
}

void CIniReader::RegisterFileForMerging(vfs::Path const& filename)
{
	m_merge_files.insert(filename);
}

CIniReader::CIniReader(const STR8	szFileName)
{
	UtilsDataBoundaryModel::CopyString(m_szFileName, SafeString(szFileName));
	if (!szFileName || !szFileName[0]) return;
	if(m_merge_files.find(szFileName) == m_merge_files.end())
	{
		CIniReader_File_Found = LoadIni(m_oProps, vfs::Path(szFileName));
	}
	else
	{
		CIniReader_File_Found = LoadMergedIni(m_oProps, szFileName);
	}
	// check for override file
#ifdef _WIN32
	{
		CHAR8 OvrFileName[256], Drive[128], Dir[128], Name[128], Ext[128];
		_splitpath(szFileName, Drive, Dir, Name, Ext);
		_makepath(OvrFileName, Drive, Dir, Name, "Override");
		if(getVFS()->fileExists(OvrFileName))
		{
			CIniReader_File_Found = (LoadIni(
				m_oProps, vfs::Path(OvrFileName)) ||
				CIniReader_File_Found) ? TRUE : FALSE;
		}
	}
#endif
}

CIniReader::CIniReader(const STR8	szFileName, BOOLEAN Force_Custom_Data_Path)
{
	(void)Force_Custom_Data_Path;
	// ary-05/05/2009 : force custom data path for potential non existing file -or- force default data path
	//       : Also, flag file detection to allow functions to determine course of action for case of file [not found/is found].
	UtilsDataBoundaryModel::CopyString(m_szFileName, SafeString(szFileName));
	if (!szFileName || !szFileName[0]) return;
	if(m_merge_files.find(szFileName) == m_merge_files.end())
	{
		CIniReader_File_Found = LoadIni(m_oProps, vfs::Path(szFileName));
	}
	else
	{
		CIniReader_File_Found = LoadMergedIni(m_oProps, szFileName);
	}
}

void CIniReader::Clear()
{
	UtilsDataBoundaryModel::CopyString(m_szFileName, "");
	m_legacyStringBuffer.fill(0);
	CIniReader_File_Found = FALSE;
	m_oProps.clearContainer();
}


int CIniReader::ReadInteger(const STR8	szSection, const STR8	szKey, int iDefaultValue)
{
	std::string text;
	std::int64_t parsed = 0;
	if (!ReadPropertyText(m_oProps, szSection, szKey, text) ||
		!UtilsDataBoundaryModel::ParseInt64(text, parsed) ||
		parsed < std::numeric_limits<int>::min() ||
		parsed > std::numeric_limits<int>::max())
	{
		return iDefaultValue;
	}
	return static_cast<int>(parsed);
}


int CIniReader::ReadInteger(const STR8 szSection, const STR8 szKey, int defaultValue, int minValue, int maxValue)
{
	int iniValueReadFromFile = ReadInteger(szSection, szKey, defaultValue);
	//AssertGE(iniValueReadFromFile, minValue);
	//AssertLE(iniValueReadFromFile, maxValue);
	if (iniValueReadFromFile < minValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, minValue, minValue, maxValue);
		return minValue;
	} 
	else if (iniValueReadFromFile > maxValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, maxValue, minValue, maxValue);
		return maxValue;
	}
	return iniValueReadFromFile;
}



//float CIniReader::ReadDouble(const STR8	szSection, const STR8	szKey, float fltDefaultValue)
//{
// char szResult[255];
// char szDefault[255];
// float fltResult;
// sprintf(szDefault, "%f",fltDefaultValue);
// GetPrivateProfileString(szSection,	szKey, szDefault, szResult, 255, m_szFileName);
// fltResult = (float) atof(szResult);
// return fltResult;
//}

double CIniReader::ReadDouble(const STR8 szSection, const STR8 szKey, double defaultValue, double minValue, double maxValue)
{
	std::string text;
	double iniValueReadFromFile = defaultValue;
	if (ReadPropertyText(m_oProps, szSection, szKey, text))
	{
		double parsed = 0.0;
		if (UtilsDataBoundaryModel::ParseDouble(text, parsed))
			iniValueReadFromFile = parsed;
	}
	//AssertGE(iniValueReadFromFile, minValue);
	//AssertLE(iniValueReadFromFile, maxValue);
	if (iniValueReadFromFile < minValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey,iniValueReadFromFile, minValue, minValue, maxValue);
		return minValue;
	}
	else if (iniValueReadFromFile > maxValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, maxValue, minValue, maxValue);
		return maxValue;
	}
	return iniValueReadFromFile;
}

FLOAT CIniReader::ReadFloat(const STR8 szSection, const STR8 szKey, FLOAT defaultValue, FLOAT minValue, FLOAT maxValue)
{
	std::string text;
	FLOAT iniValueReadFromFile = defaultValue;
	if (ReadPropertyText(m_oProps, szSection, szKey, text))
	{
		double parsed = 0.0;
		if (UtilsDataBoundaryModel::ParseDouble(text, parsed) &&
			parsed >= -std::numeric_limits<FLOAT>::max() &&
			parsed <= std::numeric_limits<FLOAT>::max())
		{
			const FLOAT narrowed = static_cast<FLOAT>(parsed);
			if (parsed == 0.0 || narrowed != 0.0f)
				iniValueReadFromFile = narrowed;
		}
	}

	//AssertGE(iniValueReadFromFile, minValue);
	//AssertLE(iniValueReadFromFile, maxValue);

	if (iniValueReadFromFile < minValue) 
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, minValue, minValue, maxValue);
		return minValue;
	}
	else if (iniValueReadFromFile > maxValue)
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, maxValue, minValue, maxValue);
		return maxValue;
	}
	return iniValueReadFromFile;
}

void CIniReader::ReadFloatArray(const STR8 szSection, const STR8 szKey, std::vector<FLOAT>& vec)
{
	std::string text;
	std::vector<FLOAT> staged;
	if (!ReadPropertyText(m_oProps, szSection, szKey, text) ||
		!UtilsDataBoundaryModel::ParseFloatList(text, staged))
	{
		std::stringstream errMessage;
		errMessage << "There was an error reading array [" << szSection << "][" << szKey << "] in file [" << m_szFileName << "]. Defaulting to [0].";
		iniErrorMessages.push(errMessage.str());
		vec.assign(1, 0);
		return;
	}
	vec = std::move(staged);
}

void CIniReader::ReadINT32Array(const STR8 szSection, const STR8 szKey, std::vector<INT32>& vec)
{
	std::string text;
	std::vector<INT32> staged;
	if (!ReadPropertyText(m_oProps, szSection, szKey, text) ||
		!UtilsDataBoundaryModel::ParseInt32List(text, staged))
	{
		std::stringstream errMessage;
		errMessage << "There was an error reading array [" << szSection << "][" << szKey << "] in file [" << m_szFileName << "]. Defaulting to [0].";
		iniErrorMessages.push(errMessage.str());
		vec.assign(1, 0);
		return;
	}
	vec = std::move(staged);
}

BOOLEAN CIniReader::ReadBoolean(const STR8 szSection, const STR8 szKey, bool defaultValue, bool bolDisplayError)
{
	vfs::String str = m_oProps.getStringProperty(
		SafeString(szSection), SafeString(szKey), L"");
	if( vfs::StrCmp::Equal(str, L"true") )
	{
		return TRUE;
	}
	else if( vfs::StrCmp::Equal(str, L"false") )
	{
		return FALSE;
	}
	std::string szResult = str.utf8();
	const char* szDefault = defaultValue ? "TRUE" : "FALSE";

	if(bolDisplayError){
		std::stringstream errMessage;
		errMessage << "The value [" << szSection << "][" << szKey << "] = \"" << szResult << "\" "
			<< "in file [" << this->m_szFileName << "] is neither TRUE nor FALSE.  The value " << szDefault << " will be used.";
		iniErrorMessages.push(errMessage.str());
	}
	return defaultValue;
}

// ary-05/15/2009 : snippet on how to use CIniReader::ReadString
//	CHAR8 test_ini_string[255]{};
//	iniReader.ReadString("JA2 Game Settings", "TEST_STRING", "default string",
//		test_ini_string, std::size(test_ini_string));

void CIniReader::ReadString(const char* szSection, const char* szKey, const char* szDefaultValue, CHAR8 *input_buffer, size_t buffer_size)
{
	std::string value;
	if (!ReadPropertyText(m_oProps, szSection, szKey, value))
		value = SafeString(szDefaultValue);
	UtilsDataBoundaryModel::CopyString(input_buffer, buffer_size, value);
}

// WANNE - MP: Old version, currently used by Multiplayer
CHAR8 *	CIniReader::ReadString(const char* szSection, const char* szKey, const char* szDefaultValue)
{
	ReadString(szSection, szKey, szDefaultValue,
		m_legacyStringBuffer.data(), m_legacyStringBuffer.size());
	return m_legacyStringBuffer.data();
}

UINT8  CIniReader::ReadUINT8(const STR8 szSection, const STR8 szKey, UINT8  defaultValue, UINT8  minValue, UINT8  maxValue)
{
	UINT8 iniValueReadFromFile;


	iniValueReadFromFile = (UINT8) this->ReadUINT( szSection,  szKey, (UINT32) defaultValue, (UINT32) minValue, (UINT32) maxValue);

	return iniValueReadFromFile;

}

UINT16 CIniReader::ReadUINT16(const STR8 szSection, const STR8 szKey, UINT16 defaultValue, UINT16 minValue, UINT16 maxValue)
{
	UINT16 iniValueReadFromFile;

	iniValueReadFromFile = (UINT16) this->ReadUINT( szSection,  szKey, (UINT32) defaultValue, (UINT32) minValue, (UINT32) maxValue);

	return iniValueReadFromFile;

}

UINT32 CIniReader::ReadUINT32(const STR8 szSection, const STR8 szKey, UINT32 defaultValue, UINT32 minValue, UINT32 maxValue)
{
	UINT32 iniValueReadFromFile;

	iniValueReadFromFile = (UINT32) this->ReadUINT( szSection,  szKey, (UINT32) defaultValue, (UINT32) minValue, (UINT32) maxValue);

	return iniValueReadFromFile;

}

UINT32 CIniReader::ReadUINT(const STR8 szSection, const STR8 szKey, UINT32 defaultValue, UINT32 minValue, UINT32 maxValue )
{
	std::string text;
	std::uint64_t parsed = 0;
	UINT32 iniValueReadFromFile = defaultValue;
	if (ReadPropertyText(m_oProps, szSection, szKey, text) &&
		UtilsDataBoundaryModel::ParseUInt64(text, parsed) &&
		parsed <= std::numeric_limits<UINT32>::max())
	{
		iniValueReadFromFile = static_cast<UINT32>(parsed);
	}
	//AssertGE(iniValueReadFromFile, minValue);
	//AssertLE(iniValueReadFromFile, maxValue);

	if (iniValueReadFromFile < minValue) 
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, minValue, minValue, maxValue);
		iniValueReadFromFile = minValue;
	} 
	else if (iniValueReadFromFile > maxValue) 
	{
		PushErrorMessage(this->m_szFileName, szSection, szKey, iniValueReadFromFile, maxValue, minValue, maxValue);
		iniValueReadFromFile = maxValue;
	}

	return iniValueReadFromFile;
}
