#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include <algorithm>
#include <vector>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "LuaInitNPCs.h"
	#include "email.h"
	#include "InterfaceItemImages.h"
	#include "Soldier Profile.h"
	#include "aim.h"
	#include "mercs.h"
	#include "Encrypted File.h"
	#include "GameSettings.h"

#include "BriefingRoom_Data.h"

typedef enum
{
	ENCYCLOPEDIA_ELEMENT_NONE = 0,
	ENCYCLOPEDIA_ELEMENT_LIST,
	ENCYCLOPEDIA_ELEMENT,
	ENCYCLOPEDIA_ELEMENT_PROPERTY,
	ENCYCLOPEDIA_ELEMENT_SUBLIST,
	ENCYCLOPEDIA_ELEMENT_SUBLIST_PROPERTY,
	
	
} ENCYCLOPEDIA_PARSE_STAGE;

typedef struct
{
	ENCYCLOPEDIA_PARSE_STAGE	curElement;

	CHAR8		szCharData[ENCYCLOPEDIA_DECRIPTION_SIZE+1];
	BRIEFINGROOM_M_DATA	curEncyclopediaData;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
	BRIEFINGROOM_M_DATA *destination;
	std::vector<bool>* seen;
	UINT32			destinationSize;
	UINT32			fileType;
	BOOLEAN		localizedVersion;
	bool			valid;
	bool			hasIndex;
}encyclopediaLocationParseData;

//extern BOOLEAN LoadMercBioInfo(UINT8 ubIndex, STR16 pInfoString, STR16 pAddInfo);

UINT32 MaxPage = 0;
UINT8 mID = 0;
UINT8			gMercArray[ NUM_PROFILES ];

#define	SIZE_MERC_BIO_INFO	400	* 2
#define SIZE_MERC_ADDITIONAL_INFO 160 * 2
#define	MERCBIOSFILENAME	"BINARYDATA\\MercBios.edt"
#define	AIMBIOSFILENAME		"BINARYDATA\\aimbios.edt"


CHAR16		MercInfoString[ SIZE_MERC_BIO_INFO ];
CHAR16		AdditionalInfoString[ SIZE_MERC_BIO_INFO ];

BOOLEAN bMERC;

BOOLEAN LoadEncyclopediaMercBio( UINT8 ubIndex, CHAR16 *pInfoString, CHAR16 *pAddInfo, UINT32 Type )
{
	HWFILE		hFile=0;
	UINT32		uiBytesRead;
	//UINT16		i;
	UINT32		uiStartSeekAmount;

	if ( Type == 1 )
	hFile = FileOpen(AIMBIOSFILENAME, FILE_ACCESS_READ, FALSE);
	else if ( Type == 2 )
	hFile = FileOpen(MERCBIOSFILENAME, FILE_ACCESS_READ, FALSE);
	
	if ( !hFile )
	{
		return( FALSE );
	}


	// Get current mercs bio info
	uiStartSeekAmount = (SIZE_MERC_BIO_INFO + SIZE_MERC_ADDITIONAL_INFO) * ubIndex;

	if ( FileSeek( hFile, uiStartSeekAmount, FILE_SEEK_FROM_START ) == FALSE )
	{
		FileClose(hFile);
		return( FALSE );
	}

	if( !FileRead( hFile, pInfoString, SIZE_MERC_BIO_INFO, &uiBytesRead) ||
		uiBytesRead != SIZE_MERC_BIO_INFO )
	{
		FileClose(hFile);
		return( FALSE );
	}

	DecodeString(pInfoString, SIZE_MERC_BIO_INFO / 2);

	// Get the additional info
	uiStartSeekAmount = ((SIZE_MERC_BIO_INFO + SIZE_MERC_ADDITIONAL_INFO) * ubIndex )+ SIZE_MERC_BIO_INFO ;
	if ( FileSeek( hFile, uiStartSeekAmount, FILE_SEEK_FROM_START ) == FALSE )
	{
		FileClose(hFile);
		return( FALSE );
	}

	if( !FileRead( hFile, pAddInfo, SIZE_MERC_ADDITIONAL_INFO, &uiBytesRead) ||
		uiBytesRead != SIZE_MERC_ADDITIONAL_INFO )
	{
		FileClose(hFile);
		return( FALSE );
	}

	DecodeString(pAddInfo, SIZE_MERC_ADDITIONAL_INFO / 2);

	FileClose(hFile);
	return(TRUE);
}

BOOLEAN LoadGraphicForItem( BRIEFINGROOM_M_DATA *pEncy, UINT32 i )
{
	CHAR8	 zName[ 100 ];
//	UINT32	uiVo;
	UINT16		ubGraphic, ubGraphicType;
	CHAR8	zString[512]; 
//	CHAR8	zString2[512];

	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("LoadTileGraphicForItem"));

	const char* ext = g_bUsePngItemImages ? ".png" : ".sti";

	// CHECK SUBCLASS
	ubGraphic = Item[i].ubGraphicNum;
	ubGraphicType = Item[i].ubGraphicType;

	if ( Item[i].ubGraphicType == 0 )
	{
		// CHECK SUBCLASS
		//ubGraphic++;

		if ( ubGraphic < 10 )
		{
			sprintf( zName, "gun0%d", ubGraphic );
		}
		else
		{
			sprintf( zName, "gun%d", ubGraphic );
		}
	}
	else
	{
		if ( ubGraphic < 10 )
		{
			sprintf( zName, "p%ditem0%d", ubGraphicType, ubGraphic );
		}
		else
		{
			sprintf( zName, "p%ditem%d", ubGraphicType, ubGraphic );
		}
	}

	//Load item
	sprintf( zString, "BIGITEMS\\%s%s", zName, ext );
	
	


	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("LoadTileGraphicForItem: done"));

	return( TRUE );
}

