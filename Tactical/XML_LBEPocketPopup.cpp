#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include <memory>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <map>
#include <stdexcept>

	#include "sgp.h"
	#include "popup_class.h"
	#include "popup_definition.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"

// namespace'd because of name collision with POPUP class def
namespace POPUP_PARSE {
	enum
	{
		OUTSIDE_POCKET_LIST = 0,
		POCKET_LIST,

		POCKET,
		POCKET_PROPERTY,	// for reading pocket id
		
		POPUP,
		POPUP_PROPERTY,	// unused
		// for root-level popup
		OPTION,
		OPTION_PROPERTY,
		GENERATOR,
		GENERATOR_PROPERTY,

		SUBMENU,
		SUBMENU_PROPERTY,
		// for submenus
		SUBMENU_OPTION,
		SUBMENU_OPTION_PROPERTY,
		SUBMENU_GENERATOR,
		SUBMENU_GENERATOR_PROPERTY
	}
	typedef POPUP_PARSE_STAGE;
}


struct
{
	POPUP_PARSE::POPUP_PARSE_STAGE	curElement;
	UINT8		curSubPopupLevel;

	CHAR8		szCharData[MAX_CHAR_DATA_LENGTH+1];

	// popupDef and its pocket number
	UINT8			curPocketId;
	popupDef		*curPocketPopup;

	// for reading in options to popups and subPopups
	//popupDefOption	*curPocketPopupOption;	// not really used
	WCHAR			curPocketPopupOptionName[128];
	UINT16			curPocketPopupOptionCallback;
	UINT16			curPocketPopupOptionAvail;

	// for reading in subPopups
	popupDefSubPopupOption *curPocketSubPopupOption[POPUP_MAX_SUB_POPUPS];
	WCHAR			curPocketSubPopupOptionName[POPUP_MAX_SUB_POPUPS][128];

	// for reading in content generator references
	UINT16			curPocketPopupGeneratorId;
	std::map<UINT8, popupDef> parsedPopups;

	UINT32			currentDepth;
	UINT32			maxReadDepth;
}
typedef pocketPopupParseData;

// maps generator name strings found in XML to generator IDs used by the function that binds them
static UINT16 mapGeneratorNameToId( const CHAR8 * name ){

	if( strcmp(name,"dummy") == 0 ){
		return popupGenerators::dummy;
	} else if( strcmp(name,"addArmor") == 0 ){
		return popupGenerators::addArmor;
	} else if( strcmp(name,"addLBE") == 0 ){
		return popupGenerators::addLBE;
	} else if( strcmp(name,"addWeapons") == 0 ){
		return popupGenerators::addWeapons;
	} else if( strcmp(name,"addWeaponGroups") == 0 ){
		return popupGenerators::addWeaponGroups;
	} else if( strcmp(name,"addGrenades") == 0 ){
		return popupGenerators::addGrenades;
	} else if( strcmp(name,"addBombs") == 0 ){
		return popupGenerators::addBombs;
	} else if( strcmp(name,"addFaceGear") == 0 ){
		return popupGenerators::addFaceGear;
	} else if( strcmp(name,"addAmmo") == 0 ){
		return popupGenerators::addAmmo;
	} else if( strcmp(name,"addRifleGrenades") == 0 ){
		return popupGenerators::addRifleGrenades;
	} else if( strcmp(name,"addRocketAmmo") == 0 ){
		return popupGenerators::addRocketAmmo;
	} else if( strcmp(name,"addMisc") == 0 ){
		return popupGenerators::addMisc;
	} else if( strcmp(name,"addKits") == 0 ){
		return popupGenerators::addKits;
	} else return 0; // includes 'none'

}

// maps option callback name strings found in XML to callback IDs used by the function that binds them
static UINT16 mapCallbackNameToId( const CHAR8 * name ){
	
	if( strcmp(name,"dummy") == 0 ){
		return 1;
	}  else return 0; // includes 'none'

}

