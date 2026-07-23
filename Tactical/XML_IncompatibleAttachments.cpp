#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "FileMan.h"
#include "Item Types.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"

// Flugente: in order not to loop over MAXATTACHMENTS entries in IncompatibleAttachments[] if we only have a few thousand, remember the actual number read in
UINT32 gINCOMPATIBLEATTACHMENTS_READ = 0;

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];

	UINT16			curIncompatibleAttachment[2];
	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef incompatibleattachmentParseData;

static void XMLCALL
incompatibleattachmentStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	incompatibleattachmentParseData * pData = (incompatibleattachmentParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "INCOMPATIBLEATTACHMENTLIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "INCOMPATIBLEATTACHMENT") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			//DebugMsg(TOPIC_JA2, DBG_LEVEL_3,"IncompatibleAttachmentStartElementHandle: setting memory for curIncompatibleAttachment");

			memset(&pData->curIncompatibleAttachment,0,sizeof(UINT16[2]));

			pData->maxReadDepth++; //we are not skipping this element
			pData->curIndex++;
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "incompatibleattachmentIndex") == 0 ||
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
incompatibleattachmentCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	incompatibleattachmentParseData * pData = (incompatibleattachmentParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
incompatibleattachmentEndElementHandle(void *userData, const XML_Char *name)
{
	incompatibleattachmentParseData * pData = (incompatibleattachmentParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "INCOMPATIBLEATTACHMENTLIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "INCOMPATIBLEATTACHMENT") == 0)
		{
			pData->curElement = ELEMENT_LIST;

			// Flugente: new entry
			gINCOMPATIBLEATTACHMENTS_READ = pData->curIndex;

			if(pData->curIndex < pData->maxArraySize)
			{
				//DebugMsg(TOPIC_JA2, DBG_LEVEL_3,"IncompatibleAttachmentStartElementHandle: writing incompatibleattachment to array");
				IncompatibleAttachments[pData->curIndex][0] = pData->curIncompatibleAttachment[0]; //write the incompatibleattachment into the table
				IncompatibleAttachments[pData->curIndex][1] = pData->curIncompatibleAttachment[1];
			}
		}
		else if(strcmp(name, "incompatibleattachmentIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curIncompatibleAttachment[1] = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "itemIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curIncompatibleAttachment[0] = (UINT16) atol(pData->szCharData);
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}




BOOLEAN ReadInIncompatibleAttachmentStats(STR fileName)
{
	incompatibleattachmentParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading IncompatibleAttachments.xml" );

	memset(&pData,0,sizeof(pData));
	pData.maxArraySize = MAXATTACHMENTS;
	pData.curIndex = -1;

	const LegacyXmlCallbacks callbacks{
		&pData, incompatibleattachmentStartElementHandle,
		incompatibleattachmentEndElementHandle,
		incompatibleattachmentCharacterDataHandle};
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

	// read was x -> x+1 entries
	++gINCOMPATIBLEATTACHMENTS_READ;

	return( TRUE );
}
BOOLEAN WriteIncompatibleAttachmentStats()
{
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( "TABLEDATA\\IncompatibleAttachments out.xml", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<INCOMPATIBLEATTACHMENTLIST>\r\n");
		for(cnt = 0;cnt < MAXATTACHMENTS;cnt++)
		{
			FilePrintf(hFile,"\t<INCOMPATIBLEATTACHMENT>\r\n");

			FilePrintf(hFile,"\t\t<itemIndex>%d</itemIndex>\r\n",							IncompatibleAttachments[cnt][0]);
			FilePrintf(hFile,"\t\t<incompatibleattachmentIndex>%d</incompatibleattachmentIndex>\r\n",						IncompatibleAttachments[cnt][1]);

			FilePrintf(hFile,"\t</INCOMPATIBLEATTACHMENT>\r\n");
		}
		FilePrintf(hFile,"</INCOMPATIBLEATTACHMENTLIST>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
