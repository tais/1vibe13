#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include <algorithm>
#include <array>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "LuaInitNPCs.h"
	#include "email.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAIL_STRING_SIZE+1];
	EMAIL_MERC_LEVEL_UP_VALUES	curEmailMercLevelUp;
	std::array<EMAIL_MERC_LEVEL_UP_VALUES, NUM_PROFILES>* records;
	std::array<bool, NUM_PROFILES>* seen;
	bool valid;
	bool hasIndex;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}

typedef emailMercLevelUpParseData;

static void XMLCALL
emailMercLevelUpStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	emailMercLevelUpParseData * pData = (emailMercLevelUpParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "EMAIL_MERC_LEVEL_UP") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "EMAIL") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->curEmailMercLevelUp = EMAIL_MERC_LEVEL_UP_VALUES{};
			pData->hasIndex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
			    strcmp(name, "Subject") == 0 ||
				strcmp(name, "Message") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
emailMercLevelUpCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	emailMercLevelUpParseData * pData = (emailMercLevelUpParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}


static void XMLCALL
emailMercLevelUpEndElementHandle(void *userData, const XML_Char *name)
{
	emailMercLevelUpParseData * pData = (emailMercLevelUpParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "EMAIL_MERC_LEVEL_UP") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "EMAIL") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->records->size(),
					pData->curEmailMercLevelUp.uiIndex))
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curEmailMercLevelUp.uiIndex;
				if ((*pData->seen)[index])
					pData->valid = false;
				else if (pData->valid)
				{
					(*pData->records)[index] = pData->curEmailMercLevelUp;
					(*pData->seen)[index] = true;
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curEmailMercLevelUp.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "Subject") == 0 )
		{
			pData->curElement = ELEMENT;

			pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curEmailMercLevelUp.szSubject);
		}
		else if(strcmp(name, "Message") == 0 )
		{
			pData->curElement = ELEMENT;

			pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curEmailMercLevelUp.szMessage);
		}
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInEmailMercLevelUp(STR fileName, BOOLEAN localizedVersion)
{
	emailMercLevelUpParseData pData{};
	std::array<EMAIL_MERC_LEVEL_UP_VALUES, NUM_PROFILES> pending{};
	std::array<bool, NUM_PROFILES> seen{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading EmailMercLevelUp.xml" );

	std::copy_n(EmailMercLevelUpText, NUM_PROFILES, pending.begin());
	pData.records = &pending;
	pData.seen = &seen;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, emailMercLevelUpStartElementHandle,
		emailMercLevelUpEndElementHandle,
		emailMercLevelUpCharacterDataHandle};
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

	std::copy(pending.begin(), pending.end(), EmailMercLevelUpText);

	return( TRUE );
}