// maps option availability check function name strings found in XML to their IDs used by the function that binds them
static UINT16 mapAvailNameToId( const CHAR8 * name ){
	
	if( strcmp(name,"dummy") == 0 ){
		return 1;
	}  else return 0; // includes 'none'

}

static void XMLCALL
pocketPopupStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
	pocketPopupParseData * pData = (pocketPopupParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "POCKETPOPUPS") == 0 &&
			pData->curElement == POPUP_PARSE::OUTSIDE_POCKET_LIST)
		{
			pData->curElement = POPUP_PARSE::POCKET_LIST;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "POCKET") == 0 &&
			pData->curElement == POPUP_PARSE::POCKET_LIST)
		{
			pData->curElement = POPUP_PARSE::POCKET;

			pData->maxReadDepth++; //we are not skipping this element
			for (auto* submenu : pData->curPocketSubPopupOption)
				if (submenu) throw std::runtime_error("pocket popup starts with an unfinished submenu");
			pData->curSubPopupLevel = 0; // we're in a new pocket now, so reset the subpopup level
		}
		else if(pData->curElement == POPUP_PARSE::POCKET &&
			   (strcmp(name, "pIndex") == 0 ))
		{
			pData->curElement = POPUP_PARSE::POCKET_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::POCKET &&
			   (strcmp(name, "popup") == 0 ))
		{
			pData->curElement = POPUP_PARSE::POPUP;

			pData->maxReadDepth++; //we are not skipping this element
			delete pData->curPocketPopup;
			pData->curPocketPopup = new popupDef();
		}
		else if(pData->curElement == POPUP_PARSE::POPUP &&		// popup options (not in submenu)
			   (strcmp(name, "option") == 0 ))
		{
			pData->curElement = POPUP_PARSE::OPTION;
			pData->curPocketPopupOptionName[0] = L'\0';
			pData->curPocketPopupOptionCallback = 0;
			pData->curPocketPopupOptionAvail = 0;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::OPTION &&		// popup option attributes (not in submenu)
			   (  strcmp(name, "name") == 0 
			   || strcmp(name, "action") == 0  
			   || strcmp(name, "availCheck") == 0 ))
		{
			pData->curElement = POPUP_PARSE::OPTION_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::POPUP &&		// content generators (not in submenu)
			   (strcmp(name, "generator") == 0 ))
		{
			pData->curElement = POPUP_PARSE::GENERATOR;
			pData->curPocketPopupGeneratorId = 0;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::GENERATOR &&		// content generator attributes (not in submenu)
			   (  strcmp(name, "id") == 0 ) )
		{
			pData->curElement = POPUP_PARSE::GENERATOR_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::POPUP &&		// submenu (first)
			   (strcmp(name, "subMenu") == 0 ))
		{
			pData->curElement = POPUP_PARSE::SUBMENU;

			pData->curSubPopupLevel = 1; // we're still at popup level so this must be the first subpopup
			if (pData->curPocketSubPopupOption[0])
				throw std::runtime_error("pocket popup submenu slot is already occupied");
			pData->curPocketSubPopupOption[ pData->curSubPopupLevel-1 ] = new popupDefSubPopupOption();
			pData->curPocketSubPopupOptionName[0][0] = L'\0';

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::SUBMENU &&	// submenu (deep)
			   (strcmp(name, "subMenu") == 0 ))
		{
			pData->curElement = POPUP_PARSE::SUBMENU;

			if (pData->curSubPopupLevel >= POPUP_MAX_SUB_POPUPS)
				throw std::runtime_error("pocket popup submenu nesting exceeds capacity");
			pData->curSubPopupLevel++;
			if (pData->curPocketSubPopupOption[pData->curSubPopupLevel - 1])
				throw std::runtime_error("nested pocket popup submenu slot is already occupied");
			pData->curPocketSubPopupOption[ pData->curSubPopupLevel-1 ] = new popupDefSubPopupOption();
			pData->curPocketSubPopupOptionName[pData->curSubPopupLevel - 1][0] = L'\0';

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::SUBMENU &&		// submenu attributes
			   (  strcmp(name, "name") == 0 ) )
		{
			pData->curElement = POPUP_PARSE::SUBMENU_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::SUBMENU &&		// popup options (submenu)
			   (strcmp(name, "option") == 0 ))
		{
			pData->curElement = POPUP_PARSE::SUBMENU_OPTION;
			pData->curPocketPopupOptionName[0] = L'\0';
			pData->curPocketPopupOptionCallback = 0;
			pData->curPocketPopupOptionAvail = 0;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::SUBMENU_OPTION &&		// popup option attributes (submenu)
			   (  strcmp(name, "name") == 0 
			   || strcmp(name, "action") == 0  
			   || strcmp(name, "availCheck") == 0 ))
		{
			pData->curElement = POPUP_PARSE::SUBMENU_OPTION_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::SUBMENU &&		// content generators (submenu)
			   (strcmp(name, "generator") == 0 ))
		{
			pData->curElement = POPUP_PARSE::SUBMENU_GENERATOR;
			pData->curPocketPopupGeneratorId = 0;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == POPUP_PARSE::SUBMENU_GENERATOR &&		// content generator attributes (submenu)
			   (  strcmp(name, "id") == 0 ) )
		{
			pData->curElement = POPUP_PARSE::SUBMENU_GENERATOR_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}


		pData->szCharData[0] = '\0';
	}

	pData->currentDepth++;

}

