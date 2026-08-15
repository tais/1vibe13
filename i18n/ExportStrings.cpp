#include "ExportStrings.h"
#include "SelectedCatalogExport.h"
#include "LocalizedStrings.h"
#include "Map Screen Interface.h"
#include "personnel.h"
#include "soldier profile type.h"
#include "Interface.h"
#include "Keys.h"
#include "Merc Contract.h"
#include "Campaign Types.h"
#include "GameSettings.h"
#include "finances.h"
#include "laptop.h"
#include <language.hpp>

#include <string>

#include <vfs/Core/vfs_string.h>
#include <vfs/Tools/vfs_tools.h>
#include <vfs/Tools/vfs_parser_tools.h>
#include <vfs/Tools/vfs_property_container.h>

namespace
{
	// vfs::PropertyContainer::{write,init}FromXMLFile takes its TagMap
	// argument by non-const reference. MSVC accepted a TagMap() rvalue;
	// clang doesn't. Bind a single per-TU empty instance and reuse it.
	vfs::PropertyContainer::TagMap& EmptyTagMap()
	{
		static vfs::PropertyContainer::TagMap m;
		return m;
	}

	class PropertyContainerExportSink final
		: public i18n::SelectedCatalogExportSink
	{
	public:
		explicit PropertyContainerExportSink(vfs::PropertyContainer& props)
			: props_(props)
		{
		}

		void copyEntry(std::wstring_view section, int index,
			std::wstring_view text) override
		{
			// The adapter's views borrow mutable legacy globals. Materialize both
			// values in this call; PropertyContainer never observes their lifetime.
			props_.setStringProperty(vfs::String(std::wstring(section)),
				vfs::toString<wchar_t>(index),
				vfs::String(std::wstring(text)));
		}

	private:
		vfs::PropertyContainer& props_;
	};
}

namespace Loc
{
	bool Translate(vfs::String::char_t* str, int len, i18n::Lang lang);

	void ExportMercBio();
	void ExportAIMHistory();
	void ExportAIMPolicy();
	void ExportAlumniName();
	void ExportDialogues();
	void ExportNPCDialogues();
};

//////////////////////////////////////////////////////////

#include "Text.h"

#include "Assignments.h"
#include "history.h"

bool Loc::ExportStrings()
{
	vfs::PropertyContainer::TagMap tmap;
	vfs::PropertyContainer props;
	PropertyContainerExportSink sink(props);

	i18n::ExportSelectedCatalog(sink);

	props.writeToXMLFile(L"Localization/GameStrings.xml",tmap);
	props.writeToIniFile(L"Localization/GameStrings.ini",true);

	Loc::ExportMercBio();
	Loc::ExportAIMHistory();
	Loc::ExportAIMPolicy();
	Loc::ExportAlumniName();
	Loc::ExportDialogues();
	Loc::ExportNPCDialogues();

	return true;
}

#include <vfs/Core/vfs_file_raii.h>
#include "Encrypted File.h"

namespace Loc
{
	wchar_t ToPolish(wchar_t siChar)
	{
		switch( siChar )
		{
			case 165:          siChar = 260;          break;
			case 198:          siChar = 262;          break;
			case 202:          siChar = 280;          break;
			case 163:          siChar = 321;          break;
			case 209:          siChar = 323;          break;
			case 211:          siChar = 211;          break;

			case 140:          siChar = 346;          break;
			case 175:          siChar = 379;          break;
			case 143:          siChar = 377;          break;
			case 185:          siChar = 261;          break;
			case 230:          siChar = 263;          break;
			case 234:          siChar = 281;          break;

			case 179:          siChar = 322;          break;
			case 241:          siChar = 324;          break;
			case 243:          siChar = 243;          break;
			case 156:          siChar = 347;          break;
			case 191:          siChar = 380;          break;
			case 159:          siChar = 378;          break;
		}
		return siChar;
	}