static void XMLCALL
encyclopediaLocationStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	encyclopediaLocationParseData * pData = (encyclopediaLocationParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "BRIEFINGROOM") == 0 && pData->curElement == ENCYCLOPEDIA_ELEMENT_NONE)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "DATA") == 0 && pData->curElement == ENCYCLOPEDIA_ELEMENT_LIST)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;
			pData->curEncyclopediaData = BRIEFINGROOM_M_DATA{};
			pData->hasIndex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ENCYCLOPEDIA_ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
			    strcmp(name, "Name") == 0 ||
				strcmp(name, "Hidden") == 0 ||
				strcmp(name, "MaxPages") == 0 ||
				strcmp(name, "MaxImages") == 0 ||
				strcmp(name, "ImagePositionX") == 0 ||
				strcmp(name, "ImagePositionY") == 0 ||
				strcmp(name, "SecretCode") == 0 ||
				strcmp(name, "NextMission") == 0 ))
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
encyclopediaLocationCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	encyclopediaLocationParseData * pData = (encyclopediaLocationParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}

static void XMLCALL
encyclopediaLocationEndElementHandle(void *userData, const XML_Char *name)
{
	encyclopediaLocationParseData * pData = (encyclopediaLocationParseData *)userData;
	
//	char temp;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "BRIEFINGROOM") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT_NONE;
		}
		else if(strcmp(name, "DATA") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT_LIST;
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->destinationSize,
					pData->curEncyclopediaData.uiIndex) ||
				pData->curEncyclopediaData.MaxPages > MAX_PAGES ||
				pData->curEncyclopediaData.MaxImages < 0 ||
				pData->curEncyclopediaData.MaxImages > MAX_IMAGES)
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curEncyclopediaData.uiIndex;
				if ((*pData->seen)[index])
				{
					pData->valid = false;
				}
				else if (pData->valid && pData->localizedVersion)
				{
					pData->valid = LaptopLocalizationModel::CopyText(
						pData->destination[index].Name,
						pData->curEncyclopediaData.Name);
					(*pData->seen)[index] = pData->valid;
				}
				else if (pData->valid)
				{
					auto record = pData->curEncyclopediaData;
					record.Hidden = !record.Hidden;
					record.NextMission = pData->fileType == 4 ?
						record.NextMission : -1;
					record.MissionID = index;
					pData->destination[index] = record;
					(*pData->seen)[index] = true;
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curEncyclopediaData.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "Name") == 0 )
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;

			pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curEncyclopediaData.Name);
		}
		else if(strcmp(name, "Hidden") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseBoolean(
					pData->szCharData, pData->curEncyclopediaData.Hidden);
		}
		else if(strcmp(name, "MaxPages") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curEncyclopediaData.MaxPages);
		}
		else if(strcmp(name, "MaxImages") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curEncyclopediaData.MaxImages);
		}
		else if(strcmp(name, "ImagePositionX") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData,
					pData->curEncyclopediaData.sImagePositionX[0]);
		}
		else if(strcmp(name, "ImagePositionY") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData,
					pData->curEncyclopediaData.sImagePositionY[0]);
		}
		else if(strcmp(name, "SecretCode") == 0 )
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;

			pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curEncyclopediaData.sCode);
		}
		else if(strcmp(name, "NextMission") == 0)
		{
			pData->curElement = ENCYCLOPEDIA_ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curEncyclopediaData.NextMission);
		}		
		
		
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInBriefingRoom(STR fileName, BOOLEAN localizedVersion,
	BRIEFINGROOM_M_DATA *Ency, UINT32 destinationSize, UINT32 fileType)
{
	if (!Ency || destinationSize == 0)
		return FALSE;
	encyclopediaLocationParseData pData{};
	std::vector<BRIEFINGROOM_M_DATA> pending(
		Ency, Ency + destinationSize);
	std::vector<bool> seen(destinationSize, false);

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading BriefingRoom.xml" );

	pData.destination = pending.data();
	pData.seen = &seen;
	pData.destinationSize = destinationSize;
	pData.fileType = fileType;
	pData.localizedVersion = localizedVersion;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, encyclopediaLocationStartElementHandle,
		encyclopediaLocationEndElementHandle,
		encyclopediaLocationCharacterDataHandle};
	const LegacyXmlResult result =
		ParseLegacyXmlFile(fileName, callbacks);
	if (!result)
	{
		if (result.status == LegacyXmlStatus::NotFound)
			return localizedVersion;
		if (result.status != LegacyXmlStatus::ReadError)
		{
			const auto message = FormatLegacyXmlFailure(fileName, result);
			LiveMessage(message.data());
		}
		return FALSE;
	}
	if (!pData.valid)
		return FALSE;

	std::copy(pending.begin(), pending.end(), Ency);

	return( TRUE );
}
