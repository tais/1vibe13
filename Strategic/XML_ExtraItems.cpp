#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "builddefines.h"
#include <stdio.h>
#include <string>
#include "XML.h"
#include "expat.h"
#include "DEBUG.H"
#include "string.h"
#include "Tactical Save.h"
#include "FileMan.h"
#include "MemMan.h"
#include "Debug Control.h"
#include "GameSettings.h"
#include "GameInitOptionsScreen.h"

typedef enum
{
	EXTRAITEMS_ELEMENT_NONE = 0,
	EXTRAITEMS_ELEMENT_EXTRAITEMS,
	EXTRAITEMS_ELEMENT_ITEM,
	EXTRAITEMS_ELEMENT,
} EXTRAITEMS_PARSE_STAGE;

typedef struct
{
	EXTRAITEMS_PARSE_STAGE	curElement;
	CHAR8					szCharData[MAX_CHAR_DATA_LENGTH+1];
	UINT32					currentDepth;
	UINT32					maxReadDepth;

	UINT32					item;
	UINT32					quantity;
	UINT32					condition;
	UINT32					gridno;
	BOOLEAN					visible;

	bool					sectorIsLoaded;
	UINT8					sectorX;
	UINT8					sectorY;
	UINT8					sectorZ;
} ExtraItemsParseData;


static bool gSectorIsLoaded = false;
static UINT8 gX = 0;
static UINT8 gY = 0;
static UINT8 gZ = 0;

/** Process the opening tag in this expat callback.
 */
static void XMLCALL
ExtraItemsStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	ExtraItemsParseData * pData = (ExtraItemsParseData *) userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "ExtraItems") == 0 && pData->curElement == EXTRAITEMS_ELEMENT_NONE)
		{
			pData->curElement = EXTRAITEMS_ELEMENT_EXTRAITEMS;
			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "Item") == 0 && pData->curElement == EXTRAITEMS_ELEMENT_EXTRAITEMS)
		{
			pData->curElement = EXTRAITEMS_ELEMENT_ITEM;
			pData->maxReadDepth++; //we are not skipping this element

			// set defaults
			pData->quantity = 1;
			pData->condition = 100;
			pData->gridno = 0;
			pData->visible = FALSE;
		}
		else if(pData->curElement == EXTRAITEMS_ELEMENT_ITEM &&
			(strcmp(name, "uiIndex") == 0 ||
			strcmp(name, "quantity") == 0 ||
			strcmp(name, "condition") == 0 ||
			strcmp(name, "gridno") == 0 ||
			strcmp(name, "visible") == 0 ))
		{
			pData->curElement = EXTRAITEMS_ELEMENT;
			pData->maxReadDepth++;
		}
		pData->szCharData[0] = '\0';
	}
	pData->currentDepth++;
}

/** Process any text content in this callback.
 */
static void XMLCALL
ExtraItemsCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	ExtraItemsParseData * pData = (ExtraItemsParseData *) userData;

	if(pData->currentDepth <= pData->maxReadDepth && strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
}

/** Process the closing tag in this expat callback.
 */
static void XMLCALL
ExtraItemsEndElementHandle(void *userData, const XML_Char *name)
{
	ExtraItemsParseData * pData = (ExtraItemsParseData *) userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(pData->curElement == EXTRAITEMS_ELEMENT_EXTRAITEMS &&
			strcmp(name, "ExtraItems") == 0)
		{
			pData->curElement = EXTRAITEMS_ELEMENT_NONE;
		}
		else if(pData->curElement == EXTRAITEMS_ELEMENT_ITEM &&
				strcmp(name, "Item") == 0 &&
				pData->item > 0 && // Make sure not to create 0-index objects!
				pData->quantity > 0 && // Quantity should be positive. Not sure what the LIMIT is, if any.
				pData->condition > 0 && pData->condition <= 100 ) // Condition between 1 and 100!
		{
			OBJECTTYPE object;
			CreateItem(pData->item, pData->condition, &object);
			for (unsigned cnt=1; cnt <= pData->quantity; ++cnt) {
				if (gSectorIsLoaded) {
					AddItemToPool( pData->gridno, &object, pData->visible, 0, WORLD_ITEM_REACHABLE, 0 );
				} else {
					AddItemsToUnLoadedSector( gX, gY, gZ, pData->gridno, pData->visible, &object, 0, WORLD_ITEM_REACHABLE, 0, 1, 0 );
				}
			}
			pData->curElement = EXTRAITEMS_ELEMENT_EXTRAITEMS;
		}

		else if( pData->curElement == EXTRAITEMS_ELEMENT &&
				strcmp( name, "uiIndex" ) == 0)
		{
			pData->item = (UINT32)atol( pData->szCharData );
			pData->curElement = EXTRAITEMS_ELEMENT_ITEM;
		}
		else if( pData->curElement == EXTRAITEMS_ELEMENT &&
				strcmp( name, "quantity" ) == 0)
		{
			pData->quantity = (UINT32)atol( pData->szCharData );
			pData->curElement = EXTRAITEMS_ELEMENT_ITEM;
		}
		else if( pData->curElement == EXTRAITEMS_ELEMENT &&
				strcmp( name, "condition" ) == 0)
		{
			pData->condition = (UINT32)atol( pData->szCharData );
			pData->curElement = EXTRAITEMS_ELEMENT_ITEM;
		}
		else if( pData->curElement == EXTRAITEMS_ELEMENT &&
				strcmp( name, "gridno" ) == 0)
		{
			pData->gridno = (UINT32)atol( pData->szCharData );
			pData->curElement = EXTRAITEMS_ELEMENT_ITEM;
		}
		else if( pData->curElement == EXTRAITEMS_ELEMENT &&
				strcmp( name, "visible" ) == 0)
		{
			pData->visible = (UINT32)atol( pData->szCharData );
			pData->curElement = EXTRAITEMS_ELEMENT_ITEM;
		}
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}


