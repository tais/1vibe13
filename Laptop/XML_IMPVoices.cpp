#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include "sgp.h"
#include "Debug Control.h"
#include "expat.h"
#include "XML.h"
#include "IMP Confirm.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH + 1];
	IMP_VOICESET	curIMPVoice;
	std::vector<IMP_VOICESET>* voices;
	bool valid;
	bool hasName;
	bool hasVoiceSet;
	bool hasSex;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef impVoiceParseData;

std::vector<IMP_VOICESET> gIMPVoice;

static void XMLCALL
impVoicesStartElementHandle( void *userData, const XML_Char *name, const XML_Char **atts )
{
	impVoiceParseData * pData = (impVoiceParseData *)userData;

	if ( pData->currentDepth <= pData->maxReadDepth ) //are we reading this element?
	{
		if ( strcmp( name, "IMPVOICELIST" ) == 0 && pData->curElement == ELEMENT_NONE )
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if ( strcmp( name, "VOICE" ) == 0 && pData->curElement == ELEMENT_LIST )
		{
			pData->curElement = ELEMENT;
			pData->curIMPVoice = IMP_VOICESET{};
			pData->hasName = false;
			pData->hasVoiceSet = false;
			pData->hasSex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if ( pData->curElement == ELEMENT &&
				  (strcmp( name, "szVoiceSetName" ) == 0 ||
				  strcmp( name, "voiceset" ) == 0 ||
				  strcmp( name, "bSex" ) == 0 ) )
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;
}

static void XMLCALL
impVoicesCharacterDataHandle( void *userData, const XML_Char *str, int len )
{
	impVoiceParseData * pData = (impVoiceParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}

static void XMLCALL
impVoicesEndElementHandle( void *userData, const XML_Char *name )
{
	impVoiceParseData * pData = (impVoiceParseData *)userData;

	if ( pData->currentDepth <= pData->maxReadDepth )
	{
		if ( strcmp( name, "IMPVOICELIST" ) == 0 )
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if ( strcmp( name, "VOICE" ) == 0 )
		{
			pData->curElement = ELEMENT_LIST;

			if (!pData->hasName || !pData->hasVoiceSet || !pData->hasSex)
				pData->valid = false;
			if (pData->valid)
			{
				pData->curIMPVoice.exists = TRUE;
				pData->voices->push_back(pData->curIMPVoice);
			}

			++pData->curIndex;
		}
		else if ( strcmp( name, "szVoiceSetName" ) == 0 )
		{
			pData->curElement = ELEMENT;

			pData->hasName = LaptopLocalization::ConvertUtf8(
				pData->szCharData, pData->curIMPVoice.szVoiceSetName);
			pData->valid = pData->valid && pData->hasName;
		}
		else if ( strcmp( name, "voiceset" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->hasVoiceSet = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curIMPVoice.voiceset);
			pData->valid = pData->valid && pData->hasVoiceSet;
		}
		else if ( strcmp( name, "bSex" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->hasSex = LaptopLocalizationModel::ParseBoolean(
				pData->szCharData, pData->curIMPVoice.bSex);
			pData->valid = pData->valid && pData->hasSex;
		}

		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInIMPVoices( STR fileName )
{
	impVoiceParseData pData{};
	std::vector<IMP_VOICESET> pendingVoices;

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Loading IMPVoices.xml" );

	pData.voices = &pendingVoices;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, impVoicesStartElementHandle, impVoicesEndElementHandle,
		impVoicesCharacterDataHandle};
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

	gIMPVoice.swap(pendingVoices);

	return(TRUE);
}
