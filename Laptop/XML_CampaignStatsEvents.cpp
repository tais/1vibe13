#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include <algorithm>
#include <array>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "CampaignStats.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	CAMPAIGNSTATSEVENT		curItem;
	std::array<CAMPAIGNSTATSEVENT, NUM_CAMPAIGNSTATSEVENTS>* records;
	std::array<bool, NUM_CAMPAIGNSTATSEVENTS>* seen;
	BOOLEAN localizedVersion;
	bool valid;
	bool hasIndex;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
	CHAR16 gzBackground[MAX_ENEMY_NAMES_CHARS];
}
typedef CSEParseData;

static void XMLCALL
CSEStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	CSEParseData * pData = (CSEParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "CAMPAIGNSTATSEVENTS") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "EVENT") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			pData->curItem = CAMPAIGNSTATSEVENT{};
			pData->hasIndex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||		
				strcmp(name, "szText0") == 0 ||
				strcmp(name, "szText1") == 0 ||
				strcmp(name, "szText2") == 0 ||
				strcmp(name, "szText3") == 0 ||
				strcmp(name, "szText4") == 0 ||
				strcmp(name, "szText5") == 0 ||
				strcmp(name, "szText6") == 0 ||
				strcmp(name, "szText7") == 0 ||
				strcmp(name, "szText8") == 0 ||
				strcmp(name, "szText9") == 0 ||
				strcmp(name, "usCityTaken") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;
}

static void XMLCALL
CSECharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	CSEParseData * pData = (CSEParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}

static void XMLCALL
CSEEndElementHandle(void *userData, const XML_Char *name)
{
	CSEParseData * pData = (CSEParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "CAMPAIGNSTATSEVENTS") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "EVENT") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->records->size(), pData->curItem.uiIndex))
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curItem.uiIndex;
				if ((*pData->seen)[index])
				{
					pData->valid = false;
				}
				else if (pData->valid && pData->localizedVersion)
				{
					for (int i = 0; i < MAX_CAMPAIGNSTATSEVENTS_TEXTS; i++)
					{
						pData->valid = pData->valid &&
							LaptopLocalizationModel::CopyText(
								(*pData->records)[index].szText[i],
								pData->curItem.szText[i]);
					}
					(*pData->seen)[index] = pData->valid;
				}
				else if (pData->valid)
				{
					(*pData->records)[index] = pData->curItem;
					(*pData->seen)[index] = true;
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curItem.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "usCityTaken") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curItem.usCityTaken);
		}
		else
		{
			for(int i = 0; i < MAX_CAMPAIGNSTATSEVENTS_TEXTS; ++i)
			{
				char txt[10];
				sprintf(txt, "szText%d", i);

				if(strcmp(name, txt) == 0 )
				{
					pData->curElement = ELEMENT;
			
					pData->valid = pData->valid &&
						LaptopLocalization::ConvertUtf8(
							pData->szCharData, pData->curItem.szText[i]);

					break;
				}
			}
		}

		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInCampaignStatsEvents(STR fileName, BOOLEAN localizedVersion)
{
	CSEParseData pData{};
	std::array<CAMPAIGNSTATSEVENT, NUM_CAMPAIGNSTATSEVENTS> pending{};
	std::array<bool, NUM_CAMPAIGNSTATSEVENTS> seen{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading CampaignStatsEvents.xml" );

	if (localizedVersion)
		std::copy_n(zCampaignStatsEvent, pending.size(), pending.begin());
	pData.records = &pending;
	pData.seen = &seen;
	pData.localizedVersion = localizedVersion;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, CSEStartElementHandle, CSEEndElementHandle,
		CSECharacterDataHandle};
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
	if (!pData.valid)
		return FALSE;

	std::copy(pending.begin(), pending.end(), zCampaignStatsEvent);

	return( TRUE );
}


BOOLEAN WriteCampaignStatsEvents( STR fileName)
{
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( fileName, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<CAMPAIGNSTATSEVENTS>\r\n");
		for(cnt = 0; cnt < NUM_CAMPAIGNSTATSEVENTS; ++cnt)
		{
			FilePrintf(hFile,"\t<EVENT>\r\n");
			FilePrintf(hFile,"\t\t<uiIndex>%d</uiIndex>\r\n",				cnt);
			
			FilePrintf(hFile,"\t</EVENT>\r\n");
		}
		FilePrintf(hFile,"</CAMPAIGNSTATSEVENTS>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
