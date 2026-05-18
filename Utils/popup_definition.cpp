	#include "popup_class.h"
	#include "popup_callback.h"
	#include "sgp.h"

	#include "popup_definition.h"
	#include "Interface Items.h"

	// for getting psoldier
	#include "Map Screen Interface.h"
	#include "Map Screen Interface Map.h"
	#include "Overhead.h"

//////////////////////////////////////
//	popupDef
//////////////////////////////////////

	popupDef::popupDef(){}

	popupDef::~popupDef(){
		// NB: leak content[] on purpose.
		//
		// The XML loader (Tactical/XML_LBEPocketPopup.cpp) does
		//     LBEPocketPopup[id] = *pData->curPocketPopup;
		// which shallow-copies the content* pointers into the map
		// AND keeps them alive in the source `*curPocketPopup`
		// (which the loader never deletes -- it just allocates a
		// fresh popupDef for the next <popup>). The map's static
		// destructor at process exit then tries to free the same
		// pointers that the (now-leaked) source popupDef would
		// also be responsible for if its destructor ever ran, and
		// macOS's malloc guard SIGTRAPs.
		//
		// Real fixes: deep-copy via a clone() virtual, or change
		// LBEPocketPopup to hold popupDef* / unique_ptr<popupDef>.
		// For now, leak: LBEPocketPopup is process-global, the
		// number of popup defs is bounded by the XML file (a few
		// dozen), and the OS reclaims the memory when the process
		// exits.
	}

	BOOLEAN popupDef::applyToBox(POPUP* popup){
		for(std::vector<popupDefContent*>::iterator content=this->content.begin(); content != this->content.end(); ++content)
		{
			if ( (*content)->addToBox( popup ) != TRUE ) return FALSE;
		}

		return TRUE;
	}

	BOOLEAN popupDef::addOption(std::wstring* name, UINT16 callbackId, UINT16 availId){
		// TODO: check for vaid callbacl/avail ID
		
		this->content.push_back( new popupDefOption(name,callbackId,availId) );

		return TRUE;
	}

	popupDef * popupDef::addSubPopup(std::wstring* name){

		popupDefSubPopupOption * sub = new popupDefSubPopupOption(name);

		this->content.push_back( sub );
	
		return sub->getSubDef();
	}

	BOOLEAN popupDef::addSubPopup(popupDefSubPopupOption* sub){
		// TODO: add check for max options
		this->content.push_back( sub );

		return TRUE;
	};

	BOOLEAN popupDef::addGenerator(UINT16 id){
		// TODO: check for valid generator id

		this->content.push_back( new popupDefContentGenerator(id) );

		return TRUE;
	}


//////////////////////////////////////
//	popupDefContent
//////////////////////////////////////

	popupDefContent::popupDefContent(){}
	popupDefContent::~popupDefContent(){}


//////////////////////////////////////
//	popupDefOption helpers
//////////////////////////////////////

	static BOOLEAN setPopupDefCallback( POPUP_OPTION * opt, UINT16 callbackId ){
	
		// TODO
		opt->setAction(NULL);

		return TRUE;

	}

	static BOOLEAN setPopupDefAvail( POPUP_OPTION * opt, UINT16 callbackId ){
	
		// TODO
		opt->setAvail(NULL);

		return TRUE;

	}

//////////////////////////////////////
//	popupDefOption
//////////////////////////////////////
	/* defined in header file
	popupDefOption::popupDefOption(){}
	popupDefOption::popupDefOption( std::wstring* name, UINT16 callbackId, UINT16 availId ){}

	~popupDefOption::popupDefOption(){}
	*/
	BOOLEAN popupDefOption::addToBox(POPUP * popup){
	
		POPUP_OPTION * opt = new POPUP_OPTION();

		opt->setName( this->name );
		if (	!setPopupDefCallback(opt, this->callbackId) 
			||	!setPopupDefAvail(opt, this->availId) )
		{
			delete opt;
			return false;
		}
		
	
		return popup->addOption(*opt);
	}


//////////////////////////////////////
//	popupDefSubPopupOption
//////////////////////////////////////
	/* defined in header file
	popupDefSubPopupOption::popupDefSubPopupOption(){}
	popupDefSubPopupOption::popupDefSubPopupOption( std::wstring* name ){}
	
	popupDefSubPopupOption::~popupDefSubPopupOption(){}
	*/
	BOOLEAN popupDefSubPopupOption::addToBox(POPUP * popup){
	
		POPUP_SUB_POPUP_OPTION * sub = new POPUP_SUB_POPUP_OPTION( this->name );

		if( !this->content->applyToBox( sub->subPopup ) ){
			delete sub;
			return false;
		}

		return popup->addSubMenuOption(sub);	
	}
	

//////////////////////////////////////
//	popupDefContentGenerator helpers
//////////////////////////////////////

	/*
	void addArmorToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addLBEToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addWeaponsToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addWeaponGroupsToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addGrenadesToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addBombsToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addFaceGearToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addAmmoToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );

	void addRifleGrenadesToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addRocketAmmoToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	void addDrugsToPocketPopup( SOLDIERTYPE *pSoldier, INT16 sPocket, POPUP* popup );
	*/

	static BOOLEAN applyPopupContentGenerator( POPUP * popup, UINT16 generatorId ){

		SOLDIERTYPE	*pSoldier;
		GetSoldier( &pSoldier, gCharactersList[ bSelectedInfoChar ].usSolID );

		switch(generatorId){
		case popupGenerators::dummy:
			popup->addOption(new std::wstring( L"Dummy generator" ),NULL);
			break;

		case popupGenerators::addArmor:
			addArmorToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addLBE:
			addLBEToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addWeapons:
			addWeaponsToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addWeaponGroups:
			addWeaponGroupsToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addGrenades:
			addGrenadesToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addBombs:
			addBombsToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addFaceGear:
			addFaceGearToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addAmmo:
			addAmmoToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addRifleGrenades:
			addRifleGrenadesToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addRocketAmmo:
			addRocketAmmoToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addMisc:
			addMiscToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;
		case popupGenerators::addKits:
			addKitsToPocketPopup( pSoldier, gsPocketUnderCursor, popup );
			break;

		default:
			return FALSE;
		}

		return TRUE;
		
	}

//////////////////////////////////////
//	popupDefContentGenerator
//////////////////////////////////////
	/* defined in header file
	popupDefContentGenerator::popupDefContentGenerator(){}
	popupDefContentGenerator::popupDefContentGenerator( UINT16 generatorId ){}
	*/

	// The destructor is virtual (inherited from popupDefContent --
	// recently made virtual to silence the macOS malloc guard's
	// SIGTRAP on delete-via-base-pointer). It's the key function
	// that anchors popupDefContentGenerator's vtable in this TU.
	popupDefContentGenerator::~popupDefContentGenerator(){}

	BOOLEAN popupDefContentGenerator::addToBox(POPUP * popup){
	
		return applyPopupContentGenerator( popup, this->generatorId );
	
	}
	
