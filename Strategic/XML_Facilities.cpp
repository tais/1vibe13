///////////////////////////////////////////////////////////////////////////////
// HEADROCK HAM 3.5: Facility Location Externalization
//
// This file reads from an XML called "Facilities.XML". That file contains
// the locations of all facilities such as the "Gun Range", "Industrial",
// "Prison", etcetera. The data is written DIRECTLY into the global SectorInfo
// array, replacing a hardcoded system.
//
///////////////////////////////////////////////////////////////////////////////

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include <Item Types.h>
	#include "XML.h"
	#include "FileMan.h"
	#include "Campaign Types.h"

FACILITYLOCATIONS gFacilityLocations[256][MAX_NUM_FACILITY_TYPES];
UINT16 NUM_FACILITIES;

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];

	INT16			sCurSectorX;
	INT16			sCurSectorY;
	UINT8			ubCurFacilityType;
	UINT8			ubHidden;
	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef facilityParseData;


static void XMLCALL
facilityStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	facilityParseData * pData = (facilityParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "SECTORFACILITIES") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "FACILITY") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			//DebugMsg(TOPIC_JA2, DBG_LEVEL_3,"MergeStartElementHandle: setting memory for curMerge");

			pData->maxReadDepth++; //we are not skipping this element

			//pData->curIndex++;
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "SectorGrid") == 0 ||
				strcmp(name, "FacilityType") == 0 ||
				strcmp(name, "ubHidden") == 0
				))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
facilityCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	facilityParseData * pData = (facilityParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
facilityEndElementHandle(void *userData, const XML_Char *name)
{
	facilityParseData * pData = (facilityParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "SECTORFACILITIES") == 0)
		{
			pData->curElement = ELEMENT_NONE;
			NUM_FACILITIES = pData->curIndex;
		}
		else if(strcmp(name, "FACILITY") == 0)
		{
			pData->curIndex++;
			pData->curElement = ELEMENT_LIST;

			if (pData->ubCurFacilityType != 0)
			{
				gFacilityLocations[SECTOR(pData->sCurSectorX,pData->sCurSectorY)][pData->ubCurFacilityType].fFacilityHere = TRUE;
				gFacilityLocations[SECTOR(pData->sCurSectorX,pData->sCurSectorY)][pData->ubCurFacilityType].ubHidden = pData->ubHidden;
			}
		}

		else if(strcmp(name, "SectorGrid") == 0 )
		{
			UINT8	x, y;
			pData->curElement = ELEMENT;

			y = (UINT8)pData->szCharData[0] & 0x1F;
			x = (UINT8)atol(&pData->szCharData[1]);
			if ( x > 0 && x <= 16	&& y > 0 && y <= 16 )
			{
				pData->sCurSectorX	= x;
				pData->sCurSectorY  = y;
			}
		}
		else if(strcmp(name, "FacilityType") == 0 )
		{
			pData->curElement = ELEMENT;
			pData->ubCurFacilityType = (UINT8) atol(pData->szCharData);
		}

		else if(strcmp(name, "ubHidden") == 0 )
		{
			pData->curElement = ELEMENT;
			pData->ubHidden = (BOOLEAN) atol(pData->szCharData);
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}




BOOLEAN ReadInSectorFacilities(STR fileName)
{
	// Converted to the shared, leak-safe ParseXMLFile() helper (XML.cpp). The old
	// hand-rolled block leaked the parser on FileOpen-fail and the parser+handle on
	// FileRead-fail; the helper owns all three via RAII so every path is leak-free.
	facilityParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading Facilities.xml" );

	memset(&pData,0,sizeof(pData));
	pData.maxArraySize = MAXITEMS;
	pData.curIndex = -1;

	return ParseXMLFile(fileName,
		facilityStartElementHandle,
		facilityEndElementHandle,
		facilityCharacterDataHandle,
		&pData,
		"Facilities.xml") ? TRUE : FALSE;
}

