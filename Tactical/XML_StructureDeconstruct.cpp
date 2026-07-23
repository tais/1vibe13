#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "sgp.h"
	#include "Overhead.h"
	#include "Handle Items.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8			szCharData[MAX_CHAR_DATA_LENGTH+1];
	STRUCTURE_DECONSTRUCT *		curArray;
	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef structuredeconstructParseData;

static void XMLCALL
structuredeconstructStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	structuredeconstructParseData * pData = (structuredeconstructParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "STRUCTURESLIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "STRUCTURE") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "usDeconstructItem") == 0 ||
				strcmp( name, "usItemToCreate" ) == 0 ||
				strcmp( name, "usCreatedItemStatus" ) == 0 ||
				strcmp( name, "szTileSetDisplayName" ) == 0 ||
				strcmp( name, "szTileSetName") == 0 ||
				strcmp( name, "dCreationCost" ) == 0 ||
				strcmp( name, "allowedtile") == 0 )) 
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
structuredeconstructCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	structuredeconstructParseData * pData = (structuredeconstructParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
structuredeconstructEndElementHandle(void *userData, const XML_Char *name)
{
	static std::vector<UINT8> statictilevector;

	structuredeconstructParseData * pData = (structuredeconstructParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "STRUCTURESLIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "STRUCTURE") == 0)
		{
			pData->curElement = ELEMENT_LIST;

			if ( pData->curIndex < pData->maxArraySize )
			{
				// for whatever reasons the game crashes in VS2008 Release builds when copying over the tilevector
				// this seems odd, as this works just fine in VS2010 and VS2013, and also works in VS205 debug builds
				// for now, copy over the content by hand
				// check if the vector is empty because assigning an empty vector will crash VS2010 debug builds!
				if ( !statictilevector.empty( ) )
				{
					pData->curArray[pData->curIndex].tilevector = statictilevector;
					statictilevector.clear( );
				}
			}

			pData->curIndex++;
		}
		else if ( strcmp( name, "usDeconstructItem" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curArray[pData->curIndex].usDeconstructItem = (UINT16)atol( pData->szCharData );
		}
		else if(strcmp(name, "usItemToCreate") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curArray[pData->curIndex].usItemToCreate = (UINT16)atol( pData->szCharData );
		}
		else if ( strcmp( name, "usCreatedItemStatus" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curArray[pData->curIndex].usCreatedItemStatus = (UINT8)atol( pData->szCharData );
		}
		else if(strcmp(name, "szTileSetDisplayName") == 0)
		{
			pData->curElement = ELEMENT;

			strncpy( pData->curArray[pData->curIndex].szTileSetDisplayName, pData->szCharData, 20 );
		}
		else if ( strcmp( name, "szTileSetName" ) == 0 )
		{
			pData->curElement = ELEMENT;

			strncpy( pData->curArray[pData->curIndex].szTileSetName, pData->szCharData, 20 );
		}
		else if ( strcmp( name, "dCreationCost" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curArray[pData->curIndex].dCreationCost = (FLOAT)atof( pData->szCharData );
		}
		else if(strcmp(name, "allowedtile") == 0)
		{
			pData->curElement = ELEMENT;
			statictilevector.push_back( (UINT8) atol(pData->szCharData) );
		}
		
		pData->maxReadDepth--;
	}

	pData->currentDepth--;
}


BOOLEAN ReadInStructureDeconstructStats(STR fileName)
{
	structuredeconstructParseData pData;

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading StructureDeconstruct.xml" );

	memset(&pData,0,sizeof(pData));
	pData.curArray = gStructureDeconstruct;
	pData.curIndex = 0;
	pData.maxArraySize = STRUCTURE_DECONSTRUCT_MAX;

	const LegacyXmlCallbacks callbacks{
		&pData, structuredeconstructStartElementHandle, structuredeconstructEndElementHandle,
		structuredeconstructCharacterDataHandle};
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

BOOLEAN WriteStructureDeconstructStats()
{
	//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"writefoodsstats");
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( "TABLEDATA\\StructureDeconstruct out.xml", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		FilePrintf(hFile,"<STRUCTURESLIST>\r\n");
		for ( UINT32 cnt = 0; cnt < STRUCTURE_DECONSTRUCT_MAX; ++cnt )
		{
			FilePrintf(hFile,"\t<STRUCTURE>\r\n");

			FilePrintf(hFile,"\t\t<usDeconstructItem>%d</usDeconstructItem>\r\n",	gStructureDeconstruct[cnt].usDeconstructItem );
			FilePrintf(hFile,"\t\t<usItemToCreate>%d</usItemToCreate>\r\n",			gStructureDeconstruct[cnt].usItemToCreate );
			FilePrintf(hFile,"\t\t<usCreatedItemStatus>%d</usCreatedItemStatus>\r\n", gStructureDeconstruct[cnt].usCreatedItemStatus );
			FilePrintf(hFile,"\t\t<szTileSetDisplayName>%s</szTileSetDisplayName>\r\n", gStructureDeconstruct[cnt].szTileSetDisplayName );
			FilePrintf(hFile,"\t\t<szTileSetName>%s</szTileSetName>\r\n",			gStructureDeconstruct[cnt].szTileSetName	);

			FilePrintf(hFile,"\t</STRUCTURE>\r\n");
		}
		FilePrintf(hFile,"</STRUCTURESLIST>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
