#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

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
	IMP_VOICESET*	curArray;

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

	if ( (pData->currentDepth <= pData->maxReadDepth) &&
		 (strlen( pData->szCharData ) < MAX_CHAR_DATA_LENGTH) )
	{
		strncat( pData->szCharData, str, __min( (unsigned int)len, MAX_CHAR_DATA_LENGTH - strlen( pData->szCharData ) ) );
	}
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

			pData->curIMPVoice.exists = TRUE;

			gIMPVoice.push_back( pData->curIMPVoice );

			++pData->curIndex;
		}
		else if ( strcmp( name, "szVoiceSetName" ) == 0 )
		{
			pData->curElement = ELEMENT;

			MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, pData->curIMPVoice.szVoiceSetName, sizeof(pData->curIMPVoice.szVoiceSetName) / sizeof(pData->curIMPVoice.szVoiceSetName[0]) );
			pData->curIMPVoice.szVoiceSetName[sizeof(pData->curIMPVoice.szVoiceSetName) / sizeof(pData->curIMPVoice.szVoiceSetName[0]) - 1] = '\0';
		}
		else if ( strcmp( name, "voiceset" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curIMPVoice.voiceset = (UINT16)atol( pData->szCharData );
		}
		else if ( strcmp( name, "bSex" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curIMPVoice.bSex = (BOOLEAN)atol( pData->szCharData );
		}

		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInIMPVoices( STR fileName )
{
	impVoiceParseData pData;

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Loading IMPVoices.xml" );

	memset( &pData, 0, sizeof(pData) );

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

	return(TRUE);
}
