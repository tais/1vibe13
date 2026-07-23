#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "FileMan.h"
#include "Item Types.h"
	#include "Weapons.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	MAGTYPE		curMagazine;
	MAGTYPE *	curArray;
	UINT32			maxArraySize;

	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef magazineParseData;

static void XMLCALL
magazineStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	magazineParseData * pData = (magazineParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "MAGAZINELIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			memset(pData->curArray,0,sizeof(MAGTYPE)*pData->maxArraySize);

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "MAGAZINE") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			memset(&pData->curMagazine,0,sizeof(MAGTYPE));

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "ubCalibre") == 0 ||
				strcmp(name, "ubMagSize") == 0 ||
				strcmp(name, "ubAmmoType") == 0 ||
				strcmp(name, "ubMagType") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
magazineCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	magazineParseData * pData = (magazineParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
magazineEndElementHandle(void *userData, const XML_Char *name)
{
	magazineParseData * pData = (magazineParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "MAGAZINELIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "MAGAZINE") == 0)
		{
			pData->curElement = ELEMENT_LIST;

			if(pData->curMagazine.uiIndex < pData->maxArraySize)
			{
				pData->curArray[pData->curMagazine.uiIndex] = pData->curMagazine; //write the magazine into the table
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMagazine.uiIndex	= (UINT32) strtoul(pData->szCharData, NULL, 0);
		}
		else if(strcmp(name, "ubCalibre") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMagazine.ubCalibre	= (UINT8) atol(pData->szCharData);
		}
		else if(strcmp(name, "ubMagSize") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMagazine.ubMagSize = (UINT16) atol(pData->szCharData);
		}
		else if(strcmp(name, "ubAmmoType") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMagazine.ubAmmoType	= (UINT8) atol(pData->szCharData);
		}
		else if(strcmp(name, "ubMagType") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curMagazine.ubMagType	= (UINT8) atol(pData->szCharData);
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}




BOOLEAN ReadInMagazineStats(STR fileName)
{
	magazineParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading Magazines.xml" );

	memset(&pData,0,sizeof(pData));
	pData.curArray = Magazine;
	pData.maxArraySize = MAXITEMS;

	const LegacyXmlCallbacks callbacks{
		&pData, magazineStartElementHandle, magazineEndElementHandle,
		magazineCharacterDataHandle};
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
BOOLEAN WriteMagazineStats()
{
	//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"writemagazinestats");
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( "TABLEDATA\\Magazine out.xml", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<MAGAZINELIST>\r\n");
		for(cnt = 0;cnt < MAXITEMS;cnt++)
		{

			FilePrintf(hFile,"\t<MAGAZINE>\r\n");

			FilePrintf(hFile,"\t\t<uiIndex>%d</uiIndex>\r\n",								cnt );
			FilePrintf(hFile,"\t\t<ubCalibre>%d</ubCalibre>\r\n",								Magazine[cnt].ubCalibre	);
			FilePrintf(hFile,"\t\t<ubMagSize>%d</ubMagSize>\r\n",								Magazine[cnt].ubMagSize	);
			FilePrintf(hFile,"\t\t<ubAmmoType>%d</ubAmmoType>\r\n",								Magazine[cnt].ubAmmoType	);
			FilePrintf(hFile,"\t\t<ubMagType>%d</ubMagType>\r\n",								Magazine[cnt].ubMagType	);

			FilePrintf(hFile,"\t</MAGAZINE>\r\n");
		}
		FilePrintf(hFile,"</MAGAZINELIST>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