	wchar_t ToRussian(wchar_t siChar)
	{
		switch( siChar )
		{
			//capital letters
			case 168:          siChar = 1025;  break;	//U+0401		   d0 81     CYRILLIC CAPITAL LETTER IO
			case 192:          siChar = 1040;  break; //U+0410     A     d0 90     CYRILLIC CAPITAL LETTER A
			case 193:          siChar = 1041;  break;
			case 194:          siChar = 1042;  break;
			case 195:          siChar = 1043;  break;
			case 196:          siChar = 1044;  break;
			case 197:          siChar = 1045;  break;
			case 198:          siChar = 1046;  break;
			case 199:          siChar = 1047;  break;
			case 200:          siChar = 1048;  break;
			case 201:          siChar = 1049;  break;
			case 202:          siChar = 1050;  break;
			case 203:          siChar = 1051;  break;
			case 204:          siChar = 1052;  break;
			case 205:          siChar = 1053;  break;
			case 206:          siChar = 1054;  break;
			case 207:          siChar = 1055;  break;
			case 208:          siChar = 1056;  break;
			case 209:          siChar = 1057;  break;
			case 210:          siChar = 1058;  break;
			case 211:          siChar = 1059;  break;
			case 212:          siChar = 1060;  break;
			case 213:          siChar = 1061;  break;
			case 214:          siChar = 1062;  break;
			case 215:          siChar = 1063;  break;
			case 216:          siChar = 1064;  break;
			case 217:          siChar = 1065;  break;
			case 218:          siChar = 1066;  break;
			case 219:          siChar = 1067;  break;
			case 220:          siChar = 1068;  break;
			case 221:          siChar = 1069;  break;
			case 222:          siChar = 1070;  break;
			case 223:          siChar = 1071;  break; //U+042F           d0 af     CYRILLIC CAPITAL LETTER YA

			//small letters
			case 185:          siChar = 8470;  break;		// ¹
			case 178:          siChar = 1030;  break;		// ²
			case 161:          siChar = 1038;  break;		// ¡
			case 179:          siChar = 1110;  break;		// ³
			case 162:          siChar = 1118;  break;		// ¢
			case 165:          siChar = 1168;  break;		// ¥
			case 170:          siChar = 1028;  break;		// ª
			case 175:          siChar = 1031;  break;		// ¯
			case 180:          siChar = 1169;  break;		// ´
			case 186:          siChar = 1108;  break;		// º
			case 191:          siChar = 1111;  break;		// ¿

			case 184:          siChar = 1105;  break; //U+0451           d1 91     CYRILLIC SMALL LETTER IO
			case 224:          siChar = 1072;  break; //U+0430     a     d0 b0     CYRILLIC SMALL LETTER A
			case 225:          siChar = 1073;  break;
			case 226:          siChar = 1074;  break;
			case 227:          siChar = 1075;  break;
			case 228:          siChar = 1076;  break;
			case 229:          siChar = 1077;  break;
			case 230:          siChar = 1078;  break;
			case 231:          siChar = 1079;  break;
			case 232:          siChar = 1080;  break;
			case 233:          siChar = 1081;  break;
			case 234:          siChar = 1082;  break;
			case 235:          siChar = 1083;  break;
			case 236:          siChar = 1084;  break;
			case 237:          siChar = 1085;  break;
			case 238:          siChar = 1086;  break;
			case 239:          siChar = 1087;  break; //U+043F           d0 bf     CYRILLIC SMALL LETTER PE
			case 240:          siChar = 1088;  break; //U+0440     p     d1 80     CYRILLIC SMALL LETTER ER
			case 241:          siChar = 1089;  break;
			case 242:          siChar = 1090;  break;
			case 243:          siChar = 1091;  break;
			case 244:          siChar = 1092;  break;
			case 245:          siChar = 1093;  break;
			case 246:          siChar = 1094;  break;
			case 247:          siChar = 1095;  break;
			case 248:          siChar = 1096;  break;
			case 249:          siChar = 1097;  break;
			case 250:          siChar = 1098;  break;
			case 251:          siChar = 1099;  break;
			case 252:          siChar = 1100;  break;
			case 253:          siChar = 1101;  break;
			case 254:          siChar = 1102;  break;
			case 255:          siChar = 1103;  break; //U+044F           d1 8f     CYRILLIC SMALL LETTER YA                
		}
		return siChar;
	}
	bool Translate(vfs::String::char_t* str, int len, i18n::Lang lang)
	{
		if(lang == i18n::Lang::en || lang == i18n::Lang::de)
		{
			return true;
		}
		else if(lang == i18n::Lang::ru)
		{
			for(int i=0; i<len; i++) str[i] = ToRussian(str[i]);
			return true;
		}
		else if(lang == i18n::Lang::pl)
		{
			for(int i=0; i<len; i++) str[i] = ToPolish(str[i]);
			return true;
		}
		return false;
	}
}; // namespace Loc

