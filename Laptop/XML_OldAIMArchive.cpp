#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include <algorithm>
#include <array>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "AimArchives.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_1000_DATA_LENGTH+1];
	OLD_MERC_ARCHIVES_VALUES	curAimOldArchives;
	std::array<OLD_MERC_ARCHIVES_VALUES, NUM_PROFILES>* records;
	std::array<bool, NUM_PROFILES>* seen;
	BOOLEAN localizedVersion;
	bool valid;
	bool hasIndex;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef aimOldArchivesParseData;

static void XMLCALL
aimOldArchivesStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	aimOldArchivesParseData * pData = (aimOldArchivesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "OLD_MERC") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "MERC") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->curAimOldArchives = OLD_MERC_ARCHIVES_VALUES{};
			pData->hasIndex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
			    strcmp(name, "NickName") == 0 ||
				strcmp(name, "Name") == 0 ||
				strcmp(name, "Bio") == 0 ||
				strcmp(name, "FaceID") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
aimOldArchivesCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	aimOldArchivesParseData * pData = (aimOldArchivesParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}


static void XMLCALL
aimOldArchivesEndElementHandle(void *userData, const XML_Char *name)
{
	aimOldArchivesParseData * pData = (aimOldArchivesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "OLD_MERC") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "MERC") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->records->size(),
					pData->curAimOldArchives.uiIndex))
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curAimOldArchives.uiIndex;
				if ((*pData->seen)[index])
				{
					pData->valid = false;
				}
				else if (pData->valid && !pData->localizedVersion)
				{
					(*pData->records)[index] = pData->curAimOldArchives;
					(*pData->seen)[index] = true;
				}
				else if (pData->valid)
				{
					auto& destination = (*pData->records)[index];
					pData->valid =
						LaptopLocalizationModel::CopyText(
							destination.szBio,
							pData->curAimOldArchives.szBio) &&
						LaptopLocalizationModel::CopyText(
							destination.szName,
							pData->curAimOldArchives.szName) &&
						LaptopLocalizationModel::CopyText(
							destination.szNickName,
							pData->curAimOldArchives.szNickName);
					(*pData->seen)[index] = pData->valid;
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curAimOldArchives.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "NickName") == 0)
		{
		
			pData->curElement = ELEMENT;
			
			pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curAimOldArchives.szNickName);

		}	
		else if(strcmp(name, "Name") == 0)
		{
		
			pData->curElement = ELEMENT;
			
			pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curAimOldArchives.szName);

		}	
		else if(strcmp(name, "Bio") == 0)
		{
		
			pData->curElement = ELEMENT;
			
			pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curAimOldArchives.szBio);

		}	
		else if(strcmp(name, "FaceID") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curAimOldArchives.FaceID);
		}	
		
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInAimOldArchive(STR fileName, BOOLEAN localizedVersion)
{
	aimOldArchivesParseData pData{};
	std::array<OLD_MERC_ARCHIVES_VALUES, NUM_PROFILES> pending{};
	std::array<bool, NUM_PROFILES> seen{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading OldAIMArchive.xml" );

	std::copy_n(gAimOldArchives, pending.size(), pending.begin());
	pData.records = &pending;
	pData.seen = &seen;
	pData.localizedVersion = localizedVersion;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, aimOldArchivesStartElementHandle, aimOldArchivesEndElementHandle,
		aimOldArchivesCharacterDataHandle};
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

	std::copy(pending.begin(), pending.end(), gAimOldArchives);

	return( TRUE );
}
