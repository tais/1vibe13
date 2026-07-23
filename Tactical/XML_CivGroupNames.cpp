#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	CIV_NAMES_VALUES	curCivGroupNames;
	CIV_NAMES_VALUES *	curArray;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef civGroupNamesParseData;

BOOLEAN CivGroupNames_TextOnly;

static void XMLCALL
civGroupNamesStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	civGroupNamesParseData * pData = (civGroupNamesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "CIV_GROUP_NAMES") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "NAME") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "Enabled") == 0 ||
				strcmp(name, "Loyalty") == 0 ||
				strcmp(name, "AddToBattle") == 0 ||
				strcmp(name, "fCanBeCaptured" ) == 0 ||
				strcmp(name, "Side") == 0 ||
				strcmp(name, "CustomSide") == 0 ||
				strcmp(name, "szGroup") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
civGroupNamesCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	civGroupNamesParseData * pData = (civGroupNamesParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
civGroupNamesEndElementHandle(void *userData, const XML_Char *name)
{
	civGroupNamesParseData * pData = (civGroupNamesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "CIV_GROUP_NAMES") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "NAME") == 0)
		{
			pData->curElement = ELEMENT_LIST;
				
			if (pData->curCivGroupNames.uiIndex < NUM_CIV_GROUPS)   // guard XML-driven index -> zCivGroupName[NUM_CIV_GROUPS] OOB write
			if (!CivGroupNames_TextOnly)
				{
					wcscpy(zCivGroupName[pData->curCivGroupNames.uiIndex].szCurGroup, pData->curCivGroupNames.szCurGroup);
					zCivGroupName[pData->curCivGroupNames.uiIndex].Enabled = pData->curCivGroupNames.Enabled;
					
					zCivGroupName[pData->curCivGroupNames.uiIndex].AddToBattle = pData->curCivGroupNames.AddToBattle;
					zCivGroupName[pData->curCivGroupNames.uiIndex].Loyalty = pData->curCivGroupNames.Loyalty;
					zCivGroupName[pData->curCivGroupNames.uiIndex].fCanBeCaptured = pData->curCivGroupNames.fCanBeCaptured;
					zCivGroupName[pData->curCivGroupNames.uiIndex].bSide = pData->curCivGroupNames.bSide;
					zCivGroupName[pData->curCivGroupNames.uiIndex].fCustomSide = pData->curCivGroupNames.fCustomSide;
				}
				else
				{
					wcscpy(zCivGroupName[pData->curCivGroupNames.uiIndex].szCurGroup, pData->curCivGroupNames.szCurGroup);
				}			
		
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curCivGroupNames.uiIndex	= (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "Enabled") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curCivGroupNames.Enabled	= (BOOLEAN) atol(pData->szCharData);
		}
		else if(strcmp(name, "Loyalty") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curCivGroupNames.Loyalty	= (BOOLEAN) atol(pData->szCharData);
		}
		else if(strcmp(name, "AddToBattle") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curCivGroupNames.AddToBattle	= (BOOLEAN) atol(pData->szCharData);
		}
		else if ( strcmp( name, "fCanBeCaptured" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curCivGroupNames.fCanBeCaptured = (BOOLEAN)atol( pData->szCharData );
		}
		else if (strcmp(name, "Side") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curCivGroupNames.bSide = (INT8)atol(pData->szCharData);
		}
		else if (strcmp(name, "CustomSide") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curCivGroupNames.fCustomSide = (BOOLEAN)atol(pData->szCharData);
		}
		else if(strcmp(name, "szGroup") == 0 )
		{
			pData->curElement = ELEMENT;

			MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, pData->curCivGroupNames.szCurGroup, sizeof(pData->curCivGroupNames.szCurGroup)/sizeof(pData->curCivGroupNames.szCurGroup[0]) );
			pData->curCivGroupNames.szCurGroup[sizeof(pData->curCivGroupNames.szCurGroup)/sizeof(pData->curCivGroupNames.szCurGroup[0]) - 1] = '\0';
	
			#ifdef JA2EDITOR
			wcscpy(gszCivGroupNames[pData->curCivGroupNames.uiIndex], pData->curCivGroupNames.szCurGroup);
			#endif
		
		}
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInCivGroupNamesStats( STR fileName, BOOLEAN localizedVersion)
{
	civGroupNamesParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading CivGroupNames.xml" );

	CivGroupNames_TextOnly = localizedVersion;

	memset(&pData,0,sizeof(pData));
	const LegacyXmlCallbacks callbacks{
		&pData, civGroupNamesStartElementHandle, civGroupNamesEndElementHandle,
		civGroupNamesCharacterDataHandle};
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