static void XMLCALL
pocketPopupCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
	pocketPopupParseData * pData = (pocketPopupParseData *)userData;

	if( (pData->currentDepth <= pData->maxReadDepth) &&
		(strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH)
	){
		strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
	}
}


static void XMLCALL
pocketPopupEndElementHandle(void *userData, const XML_Char *name)
{
	pocketPopupParseData * pData = (pocketPopupParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) 
	{
		if(strcmp(name, "POCKETPOPUPS") == 0)
		{
			pData->curElement = POPUP_PARSE::OUTSIDE_POCKET_LIST;
		}
		else if(strcmp(name, "POCKET") == 0)
		{
			pData->curElement = POPUP_PARSE::POCKET_LIST;	
			
			// done with the pocket
			// nothing to do because we already saved the popup when closing <popup> tag
		
		}
		else if(strcmp(name, "pIndex") == 0)
		{
			pData->curElement = POPUP_PARSE::POCKET;
			char* end = nullptr;
			errno = 0;
			const long pocketId = std::strtol(pData->szCharData, &end, 10);
			while (end && std::isspace(static_cast<unsigned char>(*end))) ++end;
			if (errno != 0 || end == pData->szCharData || !end || *end != '\0' ||
				pocketId < 0 || pocketId > std::numeric_limits<UINT8>::max())
				throw std::runtime_error("pocket popup index is outside the UINT8 range");
			pData->curPocketId = static_cast<UINT8>(pocketId);
		}
		else if(strcmp(name, "popup") == 0)
		{
			pData->curElement = POPUP_PARSE::POCKET;
			// done with the popup definition
			if (!pData->curPocketPopup)
				throw std::runtime_error("pocket popup closed without a definition");
			pData->parsedPopups[pData->curPocketId] = std::move(*pData->curPocketPopup);
			delete pData->curPocketPopup;
			pData->curPocketPopup = nullptr;
		}
		else if(strcmp(name, "subMenu") == 0)
		{
			if (pData->curSubPopupLevel == 0 ||
				pData->curSubPopupLevel > POPUP_MAX_SUB_POPUPS)
				throw std::runtime_error("pocket popup submenu close is unbalanced");

			// done with the subpopup definition

			// rename the current option, we should've collected a name for it by now
			const auto completedIndex = pData->curSubPopupLevel - 1;
			std::unique_ptr<popupDefSubPopupOption> completed(
				pData->curPocketSubPopupOption[completedIndex]);
			pData->curPocketSubPopupOption[completedIndex] = nullptr;
			if (!completed)
				throw std::runtime_error("pocket popup submenu definition is missing");
			completed->rename(pData->curPocketSubPopupOptionName[completedIndex]);

			if( pData->curSubPopupLevel == 1 ){	// at first submenu level, add the current menu to the base popup
				pData->curElement = POPUP_PARSE::POPUP;

				if (!pData->curPocketPopup ||
					!pData->curPocketPopup->addSubPopup(std::move(completed)))
					throw std::runtime_error("pocket popup submenu has no parent");
				pData->curSubPopupLevel = 0;
			} else {	// deep in submenu tree, add the current submenu to the one above
				pData->curElement = POPUP_PARSE::SUBMENU;

				auto* parent = pData->curPocketSubPopupOption[pData->curSubPopupLevel - 2];
				if (!parent || !parent->getSubDef()->addSubPopup(std::move(completed)))
					throw std::runtime_error("nested pocket popup submenu has no parent");
				pData->curSubPopupLevel--;
			}

		}
		else if( pData->curElement == POPUP_PARSE::OPTION && strcmp(name, "option") == 0)	// option (popup)
		{
			pData->curElement = POPUP_PARSE::POPUP;
			// done with the option
			if (!pData->curPocketPopup)
				throw std::runtime_error("pocket popup option has no parent");
			if (!pData->curPocketPopup->addOption(pData->curPocketPopupOptionName,
				pData->curPocketPopupOptionCallback, pData->curPocketPopupOptionAvail))
				throw std::runtime_error("pocket popup option could not be stored");
		}
		else if( pData->curElement == POPUP_PARSE::SUBMENU_OPTION && strcmp(name, "option") == 0)	// option (sub-popup)
		{
			pData->curElement = POPUP_PARSE::SUBMENU;
			// done with the option
			if (pData->curSubPopupLevel == 0 ||
				!pData->curPocketSubPopupOption[pData->curSubPopupLevel - 1])
				throw std::runtime_error("nested pocket popup option has no parent");
			if (!pData->curPocketSubPopupOption[pData->curSubPopupLevel - 1]
				->getSubDef()->addOption(pData->curPocketPopupOptionName,
					pData->curPocketPopupOptionCallback, pData->curPocketPopupOptionAvail))
				throw std::runtime_error("nested pocket popup option could not be stored");
		}
		else if( pData->curElement == POPUP_PARSE::GENERATOR && strcmp(name, "generator") == 0)	// generator (popup)
		{
			pData->curElement = POPUP_PARSE::POPUP;
			// done with the generator
			if (!pData->curPocketPopup ||
				!pData->curPocketPopup->addGenerator(pData->curPocketPopupGeneratorId))
				throw std::runtime_error("pocket popup generator is invalid or has no parent");
		}
		else if( pData->curElement == POPUP_PARSE::SUBMENU_GENERATOR && strcmp(name, "generator") == 0)	// generator (sub-popup)
		{
			pData->curElement = POPUP_PARSE::SUBMENU;
			// done with the generator
			if (pData->curSubPopupLevel == 0 ||
				!pData->curPocketSubPopupOption[pData->curSubPopupLevel - 1] ||
				!pData->curPocketSubPopupOption[pData->curSubPopupLevel - 1]
					->getSubDef()->addGenerator(pData->curPocketPopupGeneratorId))
				throw std::runtime_error("nested pocket popup generator is invalid or has no parent");
		}
		else if(strcmp(name, "name") == 0)
		{
			switch( pData->curElement ){
			case POPUP_PARSE::OPTION_PROPERTY:
				pData->curElement = POPUP_PARSE::OPTION;

				MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, pData->curPocketPopupOptionName, sizeof(pData->curPocketPopupOptionName)/sizeof(pData->curPocketPopupOptionName[0]) );
				pData->curPocketPopupOptionName[sizeof(pData->curPocketPopupOptionName)/sizeof(pData->curPocketPopupOptionName[0]) - 1] = '\0';

				break;

			case POPUP_PARSE::SUBMENU_OPTION_PROPERTY:
				pData->curElement = POPUP_PARSE::SUBMENU_OPTION;

				MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, pData->curPocketPopupOptionName, sizeof(pData->curPocketPopupOptionName)/sizeof(pData->curPocketPopupOptionName[0]) );
				pData->curPocketPopupOptionName[sizeof(pData->curPocketPopupOptionName)/sizeof(pData->curPocketPopupOptionName[0]) - 1] = '\0';

				break;

			case POPUP_PARSE::SUBMENU_PROPERTY:
				if (pData->curSubPopupLevel == 0 ||
					pData->curSubPopupLevel > POPUP_MAX_SUB_POPUPS)
					throw std::runtime_error("pocket popup submenu name has no parent");
				pData->curElement = POPUP_PARSE::SUBMENU;

				MultiByteToWideChar( CP_UTF8, 0, pData->szCharData, -1, pData->curPocketSubPopupOptionName[pData->curSubPopupLevel-1], sizeof(pData->curPocketSubPopupOptionName[pData->curSubPopupLevel-1])/sizeof(pData->curPocketSubPopupOptionName[pData->curSubPopupLevel-1][0]) );
				pData->curPocketSubPopupOptionName[pData->curSubPopupLevel-1][sizeof(pData->curPocketSubPopupOptionName[pData->curSubPopupLevel-1])/sizeof(pData->curPocketSubPopupOptionName[pData->curSubPopupLevel-1][0]) - 1] = '\0';

				break;

			default:
				break;

			}
		}
		else if(strcmp(name, "action") == 0)
		{
			switch( pData->curElement ){
				case POPUP_PARSE::OPTION_PROPERTY:
					pData->curElement = POPUP_PARSE::OPTION;
					break;

				case POPUP_PARSE::SUBMENU_OPTION_PROPERTY:
					pData->curElement = POPUP_PARSE::SUBMENU_OPTION;
					break;
				default:
					break;
			}

			pData->curPocketPopupOptionCallback	= mapCallbackNameToId(pData->szCharData);
		}
		else if(strcmp(name, "availCheck") == 0)
		{
			switch( pData->curElement ){
				case POPUP_PARSE::OPTION_PROPERTY:
					pData->curElement = POPUP_PARSE::OPTION;
					break;

				case POPUP_PARSE::SUBMENU_OPTION_PROPERTY:
					pData->curElement = POPUP_PARSE::SUBMENU_OPTION;
					break;
				default:
					break;
			}

			pData->curPocketPopupOptionAvail	= mapAvailNameToId(pData->szCharData);
		}
		else if(strcmp(name, "id") == 0)
		{
			switch( pData->curElement ){
				case POPUP_PARSE::GENERATOR_PROPERTY:
					pData->curElement = POPUP_PARSE::GENERATOR;
					break;

				case POPUP_PARSE::SUBMENU_GENERATOR_PROPERTY:
					pData->curElement = POPUP_PARSE::SUBMENU_GENERATOR;
					break;
				default:
					break;
			}

			pData->curPocketPopupGeneratorId	= mapGeneratorNameToId(pData->szCharData);
		}

		pData->maxReadDepth--;
	}
	pData->currentDepth--;
}


BOOLEAN ReadInLBEPocketPopups(STR fileName)
{
	pocketPopupParseData pData{};

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading pocketPopups.xml" );

	const LegacyXmlCallbacks callbacks{
		&pData, pocketPopupStartElementHandle, pocketPopupEndElementHandle,
		pocketPopupCharacterDataHandle};
	const LegacyXmlResult result =
		ParseLegacyXmlFile(fileName, callbacks);
	delete pData.curPocketPopup;
	pData.curPocketPopup = nullptr;
	for (auto*& submenu : pData.curPocketSubPopupOption)
	{
		delete submenu;
		submenu = nullptr;
	}
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
	for (auto& [pocketId, definition] : pData.parsedPopups)
		LBEPocketPopup[pocketId] = std::move(definition);

	/*
	// dummy popup

	 popupDef* popup = new popupDef();
	 popup->addOption(std::wstring(L"Option one"),NULL,NULL);
	 popup->addOption(std::wstring(L"Option two"),NULL,NULL);
	 popup->addOption(std::wstring(L"Option three"),NULL,NULL);

	LBEPocketPopup[5] = *popup;
	*/

	return( TRUE );
}