void Loc::ExportMercBio()
{
	const i18n::Lang lang = g_lang;
	#define	SIZE_MERC_BIO_INFO				400	* 2
	#define SIZE_MERC_ADDITIONAL_INFO		160 * 2

	vfs::String::char_t pInfoString[SIZE_MERC_BIO_INFO];
	vfs::String::char_t pAddInfo[SIZE_MERC_ADDITIONAL_INFO];
	vfs::COpenReadFile rfile("BINARYDATA\\aimbios.edt");
	vfs::tReadableFile& file = rfile.file();

	vfs::PropertyContainer props; 
	for(int i=0; i<40; ++i)
	{
		memset(pInfoString,0,SIZE_MERC_BIO_INFO*sizeof(wchar_t));
		memset(pAddInfo,0,SIZE_MERC_ADDITIONAL_INFO*sizeof(wchar_t));
		//
		file.read((vfs::Byte*)pInfoString, SIZE_MERC_BIO_INFO);
		DecodeString(pInfoString,SIZE_MERC_BIO_INFO);
		Loc::Translate(pInfoString, SIZE_MERC_BIO_INFO, lang);
		props.setStringProperty(L"Bio", vfs::toString<wchar_t>(i), pInfoString);
		
		file.read((vfs::Byte*)pAddInfo, SIZE_MERC_ADDITIONAL_INFO);
		DecodeString(pAddInfo, SIZE_MERC_ADDITIONAL_INFO);
		Loc::Translate(pAddInfo, SIZE_MERC_ADDITIONAL_INFO, lang);
		props.setStringProperty(L"Add", vfs::toString<wchar_t>(i), pAddInfo);
	}
	props.writeToXMLFile(L"Localization/AimBiographies.xml", EmptyTagMap());
}

void Loc::ExportAIMHistory()
{
	const i18n::Lang lang = g_lang;
	#define AIM_HISTORY_LINE_SIZE 400 * 2
	vfs::String::char_t pHistLine[AIM_HISTORY_LINE_SIZE];
	vfs::COpenReadFile rfile("BINARYDATA\\AimHist.edt");
	vfs::tReadableFile& file = rfile.file();

	vfs::PropertyContainer props; 
	for(int i=0; i<23; ++i)
	{
		memset(pHistLine,0,AIM_HISTORY_LINE_SIZE*sizeof(wchar_t));
		//
		file.read((vfs::Byte*)pHistLine, AIM_HISTORY_LINE_SIZE);
		DecodeString(pHistLine,AIM_HISTORY_LINE_SIZE);
		Loc::Translate(pHistLine, AIM_HISTORY_LINE_SIZE, lang);
		props.setStringProperty(L"Line", vfs::toString<wchar_t>(i), pHistLine);
	}
	props.writeToXMLFile(L"Localization/AimHistory.xml", EmptyTagMap());
}
	
	
void Loc::ExportAIMPolicy()
{
	const i18n::Lang lang = g_lang;
	#define AIM_HISTORY_LINE_SIZE 400 * 2
	vfs::String::char_t pPolLine[AIM_HISTORY_LINE_SIZE];
	vfs::COpenReadFile rfile("BINARYDATA\\AimPol.edt");
	vfs::tReadableFile& file = rfile.file();

	vfs::PropertyContainer props; 
	for(int i=0; i<46; ++i)
	{
		memset(pPolLine,0,400*sizeof(wchar_t));
		//
		file.read((vfs::Byte*)pPolLine, AIM_HISTORY_LINE_SIZE);
		DecodeString(pPolLine,AIM_HISTORY_LINE_SIZE);
		Loc::Translate(pPolLine, AIM_HISTORY_LINE_SIZE, lang);
		props.setStringProperty(L"Line", vfs::toString<wchar_t>(i), pPolLine);
	}
	props.writeToXMLFile(L"Localization/AimPolicy.xml", EmptyTagMap());
}

