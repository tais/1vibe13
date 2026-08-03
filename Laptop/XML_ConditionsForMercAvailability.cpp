#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include <algorithm>
#include <array>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "mercs.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	CONTITION_FOR_MERC_AVAILABLE	curMercAvailability;
	std::array<CONTITION_FOR_MERC_AVAILABLE, NUM_PROFILES>* records;
	std::array<CONTITION_FOR_MERC_AVAILABLE_TEMP, NUM_PROFILES>* temporaryRecords;
	std::array<bool, NUM_PROFILES>* seen;
	bool valid;
	bool hasIndex;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef mercAvailabilityParseData;

static void XMLCALL
mercAvailabilityStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	mercAvailabilityParseData * pData = (mercAvailabilityParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "MERC_AVAILABLES") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "MERC") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->curMercAvailability = CONTITION_FOR_MERC_AVAILABLE{};
			pData->hasIndex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
			    strcmp(name, "ProfilId") == 0 ||
				strcmp(name, "usMoneyPaid") == 0 ||
				strcmp(name, "Available") == 0 ||
				strcmp(name, "NewMercsAvailable") == 0 ||
				strcmp(name, "StartMercsAvailable") == 0 ||
				strcmp(name, "MercBioID") == 0 ||
				strcmp(name, "Drunk") == 0 ||
				strcmp(name, "uiAlternateIndex") == 0 ||
				strcmp(name, "usDay") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
mercCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	mercAvailabilityParseData * pData = (mercAvailabilityParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}


static void XMLCALL
mercAvailabilityEndElementHandle(void *userData, const XML_Char *name)
{
	mercAvailabilityParseData * pData = (mercAvailabilityParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "MERC_AVAILABLES") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "MERC") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->records->size(),
					pData->curMercAvailability.uiIndex) ||
				!LaptopLocalizationModel::IsIndexInRange(
					NUM_PROFILES, pData->curMercAvailability.ProfilId))
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curMercAvailability.uiIndex;
				if ((*pData->seen)[index])
					pData->valid = false;
				else if (pData->valid)
				{
					auto record = pData->curMercAvailability;
					record.ubMercArrayID = index;
					(*pData->records)[index] = record;
					auto& temporary = (*pData->temporaryRecords)[index];
					temporary.usMoneyPaid = record.usMoneyPaid;
					temporary.usDay = record.usDay;
					temporary.ubMercArrayID = index;
					temporary.uiIndex = index;
					temporary.ProfilId = record.ProfilId;
					temporary.NewMercsAvailable = record.NewMercsAvailable;
					temporary.StartMercsAvailable = record.StartMercsAvailable;
					temporary.MercBio = record.MercBio;
					temporary.Drunk = record.Drunk;
					temporary.uiAlternateIndex = record.uiAlternateIndex;
					(*pData->seen)[index] = true;
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curMercAvailability.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "usMoneyPaid") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curMercAvailability.usMoneyPaid);
		}
		else if(strcmp(name, "usDay") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curMercAvailability.usDay);
		}
		else if(strcmp(name, "ProfilId") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curMercAvailability.ProfilId);
		}
		else if(strcmp(name, "NewMercsAvailable") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseBoolean(
					pData->szCharData,
					pData->curMercAvailability.NewMercsAvailable);
		}	
		else if(strcmp(name, "StartMercsAvailable") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseBoolean(
					pData->szCharData,
					pData->curMercAvailability.StartMercsAvailable);
		}	
		else if(strcmp(name, "MercBioID") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curMercAvailability.MercBio);
		}	
		else if(strcmp(name, "Drunk") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseBoolean(
					pData->szCharData, pData->curMercAvailability.Drunk);
		}
		else if(strcmp(name, "uiAlternateIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseIntegerOrMinusOneSentinel(
					pData->szCharData,
					pData->curMercAvailability.uiAlternateIndex);
		}
		
		else if(pData->curElement == ELEMENT_PROPERTY)
		{
			// Recognized-but-unhandled leaf close (e.g. <Available>) -- pop back to ELEMENT so the
			// rest of this <MERC> record is not silently skipped/zeroed.
			pData->curElement = ELEMENT;
		}
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInMercAvailability(STR fileName, BOOLEAN localizedVersion)
{
	mercAvailabilityParseData pData{};
	std::array<CONTITION_FOR_MERC_AVAILABLE, NUM_PROFILES> pending{};
	std::array<CONTITION_FOR_MERC_AVAILABLE_TEMP, NUM_PROFILES>
		pendingTemporary{};
	std::array<bool, NUM_PROFILES> seen{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading MercAvailability.xml" );

	std::copy_n(gConditionsForMercAvailability, pending.size(),
		pending.begin());
	std::copy_n(gConditionsForMercAvailabilityTemp, pendingTemporary.size(),
		pendingTemporary.begin());
	pData.records = &pending;
	pData.temporaryRecords = &pendingTemporary;
	pData.seen = &seen;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, mercAvailabilityStartElementHandle, mercAvailabilityEndElementHandle,
		mercCharacterDataHandle};
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

	std::copy(pending.begin(), pending.end(),
		gConditionsForMercAvailability);
	std::copy(pendingTemporary.begin(), pendingTemporary.end(),
		gConditionsForMercAvailabilityTemp);

	return( TRUE );
}


BOOLEAN WriteMercAvailability(STR fileName)
{
	HWFILE		hFile;

	//Debug code; make sure that what we got from the file is the same as what's there
	// Open a new file
	hFile = FileOpen( fileName, FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
	if ( !hFile )
		return( FALSE );

	{
		UINT32 cnt;

		FilePrintf(hFile,"<MERC_AVAILABLES>\r\n");
		for(cnt = 0;cnt < NUM_PROFILES;cnt++)
		{

			FilePrintf(hFile,"\t<MERC>\r\n");

			FilePrintf(hFile,"\t\t<uiIndex>%d</uiIndex>\r\n", cnt);
			FilePrintf(hFile,"\t\t<usMoneyPaid>%d</usMoneyPaid>\r\n", gConditionsForMercAvailability[cnt].usMoneyPaid);
			FilePrintf(hFile,"\t\t<usDay>%d</usDay>\r\n", gConditionsForMercAvailability[cnt].usDay);
			FilePrintf(hFile,"\t\t<ProfilId>%d</ProfilId>\r\n", gConditionsForMercAvailability[cnt].ProfilId);
			FilePrintf(hFile,"\t</MERC>\r\n");
		}
		FilePrintf(hFile,"</MERC_AVAILABLES>\r\n");
	}
	FileClose( hFile );

	return( TRUE );
}
