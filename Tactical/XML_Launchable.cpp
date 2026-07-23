#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "FileMan.h"
#include "Item Types.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];

	UINT16			curLaunchable[2];
	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef launchableParseData;

UINT32 gMAXLAUNCHABLES_READ = 0;

static void XMLCALL
launchableStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	launchableParseData * pData = (launchableParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "LAUNCHABLELIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "LAUNCHABLE") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			//DebugMsg(TOPIC_JA2, DBG_LEVEL_3,"LaunchableStartElementHandle: setting memory for curLaunchable");

			memset(&pData->curLaunchable,0,sizeof(UINT16[2]));

			pData->maxReadDepth++; //we are not skipping this element
			pData->curIndex++;
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "launchableIndex") == 0 ||
				strcmp(name, "itemIndex") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
launchableCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	launchableParseData * pData = (launchableParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
launchableEndElementHandle(void *userData, const XML_Char *name)
{
	launchableParseData * pData = (launchableParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "LAUNCHABLELIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "LAUNCHABLE") == 0)
		{
			pData->curElement = ELEMENT_LIST;

			if(pData->curIndex < pData->maxArraySize)
			{
				//DebugMsg(TOPIC_JA2, DBG_LEVEL_3,"LaunchableStartElementHandle: writing launchable to array");
				Launchable[pData->curIndex][0] = pData->curLaunchable[0]; //write the launchable into the table
				Launchable[pData->curIndex][1] = pData->curLaunchable[1];
			}
		}
		else if(strcmp(name, "launchableIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curLaunchable[0] = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "itemIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curLaunchable[1] = (UINT16) atol(pData->szCharData);
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}




BOOLEAN ReadInLaunchableStats(STR fileName)
{
	launchableParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading Launchables.xml" );

	memset(&pData,0,sizeof(pData));
	pData.maxArraySize = MAXITEMS;
	pData.curIndex = -1;

	const LegacyXmlCallbacks callbacks{
		&pData, launchableStartElementHandle, launchableEndElementHandle,
		launchableCharacterDataHandle};
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

	gMAXLAUNCHABLES_READ = pData.curIndex + 1;

	return( TRUE );
}
BOOLEAN WriteLaunchableStats()
{
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( "TABLEDATA\\Launchables out.xml", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<LAUNCHABLELIST>\r\n");
		for(cnt = 0;cnt < MAXITEMS;cnt++)
		{
			FilePrintf(hFile,"\t<LAUNCHABLE>\r\n");

			FilePrintf(hFile,"\t\t<launchableIndex>%d</launchableIndex>\r\n",						Launchable[cnt][0]);
			FilePrintf(hFile,"\t\t<itemIndex>%d</itemIndex>\r\n",							Launchable[cnt][1]);

			FilePrintf(hFile,"\t</LAUNCHABLE>\r\n");
		}
		FilePrintf(hFile,"</LAUNCHABLELIST>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
