#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "FileMan.h"
#include "Item Types.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "GameSettings.h"
	#include "XML.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];

	UINT16			curMerge[6];
	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef mergeParseData;

static void XMLCALL
mergeStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	mergeParseData * pData = (mergeParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "MERGELIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "MERGE") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			//DebugMsg(TOPIC_JA2, DBG_LEVEL_3,"MergeStartElementHandle: setting memory for curMerge");

			memset(&pData->curMerge,0,sizeof(UINT16[4]));

			pData->maxReadDepth++; //we are not skipping this element
			pData->curIndex++;
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "firstItemIndex") == 0 ||
				strcmp(name, "secondItemIndex") == 0 ||
				strcmp(name, "firstResultingItemIndex") == 0 ||
				strcmp(name, "secondResultingItemIndex") == 0 ||
				strcmp(name, "APCost") == 0 ||
				strcmp(name, "mergeType") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
mergeCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	mergeParseData * pData = (mergeParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
mergeEndElementHandle(void *userData, const XML_Char *name)
{
	mergeParseData * pData = (mergeParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "MERGELIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "MERGE") == 0)
		{
			pData->curElement = ELEMENT_LIST;

			if(pData->curIndex < pData->maxArraySize)
			{
				//DebugMsg(TOPIC_JA2, DBG_LEVEL_3,"MergeStartElementHandle: writing merge to array");
				Merge[pData->curIndex][0] = pData->curMerge[0]; //write the merge into the table
				Merge[pData->curIndex][1] = pData->curMerge[1];
				Merge[pData->curIndex][2] = pData->curMerge[2];
				Merge[pData->curIndex][3] = pData->curMerge[3];
				Merge[pData->curIndex][4] = pData->curMerge[4];
				Merge[pData->curIndex][5] = pData->curMerge[5];
			}
		}
		else if(strcmp(name, "firstItemIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMerge[0] = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "secondItemIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMerge[1] = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "firstResultingItemIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMerge[2] = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "secondResultingItemIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMerge[3] = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "mergeType") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMerge[4] = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "APCost") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMerge[5] = (UINT16) atol(pData->szCharData);
			pData->curMerge[5] = (UINT16)DynamicAdjustAPConstants(pData->curMerge[5], pData->curMerge[5]);
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}




BOOLEAN ReadInMergeStats(STR fileName)
{
	mergeParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading Merges.xml" );

	memset(&pData,0,sizeof(pData));
	pData.maxArraySize = MAXITEMS;
	pData.curIndex = -1;

	const LegacyXmlCallbacks callbacks{
		&pData, mergeStartElementHandle, mergeEndElementHandle,
		mergeCharacterDataHandle};
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
BOOLEAN WriteMergeStats()
{
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( "TABLEDATA\\Merges out.xml", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<MERGELIST>\r\n");
		for(cnt = 0;cnt < MAXITEMS;cnt++)
		{
			FilePrintf(hFile,"\t<MERGE>\r\n");

			FilePrintf(hFile,"\t\t<firstItemIndex>%d</firstItemIndex>\r\n",						Merge[cnt][0]);
			FilePrintf(hFile,"\t\t<secondItemIndex>%d</secondItemIndex>\r\n",							Merge[cnt][1]);
			FilePrintf(hFile,"\t\t<firstResultingItemIndex>%d</firstResultingItemIndex>\r\n",							Merge[cnt][2]);
			FilePrintf(hFile,"\t\t<secondResultingItemIndex>%d</secondResultingItemIndex>\r\n",							Merge[cnt][3]);
			FilePrintf(hFile,"\t\t<mergeType>%d</mergeType>\r\n",							Merge[cnt][4]);
			FilePrintf(hFile,"\t\t<APCost>%d</APCost>\r\n",							Merge[cnt][5]);

			FilePrintf(hFile,"\t</MERGE>\r\n");
		}
		FilePrintf(hFile,"</MERGELIST>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
