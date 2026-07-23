#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "Text.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	SENDER_NAMES_VALUES	curSenderNameList;
	SENDER_NAMES_VALUES *	curArray;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef senderNameListParseData;

BOOLEAN SenderNameList_TextOnly;

static void XMLCALL
senderNameListStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	senderNameListParseData * pData = (senderNameListParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "SENDER_LIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;
			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "NAME") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "Name") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
senderNameListCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	senderNameListParseData * pData = (senderNameListParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
senderNameListEndElementHandle(void *userData, const XML_Char *name)
{
	senderNameListParseData * pData = (senderNameListParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "SENDER_LIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "NAME") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			wcscpy(pSenderNameList[pData->curSenderNameList.uiIndex], pData->curSenderNameList.Name);
			
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curSenderNameList.uiIndex	= (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "Name") == 0 )
		{
			pData->curElement = ELEMENT;

			MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, pData->curSenderNameList.Name, sizeof(pData->curSenderNameList.Name)/sizeof(pData->curSenderNameList.Name[0]) );
			pData->curSenderNameList.Name[sizeof(pData->curSenderNameList.Name)/sizeof(pData->curSenderNameList.Name[0]) - 1] = '\0';
		}
		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}




BOOLEAN ReadInSenderNameList(STR fileName, BOOLEAN localizedVersion)
{
	senderNameListParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading SenderNameList.xml" );

	SenderNameList_TextOnly = localizedVersion;

	memset(&pData,0,sizeof(pData));

	const LegacyXmlCallbacks callbacks{
		&pData, senderNameListStartElementHandle, senderNameListEndElementHandle,
		senderNameListCharacterDataHandle};
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

	return( TRUE );
}
