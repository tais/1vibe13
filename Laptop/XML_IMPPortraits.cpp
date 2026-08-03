#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

	#include "LocalizationInputAdapter.h"

	#include <algorithm>
	#include <array>

	#include "sgp.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Interface.h"
	#include "IMP Confirm.h"
	#include "Soldier Profile.h"

struct
{
	PARSE_STAGE	curElement;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];
	IMP_FACE_VALUES	curIMPPortraits;
	std::array<IMP_FACE_VALUES, NUM_PROFILES>* records;
	std::array<bool, NUM_PROFILES>* seen;
	bool valid;
	bool hasIndex;

	UINT32			maxArraySize;
	UINT32			curIndex;
	UINT32			currentDepth;
	UINT32			maxReadDepth;
	//CHAR16 gzIMPPortraits[MAX_IMP_NAMES_CHARS];
}
typedef impPortraitsParseData;

IMP_FACE_VALUES gIMPFaceValues[NUM_PROFILES];

static void XMLCALL
impPortraitsStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	impPortraitsParseData * pData = (impPortraitsParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "IMPPORTRAITSLIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "IMP") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->curIMPPortraits = IMP_FACE_VALUES{};
			pData->hasIndex = false;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
			   (strcmp(name, "uiIndex") == 0 ||
			    strcmp(name, "PortraitId") == 0 ||
				strcmp(name, "bSex") == 0 ||
				strcmp(name, "usEyesX") == 0 ||
				strcmp(name, "usEyesY") == 0 ||
				strcmp(name, "usMouthY") == 0 ||
				strcmp(name, "usMouthX") == 0 ||
				strcmp(name, "DefaultSkin") == 0 ||
				strcmp(name, "DefaultHair") == 0 ||
				strcmp(name, "DefaultShirt") == 0 ||
				strcmp(name, "DefaultPants") == 0 ||
				strcmp(name, "DefaultBigBody") == 0 ))
		{
			pData->curElement = ELEMENT_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;
}

static void XMLCALL
impPortraitsCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	impPortraitsParseData * pData = (impPortraitsParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
		pData->valid = LaptopLocalizationModel::AppendText(
			pData->szCharData, str, len);
}

static void XMLCALL
impPortraitsEndElementHandle(void *userData, const XML_Char *name)
{
	impPortraitsParseData * pData = (impPortraitsParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "IMPPORTRAITSLIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
		}
		else if(strcmp(name, "IMP") == 0)
		{
			pData->curElement = ELEMENT_LIST;	
			
			if (!pData->hasIndex ||
				!LaptopLocalizationModel::IsIndexInRange(
					pData->records->size(), pData->curIMPPortraits.uiIndex))
			{
				pData->valid = false;
			}
			else
			{
				const auto index = pData->curIMPPortraits.uiIndex;
				if ((*pData->seen)[index])
					pData->valid = false;
				else if (pData->valid)
				{
					(*pData->records)[index] = pData->curIMPPortraits;
					(*pData->seen)[index] = true;
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->hasIndex = LaptopLocalizationModel::ParseInteger(
				pData->szCharData, pData->curIMPPortraits.uiIndex);
			pData->valid = pData->valid && pData->hasIndex;
		}
		else if(strcmp(name, "PortraitId") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.PortraitId);
		}
		else if(strcmp(name, "bSex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseBoolean(
					pData->szCharData, pData->curIMPPortraits.bSex);
		}
		else if(strcmp(name, "usEyesX") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.uiEyeXPositions);
		}
		else if(strcmp(name, "usEyesY") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.uiEyeYPositions);
		}
		else if(strcmp(name, "usMouthX") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.uiMouthXPositions);
		}
		else if(strcmp(name, "usMouthY") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.uiMouthYPositions);
		}
		else if(strcmp(name, "DefaultSkin") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.iCurrentSkin);
		}		
		else if(strcmp(name, "DefaultHair") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.iCurrentHair);
		}
		else if(strcmp(name, "DefaultShirt") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.iCurrentShirt);
		}	
		else if(strcmp(name, "DefaultPants") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseInteger(
					pData->szCharData, pData->curIMPPortraits.iCurrentPants);
		}		
		else if(strcmp(name, "DefaultBigBody") == 0)
		{
			pData->curElement = ELEMENT;
			pData->valid = pData->valid &&
				LaptopLocalizationModel::ParseBoolean(
					pData->szCharData, pData->curIMPPortraits.bBigBody);
		}	
	
		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}

BOOLEAN ReadInIMPPortraits(STR fileName, BOOLEAN localizedVersion)
{
	impPortraitsParseData pData{};
	std::array<IMP_FACE_VALUES, NUM_PROFILES> pending{};
	std::array<bool, NUM_PROFILES> seen{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading IMPPortraits.xml" );

	if (localizedVersion)
		std::copy_n(gIMPFaceValues, pending.size(), pending.begin());
	pData.records = &pending;
	pData.seen = &seen;
	pData.valid = true;

	const LegacyXmlCallbacks callbacks{
		&pData, impPortraitsStartElementHandle, impPortraitsEndElementHandle,
		impPortraitsCharacterDataHandle};
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

	std::copy(pending.begin(), pending.end(), gIMPFaceValues);

	return( TRUE );
}

void LoadIMPPortraitsTEMP()
{
	UINT32 cnt2 = 0;
	std::fill_n(gIMPValues, NUM_PROFILES, IMP_VALUES{});

	for ( UINT32 cnt = 0; cnt < NUM_PROFILES; ++cnt )
	{
		if ( gIMPFaceValues[cnt].PortraitId !=0)
		{
			gIMPValues[cnt2].uiIndex = cnt2;					
			gIMPValues[cnt2].Enabled = 1;					
			gIMPValues[cnt2].uiEyeXPositions = gIMPFaceValues[cnt].uiEyeXPositions;
			gIMPValues[cnt2].uiEyeYPositions = gIMPFaceValues[cnt].uiEyeYPositions;
			gIMPValues[cnt2].uiMouthXPositions = gIMPFaceValues[cnt].uiMouthXPositions;
			gIMPValues[cnt2].uiMouthYPositions = gIMPFaceValues[cnt].uiMouthYPositions;
			gIMPValues[cnt2].PortraitId = gIMPFaceValues[cnt].PortraitId;
			gIMPValues[cnt2].bSex = gIMPFaceValues[cnt].bSex;
			gIMPValues[cnt2].iCurrentSkin = gIMPFaceValues[cnt].iCurrentSkin;
			gIMPValues[cnt2].iCurrentShirt = gIMPFaceValues[cnt].iCurrentShirt;
			gIMPValues[cnt2].iCurrentHair = gIMPFaceValues[cnt].iCurrentHair;
			gIMPValues[cnt2].iCurrentPants = gIMPFaceValues[cnt].iCurrentPants;
			gIMPValues[cnt2].bBigBody = gIMPFaceValues[cnt].bBigBody;
			++cnt2;
		}
	}
}
