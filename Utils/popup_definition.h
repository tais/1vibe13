#ifndef POPUP_DEFINITION
	#define POPUP_DEFINITION

#include "sgp.h"
#include "popup_class.h"
#include "popup_callback.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

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
	popupDef() = default;
	popupDef(const popupDef& other);
	popupDef& operator=(const popupDef& other);
	popupDef(popupDef&&) noexcept = default;
	popupDef& operator=(popupDef&&) noexcept = default;
	~popupDef() = default;

	BOOLEAN applyToBox(POPUP* popup) const;

	BOOLEAN addOption(const std::wstring& name, UINT16 callbackId, UINT16 availId);

	popupDef* addSubPopup(const std::wstring& name);
	BOOLEAN addSubPopup(std::unique_ptr<popupDefSubPopupOption> sub);

	BOOLEAN addGenerator(UINT16 id);
	std::size_t contentCount() const noexcept { return content.size(); }
protected:
	std::vector<std::unique_ptr<popupDefContent>> content;
};

class popupDefContent{
public:
	popupDefContent() = default;
	virtual ~popupDefContent() = default;

	virtual BOOLEAN addToBox(POPUP* popup) const = 0;
	virtual std::unique_ptr<popupDefContent> clone() const = 0;

};

class popupDefOption : public popupDefContent{
public:
	popupDefOption() = default;
	popupDefOption(const std::wstring& name, UINT16 callbackId, UINT16 availId)
		: name(name), callbackId(callbackId), availId(availId) {}

	BOOLEAN addToBox(POPUP* popup) const override;
	std::unique_ptr<popupDefContent> clone() const override;

protected:
	std::wstring name = L"Unnamed Option";
	UINT16 callbackId = 0;
	UINT16 availId = 0;

};

class popupDefSubPopupOption : public popupDefContent{
public:
	popupDefSubPopupOption() = default;
	explicit popupDefSubPopupOption(const std::wstring& name) : name(name) {}

	BOOLEAN addToBox(POPUP* popup) const override;
	std::unique_ptr<popupDefContent> clone() const override;
	void rename(const std::wstring& newName) { name = newName; }
	popupDef* getSubDef() { return &content; }
	const popupDef* getSubDef() const { return &content; }

protected:
	std::wstring name = L"Unnamed Submenu";
	popupDef content;
};

class popupDefContentGenerator: public popupDefContent{
public:
	popupDefContentGenerator() = default;
	explicit popupDefContentGenerator(UINT16 generatorId) : generatorId(generatorId) {}

	BOOLEAN addToBox(POPUP* popup) const override;
	std::unique_ptr<popupDefContent> clone() const override;

protected:
	UINT16 generatorId = 0;
};

#endif
