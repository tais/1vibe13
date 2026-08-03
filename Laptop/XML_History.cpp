#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include <algorithm>
#include <array>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "Soldier Profile.h"
	#include "Text.h"
	#include "history.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	HISTORY_VALUES		curMercHistorys;
	std::array<HISTORY_VALUES, 500>* records;
	std::array<bool, 500>* seen;
	bool valid;
	bool hasIndex;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
	//CHAR16 gzMercNames[MAX_ENEMY_NAMES_CHARS];
}
typedef mercHistoryParseData;

static void XMLCALL
mercHistoryStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	mercHistoryParseData * pData = (mercHistoryParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "STRINGS") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "TEXT") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->curMercHistorys = HISTORY_VALUES{};
			pData->hasIndex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "sHistory") == 0 
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
mercHistoryCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	mercHistoryParseData * pData = (mercHistoryParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}


static void XMLCALL
mercHistoryEndElementHandle(void *userData, const XML_Char *name)
{
	mercHistoryParseData * pData = (mercHistoryParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "STRINGS") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "TEXT") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->records->size(),
					pData->curMercHistorys.uiIndex))
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curMercHistorys.uiIndex;
				if ((*pData->seen)[index])
					pData->valid = false;
				else if (pData->valid)
				{
					(*pData->records)[index] = pData->curMercHistorys;
					(*pData->seen)[index] = true;
				}
			}
		
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curMercHistorys.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "sHistory") == 0)
		{
			pData->curElement = ELEMENT;

			pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curMercHistorys.sHistory);
		}
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}




BOOLEAN ReadInHistorys(STR fileName, BOOLEAN localizedVersion)
{
	mercHistoryParseData pData{};
	std::array<HISTORY_VALUES, 500> pending{};
	std::array<bool, 500> seen{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading History.xml" );

	std::copy_n(HistoryName, pending.size(), pending.begin());
	pData.records = &pending;
	pData.seen = &seen;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, mercHistoryStartElementHandle, mercHistoryEndElementHandle,
		mercHistoryCharacterDataHandle};
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
	if (!pData.valid)
		return FALSE;

	std::copy(pending.begin(), pending.end(), HistoryName);

	return( TRUE );
}
