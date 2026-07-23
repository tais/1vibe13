#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "LuaInitNPCs.h"
	#include "email.h"
	#include "InterfaceItemImages.h"
	#include "Soldier Profile.h"
	#include "mercs.h"
	#include "Encrypted File.h"
	#include "GameSettings.h"
	#include "Text.h"

#include "XML_Language.h"

typedef enum
{
	LANGUAGE_ELEMENT_NONE = 0,
	LANGUAGE_ELEMENT_LIST,
	LANGUAGE_ELEMENT,
	LANGUAGE_ELEMENT_PROPERTY,
	LANGUAGE_ELEMENT_SUBLIST,
	LANGUAGE_ELEMENT_SUBLIST_PROPERTY,
	
	
} LANGUAGE_PARSE_STAGE;

LANGUAGE_LOCATION zlanguageText[1000];

typedef struct
{
	LANGUAGE_PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	LANGUAGE_LOCATION	curLanguageData;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
	LANGUAGE_LOCATION *destination;
	UINT32			fileType;
}languageLocationParseData;


BOOLEAN LanguageLocation_TextOnly;

LANGUAGE_LOCATION *pLang;

UINT32 FileTypeXml = 0;

static void XMLCALL
languageLocationStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	languageLocationParseData * pData = (languageLocationParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "MESSAGES") == 0 && pData->curElement == LANGUAGE_ELEMENT_NONE)
		{
			pData->curElement = LANGUAGE_ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "TEXT") == 0 && pData->curElement == LANGUAGE_ELEMENT_LIST)
		{
			pData->curElement = LANGUAGE_ELEMENT;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == LANGUAGE_ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
			    strcmp(name, "Message") == 0 ))
		{
			pData->curElement = LANGUAGE_ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
languageLocationCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	languageLocationParseData * pData = (languageLocationParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}

static void XMLCALL
languageLocationEndElementHandle(void *userData, const XML_Char *name)
{
	languageLocationParseData * pData = (languageLocationParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "MESSAGES") == 0)
		{
			pData->curElement = LANGUAGE_ELEMENT_NONE;
		}
		else if(strcmp(name, "TEXT") == 0)
		{
			pData->curElement = LANGUAGE_ELEMENT_LIST;	
			
			if (!LanguageLocation_TextOnly )
				{
					pLang[pData->curLanguageData.uiIndex].uiIndex = pData->curLanguageData.uiIndex;
					
					if ( FileTypeXml == 0 )
						wcscpy(XMLTacticalMessages[pData->curLanguageData.uiIndex], pData->curLanguageData.Message);	
				}
				else
				{
					if ( FileTypeXml == 0 )
						wcscpy(XMLTacticalMessages[pData->curLanguageData.uiIndex], pData->curLanguageData.Message);
				}		
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = LANGUAGE_ELEMENT;
			pData->curLanguageData.uiIndex	= (UINT32) strtoul(pData->szCharData, NULL, 0);
		}
		else if(strcmp(name, "Message") == 0 )
		{
			pData->curElement = LANGUAGE_ELEMENT;

			MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, pData->curLanguageData.Message, sizeof(pData->curLanguageData.Message)/sizeof(pData->curLanguageData.Message[0]) );
			pData->curLanguageData.Message[sizeof(pData->curLanguageData.Message)/sizeof(pData->curLanguageData.Message[0]) - 1] = '\0';
		}
		
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

static void PrepareLanguageDocument(void *userData)
{
	languageLocationParseData *pData =
		(languageLocationParseData *)userData;
	pLang = pData->destination;
	FileTypeXml = pData->fileType;
}

BOOLEAN ReadInLanguageLocation(STR fileName, BOOLEAN localizedVersion, LANGUAGE_LOCATION *Lang, UINT32 FileType2 )
{
	languageLocationParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading NewTacticalMessages.xml" );

	LanguageLocation_TextOnly = localizedVersion;

	memset(&pData,0,sizeof(pData));
	pData.destination = Lang;
	pData.fileType = FileType2;

	const LegacyXmlCallbacks callbacks{
		&pData, languageLocationStartElementHandle, languageLocationEndElementHandle,
		languageLocationCharacterDataHandle, PrepareLanguageDocument};
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

	return( TRUE );
}
