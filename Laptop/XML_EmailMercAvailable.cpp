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
	EMAIL_MERC_AVAILABLE_VALUES	curEmailMercAvailable;
	std::array<EMAIL_MERC_AVAILABLE_VALUES, NUM_PROFILES>* records;
	std::array<bool, NUM_PROFILES>* seen;
	bool valid;
	bool hasIndex;
	bool hasSubject;
	bool hasMessage;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}

typedef emailMercAvailableParseData;

static void XMLCALL
emailMercAvailableStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	emailMercAvailableParseData * pData = (emailMercAvailableParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "EMAIL_MERC_AVAILABLE") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "EMAIL") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->curEmailMercAvailable = EMAIL_MERC_AVAILABLE_VALUES{};
			pData->hasIndex = false;
			pData->hasSubject = false;
			pData->hasMessage = false;

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
emailMercAvailableCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	emailMercAvailableParseData * pData = (emailMercAvailableParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}


static void XMLCALL
emailMercAvailableEndElementHandle(void *userData, const XML_Char *name)
{
	emailMercAvailableParseData * pData = (emailMercAvailableParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "EMAIL_MERC_AVAILABLE") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "EMAIL") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->records->size(),
					pData->curEmailMercAvailable.uiIndex))
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curEmailMercAvailable.uiIndex;
				if ((*pData->seen)[index])
					pData->valid = false;
				else if (pData->valid)
				{
					(*pData->records)[index] = pData->curEmailMercAvailable;
					(*pData->seen)[index] = true;
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData,
				pData->curEmailMercAvailable.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "Subject") == 0 )
		{
			pData->curElement = ELEMENT;

			pData->hasSubject = LaptopLocalization::ConvertUtf8(
				pData->szCharData,
				pData->curEmailMercAvailable.szSubject);
			pData->valid = pData->valid && pData->hasSubject;
		}
		else if(strcmp(name, "Message") == 0 )
		{
			pData->curElement = ELEMENT;

			pData->hasMessage = LaptopLocalization::ConvertUtf8(
				pData->szCharData,
				pData->curEmailMercAvailable.szMessage);
			pData->valid = pData->valid && pData->hasMessage;
		}
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInEmailMercAvailable(STR fileName, BOOLEAN localizedVersion)
{
	emailMercAvailableParseData pData{};
	std::array<EMAIL_MERC_AVAILABLE_VALUES, NUM_PROFILES> pending{};
	std::array<bool, NUM_PROFILES> seen{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading EmailMercAvailable.xml" );

	std::copy_n(EmailMercAvailableText, NUM_PROFILES, pending.begin());
	pData.records = &pending;
	pData.seen = &seen;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, emailMercAvailableStartElementHandle,
		emailMercAvailableEndElementHandle,
		emailMercAvailableCharacterDataHandle};
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

	std::copy(pending.begin(), pending.end(), EmailMercAvailableText);

	return( TRUE );
}
