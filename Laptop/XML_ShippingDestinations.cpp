#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "sgp.h"
#include "Overhead Types.h"
#include "Overhead.h"
#include "Text.h"
#include "Debug Control.h"
#include "expat.h"
#include "XML.h"
#include "PostalService.h"
#include <string>
#include <vector>

using namespace std;

CPostalService gPostalService;

typedef struct
{
	UINT8	ubMapY;
	UINT8	ubMapX;
	UINT8	ubMapZ;
	UINT32	uiIndex;
	UINT32	sGridNo;
	CHAR16	szName[MAX_DEST_NAME_LENGTH+1];
} DestinationReadInStruct;

struct
{
	PARSE_STAGE					curElement;

	CHAR8						szCharData[MAX_CHAR_DATA_LENGTH+1];

	DestinationReadInStruct		tempDest;
	UINT32						maxArraySize;
	UINT32						curIndex;
	UINT32						currentDepth;
	UINT32						maxReadDepth;
}
typedef destinationParseData;

BOOLEAN ShippingDestinations_TextOnly;

static void XMLCALL
destinationStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	destinationParseData * pData = (destinationParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth)
	{
		if(strcmp(name, "DESTINATIONLIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; 
		}
		else if(strcmp(name, "DESTINATION") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			memset(&pData->tempDest, 0, sizeof(DestinationReadInStruct));
			pData->maxReadDepth++;
			pData->curIndex++;
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "name") == 0 ||
				strcmp(name, "ubMapX") == 0 ||
				strcmp(name, "ubMapY") == 0 ||
				strcmp(name, "ubMapZ") == 0 ||
				strcmp(name, "sGridNo") == 0 ||
				strcmp(name, "uiIndex") == 0))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++;
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
destinationCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	destinationParseData * pData = (destinationParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}

static void XMLCALL
destinationEndElementHandle(void *userData, const XML_Char *name)
{
	destinationParseData * pData = (destinationParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth)
	{
		if(strcmp(name, "DESTINATIONLIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "DESTINATION") == 0)
		{
			pData->curElement = ELEMENT_LIST;
			
			if( !ShippingDestinations_TextOnly )
			{
				// We are reading the base data in the first pass of the file
				gPostalService.AddDestination(pData->tempDest.uiIndex, pData->tempDest.ubMapX, pData->tempDest.ubMapY, pData->tempDest.ubMapZ, pData->tempDest.sGridNo, pData->tempDest.szName);
			}
			else
			{
				// We are in second pass, during the loadup of the localization file.
				// We should have legitimate data for the <uiIndex> and <name> elements.
				// using uiIndex, iterate thru gPostalService::_Destinations till we find the correct 
				// DestinationStruct, and change its wstrName

				DestinationList::const_iterator dli = gPostalService.LookupDestinationList().begin();
				
				while(DESTINATION(dli).uiIndex != pData->tempDest.uiIndex)
				{
					dli++;
					if(dli == gPostalService.LookupDestinationList().end() )
					{
						// this doesnt really work, displays a message but then exits imediately, 
						// but at least it doesnt generate a software exception.. which would prolly actually make this string readable
						AssertMsg( 0, "Loading invalid uiIndex inside ShippingDestinations.xml." );
					}
					
				}
				DESTINATION(dli).wstrName = pData->tempDest.szName; 

			}
		}
		else if(strcmp(name, "name") == 0)
		{
			pData->curElement = ELEMENT;
			MultiByteToWideChar(CP_UTF8, 0, pData->szCharData, -1, pData->tempDest.szName, MAX_DEST_NAME_LENGTH);

		}
		else if(strcmp(name, "ubMapX") == 0)
		{
			pData->curElement = ELEMENT;
			pData->tempDest.ubMapX = (UINT8)strtoul(pData->szCharData, NULL, 10); 
		}
		else if(strcmp(name, "ubMapY") == 0)
		{
			pData->curElement = ELEMENT;
			pData->tempDest.ubMapY = (UINT8)strtoul(pData->szCharData, NULL, 10); 

		}
		else if(strcmp(name, "ubMapZ") == 0)
		{
			pData->curElement = ELEMENT;
			pData->tempDest.ubMapZ = (UINT8)strtoul(pData->szCharData, NULL, 10); 
		}
		else if(strcmp(name, "sGridNo") == 0)
		{
			pData->curElement = ELEMENT;
			pData->tempDest.sGridNo = (UINT32) atoi(pData->szCharData);
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->tempDest.uiIndex = (UINT8)strtoul(pData->szCharData, NULL, 10); 
		}


		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}

BOOLEAN ReadInShippingDestinations(STR fileName, BOOLEAN localizedVersion)
{
	destinationParseData pData;

	ShippingDestinations_TextOnly = localizedVersion;
	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading ShippingDestinations.xml" );

	// Initialize Data container
	memset(&pData,0,sizeof(pData));
	pData.maxArraySize = sizeof(UINT16);
	pData.curIndex = -1;

	const LegacyXmlCallbacks callbacks{
		&pData, destinationStartElementHandle, destinationEndElementHandle,
		destinationCharacterDataHandle};
	const LegacyXmlResult result =
		ParseLegacyXmlFile(fileName, callbacks);
	if (!result)
	{
		if (result.status != LegacyXmlStatus::NotFound &&
			result.status != LegacyXmlStatus::ReadError)
		{
			const auto message = FormatLegacyXmlFailure(fileName, result);
			LiveMessage(message.data());
		}
		return FALSE;
	}

	return( TRUE );
}
