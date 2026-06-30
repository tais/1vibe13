	#include "sgp.h"
	#include <FileMan.h>
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];

	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef squadnamesParseData;

std::vector<std::wstring> gSquadNameVector;

static void XMLCALL
squadnamesStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	squadnamesParseData * pData = (squadnamesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "SQUAD_NAMES") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;
			
			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "SQUADNAME") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			
			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "Squad") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}
	
	pData->currentDepth++;
}

static void XMLCALL
squadnamesCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	squadnamesParseData * pData = (squadnamesParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}

static void XMLCALL
squadnamesEndElementHandle(void *userData, const XML_Char *name)
{
	squadnamesParseData * pData = (squadnamesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "SQUAD_NAMES") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "SQUADNAME") == 0)
		{
			pData->curElement = ELEMENT_LIST;
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
		}
		else if(strcmp(name, "Squad") == 0)
		{
			pData->curElement = ELEMENT;

			CHAR16 bla[30];

			MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, bla, sizeof( bla ) / sizeof( bla[0] ) );
			bla[sizeof( bla ) / sizeof( bla[0] ) - 1] = '\0';

			gSquadNameVector.push_back( bla );
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}

BOOLEAN ReadInSquadNamesStats(STR fileName)
{
	// Converted to the shared, leak-safe ParseXMLFile() helper (XML.cpp). The old
	// hand-rolled block leaked the parser on FileOpen-fail and the parser+handle on
	// FileRead-fail; the helper owns all three via RAII so every path is leak-free.
	squadnamesParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading SquadNames.xml" );

	memset(&pData,0,sizeof(pData));

	return ParseXMLFile(fileName,
		squadnamesStartElementHandle,
		squadnamesEndElementHandle,
		squadnamesCharacterDataHandle,
		&pData,
		"SquadNames.xml") ? TRUE : FALSE;
}

BOOLEAN WriteSquadNamesStats()
{	
	return( TRUE );
}
