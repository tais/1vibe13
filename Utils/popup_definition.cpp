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

	popupDef::popupDef(const popupDef& other)
	{
		content.reserve(other.content.size());
		for (const auto& item : other.content) content.push_back(item->clone());
	}

	popupDef& popupDef::operator=(const popupDef& other)
	{
		if (this == &other) return *this;
		popupDef replacement(other);
		content.swap(replacement.content);
		return *this;
	}

	BOOLEAN popupDef::applyToBox(POPUP* popup) const
	{
		if (!popup) return FALSE;
		for (const auto& item : content)
		{
			if (item->addToBox(popup) != TRUE) return FALSE;
		}

		return TRUE;
	}

	BOOLEAN popupDef::addOption(const std::wstring& name, UINT16 callbackId, UINT16 availId){
		// TODO: check for vaid callbacl/avail ID
		content.push_back(std::make_unique<popupDefOption>(name, callbackId, availId));

		return TRUE;
	}

	popupDef* popupDef::addSubPopup(const std::wstring& name){
		auto sub = std::make_unique<popupDefSubPopupOption>(name);
		popupDef* definition = sub->getSubDef();
		content.push_back(std::move(sub));
		return definition;
	}

	BOOLEAN popupDef::addSubPopup(std::unique_ptr<popupDefSubPopupOption> sub){
		// TODO: add check for max options
		if (!sub) return FALSE;
		content.push_back(std::move(sub));

		return TRUE;
	};

	BOOLEAN popupDef::addGenerator(UINT16 id){
		if (id < popupGenerators::dummy || id > popupGenerators::addKits)
			return FALSE;

		content.push_back(std::make_unique<popupDefContentGenerator>(id));

		return TRUE;
	}


//////////////////////////////////////
//	popupDefContent
//////////////////////////////////////

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

	~popupDefOption::popupDefOption(){}
	*/
	BOOLEAN popupDefOption::addToBox(POPUP* popup) const {
		if (!popup) return FALSE;

		std::unique_ptr<POPUP_OPTION> opt(new POPUP_OPTION());

		opt->setName(this->name);
		if (	!setPopupDefCallback(opt.get(), this->callbackId)
			||	!setPopupDefAvail(opt.get(), this->availId) )
		{
			return false;
		}

		if (!popup->addOption(*opt)) return FALSE;
		opt.release();
		return TRUE;
	}

	std::unique_ptr<popupDefContent> popupDefOption::clone() const
	{
		return std::make_unique<popupDefOption>(*this);
	}


//////////////////////////////////////
//	popupDefSubPopupOption
//////////////////////////////////////
	/* defined in header file
	popupDefSubPopupOption::popupDefSubPopupOption(){}

	popupDefSubPopupOption::~popupDefSubPopupOption(){}
	*/
	BOOLEAN popupDefSubPopupOption::addToBox(POPUP* popup) const {
		if (!popup) return FALSE;

		std::unique_ptr<POPUP_SUB_POPUP_OPTION> sub(
			new POPUP_SUB_POPUP_OPTION(this->name));

		if(!content.applyToBox(sub->subPopup)){
			return false;
		}

		if (!popup->addSubMenuOption(sub.get())) return FALSE;
		sub.release();
		return TRUE;
	}

	std::unique_ptr<popupDefContent> popupDefSubPopupOption::clone() const
	{
		return std::make_unique<popupDefSubPopupOption>(*this);
	}
	

//////////////////////////////////////
//	popupDefContentGenerator helpers
//////////////////////////////////////

	/*
	void addArmorToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addLBEToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addWeaponsToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addWeaponGroupsToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addGrenadesToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addBombsToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addFaceGearToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addAmmoToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );

	void addRifleGrenadesToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addRocketAmmoToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	void addDrugsToPocketPopup( TacticalActor *pSoldier, INT16 sPocket, POPUP* popup );
	*/

	static BOOLEAN applyPopupContentGenerator( POPUP * popup, UINT16 generatorId ){
		if (!popup) return FALSE;
		if (generatorId == popupGenerators::dummy)
			return popup->addOption(L"Dummy generator", NULL) != NULL;
		if (bSelectedInfoChar < 0 ||
			bSelectedInfoChar >= CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS ||
			!gCharactersList[bSelectedInfoChar].fValid) return FALSE;
		TacticalActor* pSoldier = nullptr;
		if (!GetSoldier(&pSoldier, gCharactersList[bSelectedInfoChar].usSolID) ||
			!pSoldier) return FALSE;

		switch(generatorId){
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

	BOOLEAN popupDefContentGenerator::addToBox(POPUP* popup) const {

		return applyPopupContentGenerator( popup, this->generatorId );

	}

	std::unique_ptr<popupDefContent> popupDefContentGenerator::clone() const
	{
		return std::make_unique<popupDefContentGenerator>(*this);
	}
	
