#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include <algorithm>
#include <array>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "aim.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	AIM_AVAILABLE	curAimAvailability;
	std::array<AIM_AVAILABLE, NUM_PROFILES>* records;
	std::array<AIM_AVAILABLE_TEMP, NUM_PROFILES>* temporaryRecords;
	std::array<bool, NUM_PROFILES>* seen;
	bool valid;
	bool hasIndex;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef aimAvailabilityParseData;

static void XMLCALL
aimAvailabilityStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	aimAvailabilityParseData * pData = (aimAvailabilityParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "AIM_AVAILABLES") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "AIM") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->curAimAvailability = AIM_AVAILABLE{};
			pData->hasIndex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
			    strcmp(name, "ProfilId") == 0 ||
				strcmp(name, "AimBioID") == 0 ||
				strcmp(name, "AIMBioID") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
aimCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	aimAvailabilityParseData * pData = (aimAvailabilityParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}


static void XMLCALL
aimAvailabilityEndElementHandle(void *userData, const XML_Char *name)
{
	aimAvailabilityParseData * pData = (aimAvailabilityParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "AIM_AVAILABLES") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "AIM") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->records->size(),
					pData->curAimAvailability.uiIndex))
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curAimAvailability.uiIndex;
				if ((*pData->seen)[index])
					pData->valid = false;
				else if (pData->valid)
				{
					auto record = pData->curAimAvailability;
					record.ubAimArrayID = index;
					(*pData->records)[index] = record;
					auto& temporary = (*pData->temporaryRecords)[index];
					temporary.uiIndex = index;
					temporary.ProfilId = record.ProfilId;
					temporary.ubAimArrayID = index;
					temporary.AimBio = record.AimBio;
					(*pData->seen)[index] = true;
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curAimAvailability.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "ProfilId") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseIntegerOrMinusOneSentinel(
					pData->szCharData, pData->curAimAvailability.ProfilId);
		}	
		else if(strcmp(name, "AimBioID") == 0 ||
			strcmp(name, "AIMBioID") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseIntegerOrMinusOneSentinel(
					pData->szCharData, pData->curAimAvailability.AimBio);
		}	
		
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInAimAvailability(STR fileName, BOOLEAN localizedVersion)
{
	aimAvailabilityParseData pData{};
	std::array<AIM_AVAILABLE, NUM_PROFILES> pending{};
	std::array<AIM_AVAILABLE_TEMP, NUM_PROFILES> pendingTemporary{};
	std::array<bool, NUM_PROFILES> seen{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading AimAvailability.xml" );

	std::copy_n(gAimAvailability, pending.size(), pending.begin());
	std::copy_n(gAimAvailabilityTemp, pendingTemporary.size(),
		pendingTemporary.begin());
	pData.records = &pending;
	pData.temporaryRecords = &pendingTemporary;
	pData.seen = &seen;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, aimAvailabilityStartElementHandle, aimAvailabilityEndElementHandle,
		aimCharacterDataHandle};
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
	if (!pData.valid)
		return FALSE;

	std::copy(pending.begin(), pending.end(), gAimAvailability);
	std::copy(pendingTemporary.begin(), pendingTemporary.end(),
		gAimAvailabilityTemp);

	return( TRUE );
}


BOOLEAN WriteAimAvailability(STR fileName)
{
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( fileName, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<AIM_AVAILABLES>\r\n");
		for(cnt = 0;cnt < NUM_PROFILES;cnt++)
		{

			FilePrintf(hFile,"\t<AIM>\r\n");

			FilePrintf(hFile,"\t\t<uiIndex>%d</uiIndex>\r\n", cnt);
		//	FilePrintf(hFile,"\t\t<ProfilId>-1</ProfilId>\r\n");
		//	FilePrintf(hFile,"\t\t<AIMBioID>0</AIMBioID>\r\n");
		
			FilePrintf(hFile,"\t\t<ProfilId>%d</ProfilId>\r\n", gAimAvailability[cnt].ProfilId);
			FilePrintf(hFile,"\t\t<AIMBioID>%d</AIMBioID>\r\n", gAimAvailability[cnt].AimBio);
			FilePrintf(hFile,"\t</AIM>\r\n");
		}
		FilePrintf(hFile,"</AIM_AVAILABLES>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
