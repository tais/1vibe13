#ifndef POPUP_DEFINITION
	#define POPUP_DEFINITION

#include "sgp.h"	
#include "popup_class.h"
#include "popup_callback.h"

namespace popupGenerators{

	enum{
	dummy = 1,
	addArmor,
	addLBE,
	addWeapons,
	addWeaponGroups,
	addGrenades,
	addBombs,
	addFaceGear,
	addAmmo,
	addRifleGrenades,
	addRocketAmmo,
	addMisc,
	addKits
	};

};

class popupDef;
class popupDefContent;

class popupDefOption;
class popupDefSubPopupOption;
class popupDefContentGenerator;

class popupDef{
public:
	popupDef();
	~popupDef();

	BOOLEAN applyToBox(POPUP* popup);

	BOOLEAN addOption(std::wstring* name, UINT16 callbackId, UINT16 availId);

	popupDef * addSubPopup(std::wstring* name);
	BOOLEAN addSubPopup(popupDefSubPopupOption* sub);

	BOOLEAN addGenerator(UINT16 id);
protected:
	std::vector<popupDefContent*> content;
};

class popupDefContent{
public:
	popupDefContent();
	// NB: virtual. popupDef::~popupDef walks content[] and does
	// `delete <popupDefContent*>` -- prior to this, the destructor
	// was non-virtual so the derived destructors (popupDefOption,
	// popupDefSubPopupOption) never ran and their `name` /
	// `content` members leaked. macOS's malloc guard caught the
	// resulting mismatch and SIGTRAP'd at process exit.
	virtual ~popupDefContent();

	virtual BOOLEAN addToBox(POPUP * popup) = 0;

};

class popupDefOption : public popupDefContent{
public:
	popupDefOption() : name( new std::wstring(L"Unnamed Option") ), callbackId(0), availId(0){};
	popupDefOption( std::wstring* name, UINT16 callbackId, UINT16 availId ) : name( name ), callbackId(callbackId), availId(availId){};

	~popupDefOption(){ delete this->name; };

	BOOLEAN addToBox(POPUP * popup);

protected:
	std::wstring* name;
	UINT16 callbackId;
	UINT16 availId;

};

class popupDefSubPopupOption : public popupDefContent{
public:
	popupDefSubPopupOption() : name( new std::wstring(L"Unnamed Submenu") ){ this->content = new popupDef(); };
	popupDefSubPopupOption( std::wstring* name ) : name( name ){ this->content = new popupDef(); };

	~popupDefSubPopupOption(){ delete this->name; delete this->content; };

	BOOLEAN addToBox(POPUP * popup);
	void rename( std::wstring* name ){
		delete this->name;	// lets just hope nothing else was using this string. TODO: use smart pointer
		this->name = name;
	};
	popupDef * getSubDef(){ return this->content; };

protected:
	std::wstring* name;
	popupDef * content;
};

class popupDefContentGenerator: public popupDefContent{
public:
	popupDefContentGenerator() : generatorId(0){};
	popupDefContentGenerator( UINT16 generatorId ) : generatorId( generatorId ){};

	~popupDefContentGenerator();

	BOOLEAN addToBox(POPUP * popup);

protected:	
	UINT16 generatorId;
};

#endif
