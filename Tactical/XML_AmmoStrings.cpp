#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "Overhead.h"
	#include "Text.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef ammoParseData;

static void XMLCALL
ammoStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	ammoParseData * pData = (ammoParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "AMMOLIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "AMMO") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			pData->maxReadDepth++; //we are not skipping this element
			pData->curIndex++;
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "AmmoCaliber") == 0 ||
				strcmp(name, "BRCaliber") == 0 ||
				strcmp(name, "NWSSCaliber") == 0))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
ammoCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	ammoParseData * pData = (ammoParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}

extern char NWSSCaliber[MAXITEMS][255];

static void XMLCALL
ammoEndElementHandle(void *userData, const XML_Char *name)
{
	ammoParseData * pData = (ammoParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "AMMOLIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "AMMO") == 0)
		{
			pData->curElement = ELEMENT_LIST;

		}
		else if(strcmp(name, "AmmoCaliber") == 0)
		{
			pData->curElement = ELEMENT;
			if(pData->curIndex < pData->maxArraySize)
			{
				MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, AmmoCaliber[pData->curIndex], sizeof(AmmoCaliber[0])/sizeof(AmmoCaliber[0][0]) );
				AmmoCaliber[pData->curIndex][sizeof(AmmoCaliber[0])/sizeof(AmmoCaliber[0][0]) - 1] = '\0';
			}

		}
		else if(strcmp(name, "BRCaliber") == 0)
		{
			pData->curElement = ELEMENT;
			if(pData->curIndex < pData->maxArraySize)
			{
				MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, BobbyRayAmmoCaliber[pData->curIndex], sizeof(BobbyRayAmmoCaliber[0])/sizeof(BobbyRayAmmoCaliber[0][0]) );
				BobbyRayAmmoCaliber[pData->curIndex][sizeof(BobbyRayAmmoCaliber[0])/sizeof(BobbyRayAmmoCaliber[0][0]) - 1] = '\0';
			}

		}
		else if (strcmp(name, "NWSSCaliber") == 0)
		{
			pData->curElement = ELEMENT_LIST;
			if (pData->curIndex < pData->maxArraySize)
			{
				if (strlen(pData->szCharData) < 255)
				{
					strcpy(NWSSCaliber[pData->curIndex], pData->szCharData);
				}
				else
				{
					strncpy(NWSSCaliber[pData->curIndex], pData->szCharData, 254);
					NWSSCaliber[pData->curIndex][254] = '\0';
				}
			}
		}

		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}

BOOLEAN ReadInAmmoStats(STR fileName)
{
	ammoParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading AmmoStrings.xml" );

	memset(&pData,0,sizeof(pData));
	pData.maxArraySize = MAXITEMS;
	pData.curIndex = -1;

	const LegacyXmlCallbacks callbacks{
		&pData, ammoStartElementHandle, ammoEndElementHandle,
		ammoCharacterDataHandle};
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
BOOLEAN WriteAmmoStats()
{
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( "TABLEDATA\\Ammos out.xml", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<AMMOLIST>\r\n");
		for(cnt = 0;cnt < MAXITEMS;cnt++)
		{
			FilePrintf(hFile,"\t<AMMO>\r\n");

			CHAR16 *szRemainder = AmmoCaliber[cnt]; //the remaining string to be output (for making valid XML)

			FilePrintf(hFile,"\t\t<AmmoCaliber>");
			while(szRemainder[0] != '\0')
			{
				UINT32 uiCharLoc = wcscspn(szRemainder,L"&<>\'\"\0");
				CHAR16 invChar = szRemainder[uiCharLoc];

				if(uiCharLoc)
				{
					szRemainder[uiCharLoc] = '\0';
					FilePrintf(hFile,"%S",szRemainder);
					szRemainder[uiCharLoc] = invChar;
				}

				szRemainder += uiCharLoc;

				switch(invChar)
				{
					case '&':
						FilePrintf(hFile,"&amp;");
						szRemainder++;
					break;

					case '<':
						FilePrintf(hFile,"&lt;");
						szRemainder++;
					break;

					case '>':
						FilePrintf(hFile,"&gt;");
						szRemainder++;
					break;

					case '\'':
						FilePrintf(hFile,"&apos;");
						szRemainder++;
					break;

					case '\"':
						FilePrintf(hFile,"&quot;");
						szRemainder++;
					break;
				}
			}
			FilePrintf(hFile,"</AmmoCaliber>\r\n");


			szRemainder = BobbyRayAmmoCaliber[cnt]; //the remaining string to be output (for making valid XML)

			FilePrintf(hFile,"\t\t<BRCaliber>");
			while(szRemainder[0] != '\0')
			{
				UINT32 uiCharLoc = wcscspn(szRemainder,L"&<>\'\"\0");
				CHAR16 invChar = szRemainder[uiCharLoc];

				if(uiCharLoc)
				{
					szRemainder[uiCharLoc] = '\0';
					FilePrintf(hFile,"%S",szRemainder);
					szRemainder[uiCharLoc] = invChar;
				}

				szRemainder += uiCharLoc;

				switch(invChar)
				{
					case '&':
						FilePrintf(hFile,"&amp;");
						szRemainder++;
					break;

					case '<':
						FilePrintf(hFile,"&lt;");
						szRemainder++;
					break;

					case '>':
						FilePrintf(hFile,"&gt;");
						szRemainder++;
					break;

					case '\'':
						FilePrintf(hFile,"&apos;");
						szRemainder++;
					break;

					case '\"':
						FilePrintf(hFile,"&quot;");
						szRemainder++;
					break;
				}
			}
			FilePrintf(hFile,"</BRCaliber>\r\n");


			FilePrintf(hFile,"\t</AMMO>\r\n");
		}
		FilePrintf(hFile,"</AMMOLIST>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
