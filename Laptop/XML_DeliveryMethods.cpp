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
#include <limits>
#include <utility>
#include <vector>

using namespace std;

extern CPostalService gPostalService;

typedef struct
{
	UINT32	uiDestinationIndex;
	UINT16	usDestinationFee;
	INT8	bDaysAhead;
}DestinationDeliveryInfoReadInStruct;
typedef vector<DestinationDeliveryInfoReadInStruct> DestinationDeliveryInfoReadInTable;

struct DeliveryMethodReadInStruct
{
	CHAR16 szDescription[MAX_DELIVERYMETHOD_DESC_LENGTH + 1];
	DestinationDeliveryInfoReadInTable destinationDeliveryInfos;
};

enum
{
	DELIVERYMETHOD_ELEMENT_NONE=0,
	DELIVERYMETHOD_ELEMENT_TABLE,
	DELIVERYMETHOD_ELEMENT,
	DELIVERYMETHOD_ELEMENT_PROPERTY,
	DESTINATIONDELIVERYINFO_ELEMENT_TABLE,
	DESTINATIONDELIVERYINFO_ELEMENT,
	DESTINATIONDELIVERYINFO_ELEMENT_PROPERTY
}typedef DELIVERYMETHOD_PARSE_STAGE;

struct
{
	DELIVERYMETHOD_PARSE_STAGE	curElement;

	CHAR8						szCharData[MAX_CHAR_DATA_LENGTH+1];

	DeliveryMethodReadInStruct CurDeliveryMethod;
	DestinationDeliveryInfoReadInStruct CurDestinationDeliveryInfo;
	std::vector<DeliveryMethodReadInStruct>* deliveryMethods;
	bool valid;
	bool hasDescription;
	bool hasDestinationIndex;
	bool hasDestinationFee;
	bool hasDaysAhead;

	UINT32						maxArraySize;
	UINT32						currentDepth;
	UINT32						maxReadDepth;
}
typedef deliveryMethodParseData;

