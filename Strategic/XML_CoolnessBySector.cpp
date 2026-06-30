	#include "builddefines.h"
	#include <stdio.h>
	#include "XML.h"
	#include "expat.h"
	#include "string.h"
	#include "Campaign Types.h"
	#include "FileMan.h"
	#include "MemMan.h"
	#include "Debug Control.h"
	#include "mapscreen.h"

#define MAX_CHAR_DATA_LENGTH			500

extern UINT32 gCoolnessBySector[256];

typedef struct
{
	PARSE_STAGE	curElement;
	CHAR8			szCharData[MAX_CHAR_DATA_LENGTH+1];
	INT32			iCurCoolness;
	UINT32			uiRowNumber;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
} SectorCoolnessParseData;


static void XMLCALL
SectorCoolnessStartElementHandle(void *userData, const XML_Char *name, const char **atts)
{
	SectorCoolnessParseData * pData = (SectorCoolnessParseData *) userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{

		if(strcmp(name, "COOLNESS_BY_SECTOR") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;
			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "MAP_ROW") == 0 && pData->curElement == ELEMENT_LIST)
		{
			UINT32 uiAttrIndex;
			pData->curElement = ELEMENT;

			for(uiAttrIndex = 0;atts[uiAttrIndex] != NULL;uiAttrIndex += 2)
				if(strcmp(atts[uiAttrIndex], "row") == 0)
				{
					pData->uiRowNumber = atol(atts[uiAttrIndex+1]);
					break;
				}

			if(atts[uiAttrIndex] != NULL && pData->uiRowNumber < MAP_WORLD_Y)
				pData->maxReadDepth++; //we are not skipping this element
		}
		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}



static void XMLCALL
SectorCoolnessCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	SectorCoolnessParseData * pData = (SectorCoolnessParseData *) userData;

	if(pData->currentDepth <= pData->maxReadDepth && strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
}


static void XMLCALL
SectorCoolnessEndElementHandle(void *userData, const XML_Char *name)
{
	SectorCoolnessParseData * pData = (SectorCoolnessParseData *) userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "COOLNESS_BY_SECTOR") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "MAP_ROW") == 0 && pData->curElement == ELEMENT)
		{
			STR8 curBuffer = pData->szCharData + strspn(pData->szCharData," \t\n\r");
			UINT32 curCellIndex = 0;
			UINT32 curNumber;

			pData->curElement = ELEMENT_LIST;

			while(curBuffer[0] != '\0')
			{
				sscanf( curBuffer,"%d",&curNumber);

				gCoolnessBySector[SECTOR(curCellIndex+1, pData->uiRowNumber) ] = curNumber;

				curCellIndex++;
				curBuffer += strcspn(curBuffer," \t\n\r\0");
				curBuffer += strspn(curBuffer," \t\n\r");
			}
		}
		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}


BOOLEAN ReadInCoolnessBySector(STR fileName)
{
	// Converted to the shared, leak-safe ParseXMLFile() helper (XML.cpp). The old
	// hand-rolled block leaked the parser on FileOpen-fail and the parser+handle on
	// FileRead-fail; the helper owns all three via RAII so every path is leak-free.
	SectorCoolnessParseData pData;
	memset(&pData,0,sizeof(pData));

	return ParseXMLFile(fileName,
		SectorCoolnessStartElementHandle,
		SectorCoolnessEndElementHandle,
		SectorCoolnessCharacterDataHandle,
		&pData,
		"CoolnessBySector.xml") ? TRUE : FALSE;
}
