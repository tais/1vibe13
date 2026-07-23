#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "Intro.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	INTRO_NAMES_VALUES	curIntroNames;
	INTRO_NAMES_VALUES *	curArray;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
	//CHAR16 gzEnemyNames[MAX_ENEMY_NAMES_CHARS];
}
typedef introNamesParseData;

BOOLEAN IntroName_TextOnly;

static void XMLCALL
introNamesStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	introNamesParseData * pData = (introNamesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "INTRO_FILES") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "FILE") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "szFile") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
introNamesCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	introNamesParseData * pData = (introNamesParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
introNamesEndElementHandle(void *userData, const XML_Char *name)
{
	introNamesParseData * pData = (introNamesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "VIDEO_FILES") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "FILE") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!IntroName_TextOnly)
				{
					wcscpy(zVideoFile[pData->curIntroNames.uiIndex].szFile, pData->curIntroNames.szFile);
				}
				else
				{
					wcscpy(zVideoFile[pData->curIntroNames.uiIndex].szFile, pData->curIntroNames.szFile);
				}		
		
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curIntroNames.uiIndex	= (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "szFile") == 0 )
		{
			pData->curElement = ELEMENT;

			MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, pData->curIntroNames.szFile, sizeof(pData->curIntroNames.szFile)/sizeof(pData->curIntroNames.szFile[0]) );
			pData->curIntroNames.szFile[sizeof(pData->curIntroNames.szFile)/sizeof(pData->curIntroNames.szFile[0]) - 1] = '\0';
		}
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}




BOOLEAN ReadInIntroNames(STR fileName, BOOLEAN localizedVersion)
{
	introNamesParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading IntroFiles.xml" );

	IntroName_TextOnly = localizedVersion;

	memset(&pData,0,sizeof(pData));

	const LegacyXmlCallbacks callbacks{
		&pData, introNamesStartElementHandle, introNamesEndElementHandle,
		introNamesCharacterDataHandle};
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