static void XMLCALL
deliveryMethodStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	deliveryMethodParseData * pData = (deliveryMethodParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth)
	{
		if(strcmp(name, "DELIVERYMETHODTABLE") == 0 && pData->curElement == DELIVERYMETHOD_ELEMENT_NONE)
		{
			pData->curElement = DELIVERYMETHOD_ELEMENT_TABLE;

			pData->maxReadDepth++;
		}
		else if(strcmp(name, "DELIVERYMETHOD") == 0 && pData->curElement == DELIVERYMETHOD_ELEMENT_TABLE)
		{
			pData->curElement = DELIVERYMETHOD_ELEMENT;
			pData->CurDeliveryMethod = DeliveryMethodReadInStruct{};
			pData->hasDescription = false;
			pData->maxReadDepth++;
		}
		else if(pData->curElement == DELIVERYMETHOD_ELEMENT &&
				(strcmp(name, "description") == 0) )
		{
			pData->curElement = DELIVERYMETHOD_ELEMENT_PROPERTY;

			pData->maxReadDepth++;
		}
		else if(pData->curElement == DELIVERYMETHOD_ELEMENT &&
				(strcmp(name, "DESTINATIONDELIVERYINFOTABLE") == 0))
		{
			pData->curElement = DESTINATIONDELIVERYINFO_ELEMENT_TABLE;

			pData->maxReadDepth++;
		}
		else if(pData->curElement == DESTINATIONDELIVERYINFO_ELEMENT_TABLE &&
				(strcmp(name, "DESTINATIONDELIVERYINFO") == 0))
		{
			pData->curElement = DESTINATIONDELIVERYINFO_ELEMENT;
			pData->CurDestinationDeliveryInfo =
				DestinationDeliveryInfoReadInStruct{};
			pData->hasDestinationIndex = false;
			pData->hasDestinationFee = false;
			pData->hasDaysAhead = false;

			pData->maxReadDepth++;
		}
		else if(pData->curElement == DESTINATIONDELIVERYINFO_ELEMENT &&
				(strcmp(name, "uiDestinationIndex") == 0 ||
				strcmp(name, "usDestinationFee") == 0 ||
				strcmp(name, "bDaysAhead") == 0 ) )
		{
			pData->curElement = DESTINATIONDELIVERYINFO_ELEMENT_PROPERTY;

			pData->maxReadDepth++;
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
deliveryMethodCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	deliveryMethodParseData * pData = (deliveryMethodParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}

static void XMLCALL
deliveryMethodEndElementHandle(void *userData, const XML_Char *name)
{
	deliveryMethodParseData * pData = (deliveryMethodParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth)
	{
		if(strcmp(name, "DELIVERYMETHODTABLE") == 0)
		{
			pData->curElement = DELIVERYMETHOD_ELEMENT_NONE;
		}
		else if(strcmp(name, "DELIVERYMETHOD") == 0)
		{
			pData->curElement = DELIVERYMETHOD_ELEMENT_TABLE;
			if (!pData->hasDescription)
				pData->valid = false;
			if (pData->valid)
				pData->deliveryMethods->push_back(
					std::move(pData->CurDeliveryMethod));
		}
		else if(strcmp(name, "description") == 0)
		{
			pData->curElement = DELIVERYMETHOD_ELEMENT;
			pData->hasDescription = LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->CurDeliveryMethod.szDescription);
			pData->valid = pData->valid && pData->hasDescription;
		}
		else if(strcmp(name, "DESTINATIONDELIVERYINFOTABLE") == 0)
		{
			pData->curElement = DELIVERYMETHOD_ELEMENT;
		}
		else if(strcmp(name, "DESTINATIONDELIVERYINFO") == 0)
		{
			pData->curElement = DESTINATIONDELIVERYINFO_ELEMENT_TABLE;
			if (!pData->hasDestinationIndex || !pData->hasDestinationFee ||
				!pData->hasDaysAhead)
			{
				pData->valid = false;
			}
			if (pData->valid)
			{
				pData->CurDeliveryMethod.destinationDeliveryInfos.push_back(
					pData->CurDestinationDeliveryInfo);
			}
		}
		else if(strcmp(name, "uiDestinationIndex") == 0)
		{
			pData->curElement = DESTINATIONDELIVERYINFO_ELEMENT;
			pData->hasDestinationIndex =
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData,
					pData->CurDestinationDeliveryInfo.uiDestinationIndex);
			pData->valid = pData->valid && pData->hasDestinationIndex;
		}
		else if(strcmp(name, "usDestinationFee") == 0)
		{
			pData->curElement = DESTINATIONDELIVERYINFO_ELEMENT;
			pData->hasDestinationFee =
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData,
					pData->CurDestinationDeliveryInfo.usDestinationFee);
			pData->valid = pData->valid && pData->hasDestinationFee;
		}
		else if(strcmp(name, "bDaysAhead") == 0)
		{
			pData->curElement = DESTINATIONDELIVERYINFO_ELEMENT;
			pData->hasDaysAhead = LaptopLocalizationModel::ParseInteger(
				pData->szCharData,
				pData->CurDestinationDeliveryInfo.bDaysAhead);
			pData->valid = pData->valid && pData->hasDaysAhead;
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}

BOOLEAN ReadInDeliveryMethods(STR fileName)
{
	deliveryMethodParseData pData{};
	std::vector<DeliveryMethodReadInStruct> pendingDeliveryMethods;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading DeliveryMethods.xml" );

	pData.deliveryMethods = &pendingDeliveryMethods;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, deliveryMethodStartElementHandle, deliveryMethodEndElementHandle,
		deliveryMethodCharacterDataHandle};
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
	if (!pData.valid || pendingDeliveryMethods.empty() ||
		pendingDeliveryMethods.size() >
			static_cast<std::size_t>(std::numeric_limits<UINT8>::max()) + 1 ||
		gPostalService.GetDeliveryMethodCount() != 0)
	{
		return FALSE;
	}

	const auto& destinations = gPostalService.LookupDestinationList();
	for (const auto& method : pendingDeliveryMethods)
	{
		for (auto first = method.destinationDeliveryInfos.begin();
			first != method.destinationDeliveryInfos.end(); ++first)
		{
			if (std::find_if(std::next(first),
				method.destinationDeliveryInfos.end(),
				[first](const DestinationDeliveryInfoReadInStruct& info)
				{
					return info.uiDestinationIndex ==
						first->uiDestinationIndex;
				}) != method.destinationDeliveryInfos.end())
			{
				return FALSE;
			}
			if (std::find_if(destinations.begin(), destinations.end(),
				[first](const DestinationStruct& destination)
				{
					return destination.uiIndex ==
						first->uiDestinationIndex;
				}) == destinations.end())
			{
				return FALSE;
			}
		}
	}

	for (auto& method : pendingDeliveryMethods)
	{
		const UINT8 deliveryMethod =
			gPostalService.AddDeliveryMethod(method.szDescription);
		for (const auto& info : method.destinationDeliveryInfos)
		{
			gPostalService.SetDestinationDeliveryInfo(deliveryMethod,
				info.uiDestinationIndex, info.usDestinationFee,
				info.bDaysAhead);
		}
	}

	return( TRUE );
}
