#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	HIDDEN_NAMES_VALUES	curHiddenNames;
	HIDDEN_NAMES_VALUES *	curArray;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef hiddenNamesParseData;

static void XMLCALL
hiddenNamesStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	hiddenNamesParseData * pData = (hiddenNamesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "HIDDENNAMES") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			memset(pData->curArray,0,sizeof(HIDDEN_NAMES_VALUES)*pData->maxArraySize);

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "NAME") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			memset(&pData->curHiddenNames,0,sizeof(HIDDEN_NAMES_VALUES));

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "Hidden") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
hiddenNamesCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	hiddenNamesParseData * pData = (hiddenNamesParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
hiddenNamesEndElementHandle(void *userData, const XML_Char *name)
{
	hiddenNamesParseData * pData = (hiddenNamesParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "HIDDENNAMES") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "NAME") == 0)
		{
			pData->curElement = ELEMENT_LIST;

			if(pData->curHiddenNames.uiIndex < pData->maxArraySize)
			{
				pData->curArray[pData->curHiddenNames.uiIndex] = pData->curHiddenNames; //write the hidden names into the table
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curHiddenNames.uiIndex	= (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "Hidden") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curHiddenNames.Hidden	= (BOOLEAN) atol(pData->szCharData);
		}	
		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}




BOOLEAN ReadInHiddenNamesStats(HIDDEN_NAMES_VALUES *pHiddenNames, STR fileName)
{
	hiddenNamesParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading HiddenNames.xml" );

	memset(&pData,0,sizeof(pData));
	pData.curArray = pHiddenNames;
	pData.maxArraySize = 500;

	const LegacyXmlCallbacks callbacks{
		&pData, hiddenNamesStartElementHandle, hiddenNamesEndElementHandle,
		hiddenNamesCharacterDataHandle};
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

	return( TRUE );
}

BOOLEAN WriteHiddenNamesStats(HIDDEN_NAMES_VALUES *pHiddenNames, STR fileName)
{
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( fileName, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<HIDDENNAMES>\r\n");
		for(cnt = 0;cnt < 500;cnt++)
		{

			FilePrintf(hFile,"\t<NAME>\r\n");

			FilePrintf(hFile,"\t\t<uiIndex>%d</uiIndex>\r\n", cnt);
			FilePrintf(hFile,"\t\t<Hidden>%d</Hidden>\r\n", pHiddenNames[cnt].Hidden);
			FilePrintf(hFile,"\t</NAME>\r\n");
		}
		FilePrintf(hFile,"</HIDDENNAMES>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}

//Restart Game and New Game
void LoadHiddenNames()
{
	char fileName[MAX_PATH];
	
	strcpy(fileName, TABLEDATA_DIRECTORY);
	strcat(fileName, HIDDENNAMESFILENAME);
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("LoadExternalGameplayData, fileName = %s", fileName));
	SGP_THROW_IFFALSE(ReadInHiddenNamesStats(zHiddenNames,fileName), HIDDENNAMESFILENAME);	
}
