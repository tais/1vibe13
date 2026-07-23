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
	ComboMergeInfoStruct		curAttachmentComboMerge;
	ComboMergeInfoStruct *	curArray;
	UINT32			maxArraySize;

	UINT32			curAttIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef attachmentcombomergeParseData;

static void XMLCALL
attachmentcombomergeStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	attachmentcombomergeParseData * pData = (attachmentcombomergeParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "ATTACHMENTCOMBOMERGELIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			memset(pData->curArray,0,sizeof(ComboMergeInfoStruct)*pData->maxArraySize);

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "ATTACHMENTCOMBOMERGE") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			memset(&pData->curAttachmentComboMerge,0,sizeof(ComboMergeInfoStruct));
			pData->curAttIndex = 0;
			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "usItem") == 0 ||
				strstr(name, "usAttachment") != NULL ||
				strcmp(name, "usResult") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
attachmentcombomergeCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	attachmentcombomergeParseData * pData = (attachmentcombomergeParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
attachmentcombomergeEndElementHandle(void *userData, const XML_Char *name)
{
	attachmentcombomergeParseData * pData = (attachmentcombomergeParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "ATTACHMENTCOMBOMERGELIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "ATTACHMENTCOMBOMERGE") == 0)
		{
			pData->curElement = ELEMENT_LIST;

			if(pData->curAttachmentComboMerge.uiIndex < pData->maxArraySize)
			{
				pData->curArray[pData->curAttachmentComboMerge.uiIndex] = pData->curAttachmentComboMerge; //write the attachmentcombomerge into the table
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curAttachmentComboMerge.uiIndex	= (UINT32) strtoul(pData->szCharData, NULL, 0);
		}
		else if(strcmp(name, "usItem") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curAttachmentComboMerge.usItem	= (UINT16) atol(pData->szCharData);
		}
		else if(strstr(name, "usAttachment") != NULL)
		{
			pData->curElement = ELEMENT;
			if (pData->curAttIndex < MAX_DEFAULT_ATTACHMENTS)
				pData->curAttachmentComboMerge.usAttachment[pData->curAttIndex++] = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "usResult") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curAttachmentComboMerge.usResult	= (UINT16) atol(pData->szCharData);
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}




BOOLEAN ReadInAttachmentComboMergeStats(STR fileName)
{
	attachmentcombomergeParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading AttachmentComboMerges.xml" );

	memset(&pData,0,sizeof(pData));
	pData.curArray = AttachmentComboMerge;
	pData.maxArraySize = MAXITEMS;

	const LegacyXmlCallbacks callbacks{
		&pData, attachmentcombomergeStartElementHandle,
		attachmentcombomergeEndElementHandle,
		attachmentcombomergeCharacterDataHandle};
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
BOOLEAN WriteAttachmentComboMergeStats()
{
	//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"writeattachmentcombomergestats");
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( "TABLEDATA\\AttachmentComboMerge out.xml", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<ATTACHMENTCOMBOMERGELIST>\r\n");
		for(cnt = 0;cnt < MAXITEMS;cnt++)
		{

			FilePrintf(hFile,"\t<ATTACHMENTCOMBOMERGE>\r\n");

			FilePrintf(hFile,"\t\t<uiIndex>%d</uiIndex>\r\n",								cnt );
			FilePrintf(hFile,"\t\t<usItem>%d</usItem>\r\n",								AttachmentComboMerge[cnt].usItem	);
			FilePrintf(hFile,"\t\t<usAttachment1>%d</usAttachment1>\r\n",								AttachmentComboMerge[cnt].usAttachment[0]	);
			FilePrintf(hFile,"\t\t<usAttachment2>%d</usAttachment2>\r\n",								AttachmentComboMerge[cnt].usAttachment[1]	);
			FilePrintf(hFile,"\t\t<usResult>%d</usResult>\r\n",								AttachmentComboMerge[cnt].usResult	);

			FilePrintf(hFile,"\t</ATTACHMENTCOMBOMERGE>\r\n");
		}
		FilePrintf(hFile,"</ATTACHMENTCOMBOMERGELIST>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