static void PrepareExtraItemsDocument(void *userData)
{
	ExtraItemsParseData *pData = (ExtraItemsParseData *)userData;
	gSectorIsLoaded = pData->sectorIsLoaded;
	gX = pData->sectorX;
	gY = pData->sectorY;
	gZ = pData->sectorZ;
}

void AddExtraItems(UINT8 x, UINT8 y, UINT8 z, bool sectorIsLoaded)
{
	std::string baseFileName = TABLEDATA_DIRECTORY;
	if (x < 10) {
		baseFileName += EXTRAITEMSFILENAME;
		const std::string::size_type spot = baseFileName.find("A9_0");
		Assert(spot != std::string::npos);
		if (spot == std::string::npos)
			return;
		baseFileName[spot] = 'A' + y - 1;
		baseFileName[spot + 1] = '0' + x;
		baseFileName[spot + 3] = '0' + z;
	} else {
		baseFileName += EXTRAITEMSFILENAME2;
		const std::string::size_type spot = baseFileName.find("A11_0");
		Assert(spot != std::string::npos);
		if (spot == std::string::npos)
			return;
		baseFileName[spot] = 'A' + y - 1;
		baseFileName[spot + 1] = '0' + x / 10;
		baseFileName[spot + 2] = '0' + x % 10;
		baseFileName[spot + 4] = '0' + z;
	}

	// Append a suffix based on the difficulty
	std::string difficultySuffix;
	switch (gGameOptions.ubDifficultyLevel)
	{
		case DIF_LEVEL_EASY:
			difficultySuffix = "_Novice";
			break;
		case DIF_LEVEL_MEDIUM:
			difficultySuffix = "_Experienced";
			break;
		case DIF_LEVEL_HARD:
			difficultySuffix = "_Expert";
			break;
		case DIF_LEVEL_INSANE:
			difficultySuffix = "_Insane";
			break;
		default:
			difficultySuffix =
				"_Diff_" + std::to_string(gGameOptions.ubDifficultyLevel);
			break;
	}

	const std::string difficultyFileName =
		baseFileName + difficultySuffix + ".xml";
	if(!FileExists(difficultyFileName.c_str()))//dnl ch75 261013 just to avoid sdd::exception under debug from VFS when file not exist
		return;

	ExtraItemsParseData pData;
	memset(&pData,0,sizeof(pData));
	pData.sectorIsLoaded = sectorIsLoaded;
	pData.sectorX = x;
	pData.sectorY = y;
	pData.sectorZ = z;

	const LegacyXmlCallbacks callbacks{
		&pData, ExtraItemsStartElementHandle, ExtraItemsEndElementHandle,
		ExtraItemsCharacterDataHandle, PrepareExtraItemsDocument};
	std::string selectedFileName = difficultyFileName;
	LegacyXmlResult result =
		ParseLegacyXmlFile(selectedFileName.c_str(), callbacks);
	if (result.status == LegacyXmlStatus::NotFound)
	{
		selectedFileName = baseFileName;
		result = ParseLegacyXmlFile(selectedFileName.c_str(), callbacks);
	}
	if (!result)
	{
		if (result.status != LegacyXmlStatus::NotFound &&
			result.status != LegacyXmlStatus::ReadError)
		{
			const auto message =
				FormatLegacyXmlFailure(selectedFileName.c_str(), result);
			LiveMessage(message.data());
		}
		return;
	}

	return;
}
