#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include "sgp.h"
#include "Overhead Types.h"
#include "Overhead.h"
#include "Text.h"
#include "Debug Control.h"
#include "expat.h"
#include "XML.h"
#include "PostalService.h"
#include <string>
#include <algorithm>
#include <iterator>
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
	std::vector<DestinationReadInStruct>* destinations;
	BOOLEAN					localizedVersion;
	bool					valid;
	bool					hasIndex;
	bool					hasName;
	bool					hasMapX;
	bool					hasMapY;
	bool					hasMapZ;
	bool					hasGridNo;
	UINT32						maxArraySize;
	UINT32						curIndex;
	UINT32						currentDepth;
	UINT32						maxReadDepth;
}
typedef destinationParseData;

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
			pData->tempDest = DestinationReadInStruct{};
			pData->hasIndex = false;
			pData->hasName = false;
			pData->hasMapX = false;
			pData->hasMapY = false;
			pData->hasMapZ = false;
			pData->hasGridNo = false;
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

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
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
			
			if (!LaptopLocalizationModel::IsShippingDestinationRecordValid(
				pData->localizedVersion, pData->hasIndex, pData->hasName,
				pData->hasMapX, pData->hasMapY, pData->hasMapZ,
				pData->hasGridNo))
			{
				pData->valid = false;
			}
			if (pData->valid)
				pData->destinations->push_back(pData->tempDest);
		}
		else if(strcmp(name, "name") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasName = LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->tempDest.szName);
			pData->valid = pData->valid && pData->hasName;

		}
		else if(strcmp(name, "ubMapX") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasMapX = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->tempDest.ubMapX);
			pData->valid = pData->valid && pData->hasMapX;
		}
		else if(strcmp(name, "ubMapY") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasMapY = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->tempDest.ubMapY);
			pData->valid = pData->valid && pData->hasMapY;

		}
		else if(strcmp(name, "ubMapZ") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasMapZ = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->tempDest.ubMapZ);
			pData->valid = pData->valid && pData->hasMapZ;
		}
		else if(strcmp(name, "sGridNo") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasGridNo = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->tempDest.sGridNo);
			pData->valid = pData->valid && pData->hasGridNo;
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->tempDest.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}


		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}

BOOLEAN ReadInShippingDestinations(STR fileName, BOOLEAN localizedVersion)
{
	destinationParseData pData{};
	std::vector<DestinationReadInStruct> pendingDestinations;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading ShippingDestinations.xml" );

	pData.destinations = &pendingDestinations;
	pData.localizedVersion = localizedVersion;
	pData.valid = true;

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
	if (!pData.valid || pendingDestinations.size() > MAX_DESTINATIONS)
		return FALSE;

	for (auto first = pendingDestinations.begin();
		first != pendingDestinations.end(); ++first)
	{
		if (std::find_if(std::next(first), pendingDestinations.end(),
			[first](const DestinationReadInStruct& destination)
			{
				return destination.uiIndex == first->uiIndex;
			}) != pendingDestinations.end())
		{
			return FALSE;
		}
	}

	auto& liveDestinations = gPostalService.LookupDestinationList();
	if (!localizedVersion)
	{
		if (liveDestinations.size() + pendingDestinations.size() >
			MAX_DESTINATIONS)
		{
			return FALSE;
		}
		for (const auto& pending : pendingDestinations)
		{
			if (std::find_if(liveDestinations.begin(), liveDestinations.end(),
				[&pending](const DestinationStruct& destination)
				{
					return destination.uiIndex == pending.uiIndex;
				}) != liveDestinations.end())
			{
				return FALSE;
			}
		}
		for (auto& pending : pendingDestinations)
		{
			gPostalService.AddDestination(pending.uiIndex, pending.ubMapX,
				pending.ubMapY, pending.ubMapZ, pending.sGridNo,
				pending.szName);
		}
	}
	else
	{
		std::vector<DestinationStruct*> localizedDestinations;
		localizedDestinations.reserve(pendingDestinations.size());
		for (const auto& pending : pendingDestinations)
		{
			const auto found = std::find_if(
				liveDestinations.begin(), liveDestinations.end(),
				[&pending](const DestinationStruct& destination)
				{
					return destination.uiIndex == pending.uiIndex;
				});
			if (found == liveDestinations.end())
				return FALSE;
			localizedDestinations.push_back(&*found);
		}
		for (std::size_t index = 0; index < pendingDestinations.size(); ++index)
			localizedDestinations[index]->wstrName =
				pendingDestinations[index].szName;
	}

	return( TRUE );
}