void Loc::ExportAlumniName()
{
	const i18n::Lang lang = g_lang;
	#define AIM_ALUMNI_NAME_SIZE 80 * 2
	vfs::String::char_t pAlumniName[AIM_ALUMNI_NAME_SIZE];
	vfs::COpenReadFile rfile("BINARYDATA\\AlumName.edt");
	vfs::tReadableFile& file = rfile.file();

	vfs::PropertyContainer props; 
	for(int i=0; i<51; ++i)
	{
		memset(pAlumniName,0,AIM_ALUMNI_NAME_SIZE*sizeof(wchar_t));
		//
		file.read((vfs::Byte*)pAlumniName, AIM_ALUMNI_NAME_SIZE);
		DecodeString(pAlumniName,AIM_ALUMNI_NAME_SIZE);
		Loc::Translate(pAlumniName, AIM_ALUMNI_NAME_SIZE, lang);
		props.setStringProperty(L"Line", vfs::toString<wchar_t>(i), pAlumniName);
	}
	props.writeToXMLFile(L"Localization/AlumniName.xml", EmptyTagMap());
}

#include <vfs/Core/vfs.h>

void Loc::ExportDialogues()
{
	const i18n::Lang lang = g_lang;
	#define DIALOGUESIZE		480
	vfs::String::char_t pDiagLine[DIALOGUESIZE];

	vfs::CVirtualFileSystem::Iterator it = getVFS()->begin(L"MercEdt/*.edt");
	for(; !it.end(); it.next())
	{
		vfs::PropertyContainer props;
		vfs::COpenReadFile rfile(it.value());
		vfs::tReadableFile& file = rfile.file();

		std::wstringstream wss;
		wss.str(file.getName().c_str());
		int id=0;
		wss >> id;

		for(int i=0; i<200; ++i)
		{
			memset(pDiagLine,0,DIALOGUESIZE*sizeof(wchar_t));
			//
			if(file.read((vfs::Byte*)pDiagLine, DIALOGUESIZE) > 0)
			{
				DecodeString(pDiagLine,DIALOGUESIZE);
				Loc::Translate(pDiagLine, DIALOGUESIZE, lang);
				if(wcslen(pDiagLine))
				{
					props.setStringProperty(vfs::toString<wchar_t>(id),vfs::toString<wchar_t>(i), pDiagLine);
				}
			}
		}
		vfs::Path x(L"Localization/Dialogue");
		x += vfs::Path(file.getName().c_wcs() + L".xml");
		props.writeToXMLFile(x, EmptyTagMap());
	}
}

void Loc::ExportNPCDialogues()
{
	const i18n::Lang lang = g_lang;
	#define DIALOGUESIZE		480
	#define CIVQUOTESIZE		320
	vfs::String::char_t pDiagLine[DIALOGUESIZE];

	vfs::CVirtualFileSystem::Iterator it = getVFS()->begin(L"npcdata/*.edt");
	for(; !it.end(); it.next())
	{
		vfs::PropertyContainer props;
		vfs::COpenReadFile rfile(it.value());
		vfs::tReadableFile& file = rfile.file();

		vfs::String::str_t const& ws = file.getName().c_wcs();
		vfs::String::str_t::size_type pos = ws.find_first_of(L".");
		vfs::String id = ws.substr(0,pos);

		int SIZE;
		if(vfs::matchPattern(L"civ*", id))
		{
			SIZE = CIVQUOTESIZE;
		}
		else
		{
			SIZE = DIALOGUESIZE;
		}

		for(int i=0; i<200; ++i)
		{
			memset(pDiagLine,0,DIALOGUESIZE*sizeof(wchar_t));
			//
			if(file.read((vfs::Byte*)pDiagLine, SIZE) > 0)
			{
				DecodeString(pDiagLine,SIZE);
				Loc::Translate(pDiagLine, SIZE, lang);
				if(wcslen(pDiagLine))
				{
					props.setStringProperty(id,vfs::toString<wchar_t>(i), pDiagLine);
				}
			}
		}
		vfs::Path x(L"Localization/NpcDialogue");
		x += vfs::Path(file.getName().c_wcs() + L".xml");
		props.writeToXMLFile(x, EmptyTagMap());
	}
}
