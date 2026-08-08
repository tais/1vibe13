#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>
#include "ItemDataStagingModel.h"
#include "ItemXmlWriter.h"
#include "XMLWriter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

	#include "sgp.h"
	#include "Overhead Types.h"
	#include "TacticalActor.h"
	#include "Drugs And Alcohol.h"
	#include "Overhead.h"
	#include "Event Pump.h"
	#include "Weapons.h"
	#include "Animation Control.h"
	#include "Handle UI.h"
	#include "Isometric Utils.h"
	#include "math.h"
	#include "ai.h"
	#include "LOS.h"
	#include "renderworld.h"
	#include "Interface.h"
	#include "message.h"
	#include "Campaign.h"
	#include "Items.h"
	#include "Text.h"
	#include "Soldier Profile.h"
	#include "Dialogue Control.h"
	#include "SkillCheck.h"
	#include "Quests.h"
	#include "physics.h"
	#include "random.h"
	#include "Vehicles.h"
	#include "Bullets.h"
	#include "Morale.h"
	#include "SkillCheck.h"
	#include "GameSettings.h"
	#include "Points.h"
	#include "SaveLoadMap.h"
	#include "Debug Control.h"
	#include "expat.h"
	#include "XML.h"
	#include "Utilities.h"
	#include "Store Inventory.h"

// Flugente: in order not to loop over MAXITEMS items if we only have a few thousand, remember the actual number of items in the xml
UINT32 gMAXITEMS_READ = 0;

using ItemText = std::basic_string<CHAR16>;

struct itemLocalizedTextPatch
{
	std::optional<ItemText> itemName;
	std::optional<ItemText> longItemName;
	std::optional<ItemText> itemDescription;
	std::optional<ItemText> storeName;
	std::optional<ItemText> storeDescription;
};

using BaseItemLoad =
	ItemDataStagingModel::RequiredBaseLoadTransaction<INVTYPE>;
using LocalizedItemLoad =
	ItemDataStagingModel::OptionalLocalizedLoadTransaction<
		itemLocalizedTextPatch>;

constexpr std::size_t ItemDescriptionCapacity =
	std::extent_v<decltype(INVTYPE::szItemDesc)>;
constexpr std::size_t ItemUtf8BytesPerTextUnit =
	sizeof(CHAR16) == 2 ? 3 : 4;
constexpr std::size_t ItemCharacterDataCapacity =
	(ItemDescriptionCapacity - 1) * ItemUtf8BytesPerTextUnit + 1;
using ItemCharacterData =
	ItemDataStagingModel::CharacterAccumulator<ItemCharacterDataCapacity>;
static_assert(ItemDescriptionCapacity > 1);
static_assert(std::extent_v<decltype(INVTYPE::szBRDesc)> <=
	ItemDescriptionCapacity);
static_assert(std::extent_v<decltype(INVTYPE::szItemName)> <=
	ItemDescriptionCapacity);
static_assert(std::extent_v<decltype(INVTYPE::szLongItemName)> <=
	ItemDescriptionCapacity);
static_assert(std::extent_v<decltype(INVTYPE::szBRName)> <=
	ItemDescriptionCapacity);

struct itemParseData
{
	PARSE_STAGE curElement = ELEMENT_NONE;

	ItemCharacterData characterData;
	INVTYPE curItem{};
	ItemDataStagingModel::AuxiliaryPatch currentAuxiliary;
	itemLocalizedTextPatch localizedPatch;
	BaseItemLoad* baseLoad = nullptr;
	LocalizedItemLoad* localizedLoad = nullptr;
	INT8 curStance = 0;
	bool localizedTextOnly = false;
	bool hasIndex = false;
	bool failed = false;
	bool sawItemList = false;
	bool completedItemList = false;

	UINT32 currentDepth = 0;
	UINT32 maxReadDepth = 0;
};

static void FailItemParse(
	itemParseData* pData, ItemDataStagingModel::Failure failure) noexcept
{
	pData->failed = true;
	if (pData->baseLoad) pData->baseLoad->fail(failure);
	if (pData->localizedLoad) pData->localizedLoad->fail(failure);
}

template <typename Integer>
static bool ParseItemInteger(
	itemParseData* pData, Integer& destination,
	ItemDataStagingModel::IntegerSyntax syntax =
		ItemDataStagingModel::IntegerSyntax::Decimal)
{
	if (ItemDataStagingModel::TryParseInteger(
			pData->characterData.view(), destination, syntax))
	{
		return true;
	}
	FailItemParse(pData, ItemDataStagingModel::Failure::MalformedInput);
	return false;
}

template <typename Integer>
static Integer ParseItemIntegerValue(
	itemParseData* pData,
	ItemDataStagingModel::IntegerSyntax syntax =
		ItemDataStagingModel::IntegerSyntax::Decimal)
{
	Integer value{};
	ParseItemInteger(pData, value, syntax);
	return value;
}

static bool ParseItemBooleanValue(itemParseData* pData)
{
	bool value = false;
	if (!ItemDataStagingModel::TryParseBoolean(
			pData->characterData.view(), value))
	{
		FailItemParse(pData, ItemDataStagingModel::Failure::MalformedInput);
	}
	return value;
}

static FLOAT ParseItemFloatValue(itemParseData* pData)
{
	FLOAT value = 0.0f;
	if (!ItemDataStagingModel::TryParseFiniteFloat(
			pData->characterData.view(), value))
	{
		FailItemParse(pData, ItemDataStagingModel::Failure::MalformedInput);
	}
	return value;
}

template <typename Integer>
static Integer ParseItemClampedIntegerValue(itemParseData* pData,
	std::int64_t minimum, std::int64_t maximum)
{
	Integer value{};
	if (!ItemDataStagingModel::TryParseClampedInteger(
			pData->characterData.view(), value, minimum, maximum))
	{
		FailItemParse(pData, ItemDataStagingModel::Failure::MalformedInput);
	}
	return value;
}

static UINT16 ParseLegacyItemPrice(itemParseData* pData)
{
	UINT32 widePrice = 0;
	if (!ParseItemInteger(pData, widePrice)) return 0;
	// The shipped 1.13 table contains 70000. The old Windows parser converted
	// that complete nonnegative token to 32 bits and then deliberately exposed
	// the established 16-bit modulo value (4464). Keep only this schema-specific
	// compatibility rule; every other integer destination rejects width loss.
	return static_cast<UINT16>(widePrice);
}

template <std::size_t Capacity>
static bool ParseItemText(itemParseData* pData,
	CHAR16 (&destination)[Capacity], std::optional<ItemText>* patch = nullptr)
{
	if (!pData->characterData.valid() ||
		!ItemDataStagingModel::TryCopyUtf8(
			pData->characterData.view(), destination))
	{
		FailItemParse(pData, ItemDataStagingModel::Failure::MalformedInput);
		return false;
	}
	if (patch)
	{
		try
		{
			*patch = ItemText(destination);
		}
		catch (...)
		{
			// Localized strings allocate while Expat is inside a C callback.
			// Convert allocation/copy failure into a rejected transaction so
			// no C++ exception can unwind through the C parser frames.
			FailItemParse(pData, ItemDataStagingModel::Failure::StagingFailed);
			return false;
		}
	}
	return true;
}

template <std::size_t Capacity>
static bool LocalizedTextFits(
	const std::optional<ItemText>& text, const CHAR16 (&)[Capacity]) noexcept
{
	return !text || text->size() < Capacity;
}

static bool ValidateLocalizedItemText(std::size_t index,
	const itemLocalizedTextPatch& patch) noexcept
{
	if (index >= MAXITEMS) return false;
	const INVTYPE& item = Item[index];
	return LocalizedTextFits(patch.itemName, item.szItemName) &&
		LocalizedTextFits(patch.longItemName, item.szLongItemName) &&
		LocalizedTextFits(patch.itemDescription, item.szItemDesc) &&
		LocalizedTextFits(patch.storeName, item.szBRName) &&
		LocalizedTextFits(patch.storeDescription, item.szBRDesc);
}

template <std::size_t Capacity>
static void PublishLocalizedText(const std::optional<ItemText>& text,
	CHAR16 (&destination)[Capacity]) noexcept
{
	if (!text) return;
	for (std::size_t index = 0; index < text->size(); ++index)
		destination[index] = (*text)[index];
	destination[text->size()] = L'\0';
}

static void PublishLocalizedItemText(std::size_t index,
	const itemLocalizedTextPatch& patch) noexcept
{
	INVTYPE& item = Item[index];
	PublishLocalizedText(patch.itemName, item.szItemName);
	PublishLocalizedText(patch.longItemName, item.szLongItemName);
	PublishLocalizedText(patch.itemDescription, item.szItemDesc);
	PublishLocalizedText(patch.storeName, item.szBRName);
	PublishLocalizedText(patch.storeDescription, item.szBRDesc);
}

// HEADROCK HAM 4: Inherits data between stance-based modifiers
void InheritStanceModifiers( itemParseData *pData );

static void
itemStartElementHandleImpl(void *userData, const XML_Char *name,
	const XML_Char **atts)
{
	itemParseData * pData = (itemParseData *)userData;

	//DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("itemStartElementHandle: at element (%d), name = %s, depth = %d, maxdepth = %d, jar? = %d",pData->curElement,name,pData->currentDepth,pData->maxReadDepth,strcmp(name, "Jar") ) );
	if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
	{
		if(strcmp(name, "ITEMLIST") == 0 && pData->curElement == ELEMENT_NONE)
		{
			pData->curElement = ELEMENT_LIST;
			pData->sawItemList = true;

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(strcmp(name, "ITEM") == 0 && pData->curElement == ELEMENT_LIST)
		{
			pData->curElement = ELEMENT;
			pData->curItem = INVTYPE{};
			pData->currentAuxiliary = {};
			pData->localizedPatch = {};
			pData->hasIndex = false;

			// Flugente: default value
			pData->curItem.usPortionSize = 100;

			// HEADROCK HAM 4: With the new stance-based variables, it is necessary to set vars to have an impossible
			// value. That way when they are later recorded in the item structs, a parent->child inheritence can occur
			// for children that do not have data put into them from XML.
			// -10000 has been selected to pose as "no value". Modders should never even reduce the value of any of
			// these tags below -100 anyway, and although it's not the best solution that's the only one I came up with.

			for (INT8 X = 0; X < 3; ++X)
			{
				pData->curItem.flatbasemodifier[X] = -10000;
				pData->curItem.percentbasemodifier[X] = -10000;
				pData->curItem.flataimmodifier[X] = -10000;
				pData->curItem.percentaimmodifier[X] = -10000;
				pData->curItem.percentcapmodifier[X] = -10000;
				pData->curItem.percenthandlingmodifier[X] = -10000;
				pData->curItem.targettrackingmodifier[X] = -10000;
				pData->curItem.percentdropcompensationmodifier[X] = -10000;
				pData->curItem.maxcounterforcemodifier[X] = -10000;
				pData->curItem.counterforceaccuracymodifier[X] = -10000;
				pData->curItem.aimlevelsmodifier[X] = -10000;
			}

			pData->maxReadDepth++; //we are not skipping this element
		}
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "uiIndex") == 0 ||
				strcmp(name, "szItemName") == 0 ||
				strcmp(name, "szLongItemName") == 0 ||
				strcmp(name, "szItemDesc") == 0 ||
				strcmp(name, "szBRName") == 0 ||
				strcmp(name, "szBRDesc") == 0 ||
				strcmp(name, "usItemClass") == 0 ||
				strcmp(name, "nasAttachmentClass") == 0 ||
				strcmp(name, "nasLayoutClass") == 0 ||
				strcmp(name, "AvailableAttachmentPoint") == 0 ||
				strcmp(name, "AttachmentPoint") == 0 ||
				strcmp(name, "AttachToPointAPCost") == 0 ||
				strcmp(name, "ubClassIndex") == 0 ||
				strcmp(name, "ubCursor") == 0 ||
				strcmp(name, "bSoundType") == 0 ||
				strcmp(name, "ubGraphicType") == 0 ||
				strcmp(name, "ubGraphicNum") == 0 ||
				strcmp(name, "ubWeight") == 0 ||
				strcmp(name, "ubPerPocket") == 0 ||
				strcmp(name, "ItemSize") == 0 ||
				strcmp(name, "usPrice") == 0 ||
				strcmp(name, "ubCoolness") == 0 ||
				strcmp(name, "bReliability") == 0 ||
				strcmp(name, "bRepairEase") == 0 ||
				strcmp(name, "Damageable") == 0 ||
				strcmp(name, "Repairable") == 0 ||
				strcmp(name, "WaterDamages") == 0 ||
				strcmp(name, "Metal") == 0 ||
				strcmp(name, "Sinks") == 0 ||
				strcmp(name, "ShowStatus") == 0 ||
				strcmp(name, "HiddenAddon") == 0 ||
				strcmp(name, "TwoHanded") == 0 ||
				strcmp(name, "NotBuyable") == 0 ||
				strcmp(name, "Attachment") == 0 ||
				strcmp(name, "HiddenAttachment") == 0 ||
				strcmp(name, "BigGunList") == 0 ||
				strcmp(name, "NotInEditor") == 0 ||
				strcmp(name, "DefaultUndroppable") == 0 ||
				strcmp(name, "Unaerodynamic") == 0 ||
				strcmp(name, "Electronic") == 0 ||
				strcmp(name, "Inseparable") == 0 ||
				strcmp(name, "BR_NewInventory") == 0 ||
				strcmp(name, "BR_UsedInventory") == 0 ||
				strcmp(name, "BR_ROF") == 0 ||
				strcmp(name, "PercentNoiseReduction") == 0 ||
				strcmp(name, "Bipod") == 0 ||
				strcmp(name, "BestLaserRange") == 0 ||
				strcmp(name, "ToHitBonus") == 0 ||
				strcmp(name, "RangeBonus") == 0 ||
				strcmp(name, "PercentRangeBonus") == 0 ||
				strcmp(name, "AimBonus") == 0 ||
				strcmp(name, "MinRangeForAimBonus") == 0 ||
				strcmp(name, "PercentAPReduction") == 0 ||
				strcmp(name, "PercentStatusDrainReduction") == 0 ||
				strcmp(name, "GrenadeLauncher") == 0 ||
				strcmp(name, "Duckbill") == 0 ||
				strcmp(name, "Detonator") == 0 ||
				strcmp(name, "RemoteDetonator") == 0 ||
				strcmp(name, "HideMuzzleFlash") == 0 ||
				strcmp(name, "Alcohol") == 0 ||
				strcmp(name, "GasMask") == 0 ||
				strcmp(name, "Hardware") == 0 ||
				strcmp(name, "Medical") == 0 ||
				strcmp(name, "DamageBonus") == 0 ||
				strcmp(name, "MeleeDamageBonus") == 0 ||
				strcmp(name, "Mortar") == 0 ||
				strcmp(name, "RocketLauncher") == 0 ||
				strcmp(name, "SingleShotRocketLauncher") == 0 ||
				strcmp(name, "DiscardedLauncherItem") == 0 ||
				strcmp(name, "BrassKnuckles") == 0 ||
				strcmp(name, "BloodiedItem") == 0 ||
				strcmp(name, "Crowbar") == 0 ||
				strcmp(name, "GLGrenade") == 0 ||
				strcmp(name, "FlakJacket") == 0 ||
				strcmp(name, "HearingRangeBonus") == 0 ||
				strcmp(name, "VisionRangeBonus") == 0 ||
				strcmp(name, "NightVisionRangeBonus") == 0 ||
				strcmp(name, "DayVisionRangeBonus") == 0 ||
				strcmp(name, "CaveVisionRangeBonus") == 0 ||
				strcmp(name, "BrightLightVisionRangeBonus") == 0 ||
				strcmp(name, "ItemSizeBonus") == 0 ||
				strcmp(name, "LeatherJacket") == 0 ||
				strcmp(name, "NeedsBatteries") == 0 ||
				strcmp(name, "Batteries") == 0 ||
				strcmp(name, "XRay") == 0 ||
				strcmp(name, "WireCutters") == 0 ||
				strcmp(name, "Toolkit") == 0 ||
				strcmp(name, "Canteen") == 0 ||
				strcmp(name, "Jar") == 0 ||
				strcmp(name, "CanAndString") == 0 ||
				strcmp(name, "Marbles") == 0 ||
				strcmp(name, "Walkman") == 0 ||
				strcmp(name, "RemoteTrigger") == 0 ||
				strcmp(name, "RobotRemoteControl") == 0 ||
				strcmp(name, "CamouflageKit") == 0 ||
				strcmp(name, "LocksmithKit") == 0 ||
				strcmp(name, "Mine") == 0 ||
				strcmp(name, "AntitankMine" ) == 0 ||
				strcmp(name, "GasCan") == 0 ||
				strcmp(name, "ContainsLiquid") == 0 ||
				strcmp(name, "Rock") == 0 ||
				strcmp(name, "LockBomb") == 0 ||
				strcmp(name, "Flare") == 0 ||
				strcmp(name, "MetalDetector") == 0 ||
				strcmp(name, "FingerPrintID") == 0 ||
				strcmp(name, "Cannon") == 0 ||
				strcmp(name, "RocketRifle") == 0 ||
				strcmp(name, "MedicalKit") == 0 ||
				strcmp(name, "FirstAidKit") == 0 ||
				strcmp(name, "MagSizeBonus") == 0 ||
				strcmp(name, "PercentAutofireAPReduction") == 0 ||
				strcmp(name, "PercentBurstFireAPReduction") == 0 ||
				strcmp(name, "AutoFireToHitBonus") == 0 ||
				strcmp(name, "APBonus") == 0 ||
				strcmp(name, "RateOfFireBonus") == 0 ||
				strcmp(name, "BurstSizeBonus") == 0 ||
				strcmp(name, "PercentReadyTimeAPReduction") == 0 ||
				strcmp(name, "PercentReloadTimeAPReduction") == 0 ||
				strcmp(name, "BulletSpeedBonus") == 0 ||
				strcmp(name, "BurstToHitBonus") == 0 ||
				strcmp(name, "ThermalOptics") == 0 ||
				strcmp(name, "PercentTunnelVision") == 0 ||
				strcmp(name, "DefaultAttachment") == 0 ||
				strcmp(name, "CamoBonus") == 0 ||
				strcmp(name, "UrbanCamoBonus") == 0 ||
				strcmp(name, "DesertCamoBonus") == 0 ||
				strcmp(name, "SnowCamoBonus") == 0 ||
				strcmp(name, "StealthBonus") == 0 ||
				strcmp(name, "SciFi") == 0 ||
				strcmp(name, "NewInv") == 0 ||
				strcmp(name, "AttachmentSystem") == 0 ||
				//zilpin: pellet spread patterns externalized in XML
				strcmp(name, "spreadPattern") == 0 ||
				// HEADROCK HAM 4: new NCTH variables.
				strcmp(name, "ScopeMagFactor") == 0 ||
				strcmp(name, "ProjectionFactor") == 0 ||
				strcmp(name, "PercentAccuracyModifier") == 0 ||
				strcmp(name, "RecoilModifierX") == 0 ||
				strcmp(name, "RecoilModifierY") == 0 ||
				strcmp(name, "PercentRecoilModifier") == 0 ||
				strcmp(name, "Barrel") == 0 ||
				strcmp(name, "usOverheatingCooldownFactor") == 0 ||
				strcmp(name, "overheatTemperatureModificator") == 0 ||
				strcmp(name, "overheatCooldownModificator") == 0 ||
				strcmp(name, "overheatJamThresholdModificator") == 0 ||
				strcmp(name, "overheatDamageThresholdModificator") == 0 ||
				strcmp(name, "AttachmentClass") == 0 ||
				strcmp(name, "TripWireActivation") == 0 ||
				strcmp(name, "TripWire") == 0 ||
				strcmp(name, "Directional") == 0 ||
				strcmp(name, "DrugType") == 0 ||
				strcmp(name, "BlockIronSight") == 0 ||
				strcmp(name, "ItemFlag") == 0 ||
				strcmp(name, "FoodType") == 0 ||
				strcmp(name, "DamageChance") == 0 ||
				strcmp(name, "DirtIncreaseFactor") == 0 ||

				strcmp(name, "fFlags") == 0 ||
				//JMich_SkillModifiers: Adding new flags
				strcmp(name, "LockPickModifier") == 0 ||
				strcmp(name, "CrowbarModifier") == 0 ||
				strcmp(name, "DisarmModifier") == 0 ||
				strcmp(name, "RepairModifier") == 0 ||
				strcmp(name, "usHackingModifier" ) == 0 ||
				strcmp(name, "usBurialModifier" ) == 0 ||
				
				strcmp(name, "usActionItemFlag") == 0 ||
				strcmp(name, "clothestype") == 0 ||
				strcmp(name, "randomitem") == 0 ||
				strcmp(name, "randomitemcoolnessmodificator") == 0 ||
				strcmp(name, "FlashLightRange") == 0 ||
				strcmp(name, "ItemChoiceTimeSetting") == 0 ||
				strcmp(name, "buddyitem") == 0 ||
				strcmp(name, "SleepModifier") == 0 ||
				strcmp(name, "usSpotting") == 0 ||
				strcmp(name, "sBackpackWeightModifier") == 0 ||
				strcmp(name, "AllowClimbing") == 0 ||
				strcmp(name, "Cigarette" ) == 0 ||
				strcmp(name, "cigarette" ) == 0 ||
				strcmp(name, "usPortionSize" ) == 0 ||
				strcmp(name, "DiseaseprotectionFace" ) == 0 ||
				strcmp(name, "DiseaseprotectionHand" ) == 0||
				strcmp(name, "usRiotShieldStrength" ) == 0 ||
				strcmp(name, "usRiotShieldGraphic" ) == 0 ||
				strcmp(name, "Bloodbag") == 0 ||
				strcmp(name, "Manpad" ) == 0 ||
				strcmp(name, "Beartrap") == 0 ||
				strcmp(name, "Camera") == 0 ||
				strcmp(name, "Waterdrum") == 0 ||
				strcmp(name, "BloodcatMeat") == 0 ||
				strcmp(name, "CowMeat") == 0 ||
				strcmp(name, "Beltfed") == 0 ||
				strcmp(name, "Ammobelt") == 0 ||
				strcmp(name, "AmmobeltVest") == 0 ||
				strcmp(name, "CamoRemoval") == 0 ||
				strcmp(name, "Cleaningkit") == 0 ||
				strcmp(name, "AttentionItem") == 0 ||
				strcmp(name, "Garotte") == 0 ||
				strcmp(name, "Covert") == 0 ||
				strcmp(name, "Corpse") == 0 ||
				strcmp(name, "BloodcatSkin") == 0 ||
				strcmp(name, "NoMetalDetection") == 0 ||
				strcmp(name, "JumpGrenade") == 0 ||
				strcmp(name, "Handcuffs") == 0 ||
				strcmp(name, "Taser") == 0 ||
				strcmp(name, "ScubaBottle") == 0 ||
				strcmp(name, "ScubaMask") == 0 ||
				strcmp(name, "ScubaFins") == 0 ||
				strcmp(name, "TripwireRoll") == 0 ||
				strcmp(name, "Radioset") == 0 ||
				strcmp(name, "SignalShell") == 0 ||
				strcmp(name, "Soda") == 0 ||
				strcmp(name, "RoofcollapseItem") == 0 ||
				strcmp(name, "LBEexplosionproof") == 0 ||
				strcmp(name, "EmptyBloodbag" ) == 0 ||
				strcmp(name, "MedicalSplint" ) == 0 ||
				strcmp(name, "sFireResistance" ) == 0 ||
				strcmp(name, "usAdministrationModifier" ) == 0 ||
				strcmp(name, "RobotDamageReduction") == 0 ||
				strcmp(name, "RobotStrBonus") == 0 ||
				strcmp(name, "RobotAgiBonus") == 0 ||
				strcmp(name, "RobotDexBonus") == 0 ||
				strcmp(name, "RobotTargetingSkillGrant") == 0 ||
				strcmp(name, "RobotChassisSkillGrant") == 0 ||
				strcmp(name, "RobotUtilitySkillGrant") == 0 ||
				strcmp(name, "ProvidesRobotCamo") == 0 ||
				strcmp(name, "ProvidesRobotNightVision") == 0 ||
				strcmp(name, "ProvidesRobotLaserBonus") == 0 ||
				strcmp(name, "DiseaseSystemExclusive") == 0 ||
				strcmp(name, "TransportGroupMinProgress") == 0 ||
				strcmp(name, "TransportGroupMaxProgress") == 0
				))
		{
			pData->curElement = ELEMENT_PROPERTY;
			//DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("itemStartElementHandle: going into element, name = %s",name) );

			pData->maxReadDepth++; //we are not skipping this element
		}
		// HEADROCK HAM 4: New depth: Stance-based variables
		else if(pData->curElement == ELEMENT &&
				(strcmp(name, "STAND_MODIFIERS") == 0 ||
				strcmp(name, "CROUCH_MODIFIERS") == 0 ||
				strcmp(name, "PRONE_MODIFIERS") == 0))
		{
			pData->curElement = ELEMENT_SUBLIST;

			// Set current stance.
			if (strcmp(name, "STAND_MODIFIERS") == 0)
			{
				pData->curStance = 0;
			}
			else if (strcmp(name, "CROUCH_MODIFIERS") == 0)
			{
				pData->curStance = 1;
			}
			else // prone
			{
				pData->curStance = 2;
			}

			pData->maxReadDepth++;
		}
		
		// HEADROCK HAM 4: Read stance-based variables
		else if(pData->curElement == ELEMENT_SUBLIST &&
				(strcmp(name, "FlatBase") == 0 ||
				strcmp(name, "PercentBase") == 0 ||
				strcmp(name, "FlatAim") == 0 ||
				strcmp(name, "PercentAim") == 0 ||
				strcmp(name, "PercentCap") == 0 ||
				strcmp(name, "PercentHandling") == 0 ||
				strcmp(name, "PercentTargetTrackingSpeed") == 0 ||
				strcmp(name, "PercentDropCompensation") == 0 ||
				strcmp(name, "PercentMaxCounterForce") == 0 ||
				strcmp(name, "PercentCounterForceAccuracy") == 0 ||
				strcmp(name, "AimLevels") == 0))
		{
			pData->curElement = ELEMENT_SUBLIST_PROPERTY;

			pData->maxReadDepth++; //we are not skipping this element
		}

		pData->characterData.clear();
	}

	pData->currentDepth++;

}

static void
itemCharacterDataHandleImpl(void *userData, const XML_Char *str, int len)
{
	itemParseData * pData = (itemParseData *)userData;

	if (pData->currentDepth <= pData->maxReadDepth &&
		(pData->curElement == ELEMENT_PROPERTY ||
			pData->curElement == ELEMENT_SUBLIST_PROPERTY) &&
		(len < 0 || !pData->characterData.append(
			str, static_cast<std::size_t>(len))))
	{
		FailItemParse(pData, ItemDataStagingModel::Failure::MalformedInput);
	}
}


static void
itemEndElementHandleImpl(void *userData, const XML_Char *name)
{
	itemParseData * pData = (itemParseData *)userData;

	if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
	{
		if(strcmp(name, "ITEMLIST") == 0)
		{
			pData->curElement = ELEMENT_NONE;
			pData->completedItemList = true;
		}
		else if(strcmp(name, "ITEM") == 0)
		{
			pData->curElement = ELEMENT_LIST;
			const std::optional<std::size_t> itemIndex = pData->hasIndex
				? std::optional<std::size_t>(pData->curItem.uiIndex)
				: std::nullopt;

			if (!pData->failed && pData->localizedTextOnly)
			{
				const auto stageResult = pData->localizedLoad->stage(
					itemIndex, std::move(pData->localizedPatch));
				if (stageResult ==
					ItemDataStagingModel::StageResult::RejectedStagingFailure)
				{
					FailItemParse(pData,
						ItemDataStagingModel::Failure::StagingFailed);
				}
			}
			else if (!pData->failed)
			{
				const bool publishesItem = pData->curItem.usItemClass != 0;
				if (publishesItem)
				{
					// HEADROCK HAM 4: Inherit stance-base modifiers upwards.
					InheritStanceModifiers( pData );
				}
				const auto stageResult = pData->baseLoad->stage(
					itemIndex, pData->curItem, publishesItem,
					pData->currentAuxiliary);
				if (stageResult ==
					ItemDataStagingModel::StageResult::RejectedStagingFailure)
				{
					FailItemParse(pData,
						ItemDataStagingModel::Failure::StagingFailed);
				}
			}
		}
		else if(strcmp(name, "uiIndex") == 0)
		{
			pData->curElement = ELEMENT;
			UINT32 index = 0;
			if (ParseItemInteger(pData, index))
			{
				pData->curItem.uiIndex = index;
				pData->hasIndex = true;
			}
		}
		else if(strcmp(name, "szItemName") == 0)
		{
			//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"itemEndElementHandle: itemname");
			pData->curElement = ELEMENT;
			ParseItemText(pData, pData->curItem.szItemName,
				pData->localizedTextOnly ? &pData->localizedPatch.itemName : nullptr);
		}
		else if(strcmp(name, "szLongItemName") == 0)
		{
			//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"itemEndElementHandle: longitemname");
			pData->curElement = ELEMENT;

			ParseItemText(pData, pData->curItem.szLongItemName,
				pData->localizedTextOnly ? &pData->localizedPatch.longItemName : nullptr);
		}
		else if(strcmp(name, "szItemDesc") == 0)
		{
			//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"itemEndElementHandle: itemdesc");
			pData->curElement = ELEMENT;

			ParseItemText(pData, pData->curItem.szItemDesc,
				pData->localizedTextOnly ? &pData->localizedPatch.itemDescription : nullptr);
		}
		else if(strcmp(name, "szBRName") == 0)
		{
			//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"itemEndElementHandle: brname");
			pData->curElement = ELEMENT;

			ParseItemText(pData, pData->curItem.szBRName,
				pData->localizedTextOnly ? &pData->localizedPatch.storeName : nullptr);
		}
		else if(strcmp(name, "szBRDesc") == 0)
		{
			//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"itemEndElementHandle: brdesc");
			pData->curElement = ELEMENT;

			ParseItemText(pData, pData->curItem.szBRDesc,
				pData->localizedTextOnly ? &pData->localizedPatch.storeDescription : nullptr);
		}
		else if(strcmp(name, "usItemClass") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.usItemClass = ParseItemIntegerValue<UINT32>(pData, ItemDataStagingModel::IntegerSyntax::CStyle);
		}
		else if(strcmp(name, "nasAttachmentClass") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.nasAttachmentClass = ParseItemIntegerValue<UINT64>(pData);
		}
		else if(strcmp(name, "nasLayoutClass") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.nasLayoutClass = ParseItemIntegerValue<UINT64>(pData);
		}
		else if(strcmp(name, "AvailableAttachmentPoint") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ulAvailableAttachmentPoint |= ParseItemIntegerValue<UINT64>(pData);
		}
		else if(strcmp(name, "AttachmentPoint") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ulAttachmentPoint = ParseItemIntegerValue<UINT64>(pData);
		}
		else if(strcmp(name, "AttachToPointAPCost") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubAttachToPointAPCost = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "ubClassIndex") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubClassIndex = ParseItemIntegerValue<UINT16>(pData);
		}
		else if(strcmp(name, "ubCursor") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubCursor = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "bSoundType") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bSoundType = ParseItemIntegerValue<INT8>(pData);
		}
		else if(strcmp(name, "ubGraphicType") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubGraphicType = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "ubGraphicNum") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubGraphicNum = ParseItemIntegerValue<UINT16>(pData);
		}
		else if(strcmp(name, "ubWeight") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubWeight = ParseItemIntegerValue<UINT16>(pData);
		}
		else if(strcmp(name, "ubPerPocket") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubPerPocket = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "ItemSize") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ItemSize = ParseItemIntegerValue<UINT16>(pData);
		}
		else if(strcmp(name, "usPrice") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.usPrice = ParseLegacyItemPrice(pData);
		}
		else if(strcmp(name, "ubCoolness") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubCoolness = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "bReliability") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bReliability = ParseItemIntegerValue<INT8>(pData);
		}
		else if(strcmp(name, "bRepairEase") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bRepairEase = ParseItemIntegerValue<INT8>(pData);
		}
		else if(strcmp(name, "Damageable")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_damageable;
		}
		else if(strcmp(name, "Repairable")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_repairable;
		}
		else if(strcmp(name, "WaterDamages")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_waterdamages;
		}
		else if(strcmp(name, "Metal")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_metal;
		}
		else if(strcmp(name, "Sinks")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_sinks;
		}
		else if(strcmp(name, "ShowStatus")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_showstatus;
		}
		else if(strcmp(name, "HiddenAddon")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_hiddenaddon;
		}
		else if(strcmp(name, "TwoHanded")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_twohanded;
		}
		else if(strcmp(name, "NotBuyable")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_notbuyable;
		}
		else if(strcmp(name, "Attachment")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_attachment;
		}
		else if(strcmp(name, "HiddenAttachment")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_hiddenattachment;
		}
		else if(strcmp(name, "BigGunList")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_biggunlist;
		}
		else if(strcmp(name, "NotInEditor")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_notineditor;
		}
		else if(strcmp(name, "DefaultUndroppable")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_defaultundroppable;
		}
		else if(strcmp(name, "Unaerodynamic")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_unaerodynamic;
		}
		else if(strcmp(name, "Electronic")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_electronic;
		}
		else if(strcmp(name, "Inseparable")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.inseparable = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "BR_NewInventory")	 == 0)
		{
			pData->curElement = ELEMENT;
			UINT8 value = 0;
			if (ParseItemInteger(pData, value))
				pData->currentAuxiliary.newInventory = value;
		}
		else if(strcmp(name, "BR_UsedInventory")	 == 0)
		{
			pData->curElement = ELEMENT;
			UINT8 value = 0;
			if (ParseItemInteger(pData, value))
				pData->currentAuxiliary.usedInventory = value;
		}
		else if(strcmp(name, "BR_ROF")	 == 0)
		{
			pData->curElement = ELEMENT;
			INT16 value = 0;
			if (ParseItemInteger(pData, value))
				pData->currentAuxiliary.weaponRateOfFire = value;
		}
		else if(strcmp(name, "CamoBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.camobonus = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "DesertCamoBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.desertCamobonus = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "UrbanCamoBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.urbanCamobonus = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "SnowCamoBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.snowCamobonus = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "StealthBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.stealthbonus = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentNoiseReduction")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentnoisereduction = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "GasMask")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_gasmask;
		}
		else if(strcmp(name, "Bipod")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bipod = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "ToHitBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.tohitbonus = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "RangeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.rangebonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentRangeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentrangebonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "AimBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.aimbonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "MinRangeForAimBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.minrangeforaimbonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentAPReduction")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentapreduction  = ParseItemIntegerValue<INT16>(pData);
		}

		else if(strcmp(name, "MagSizeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.magsizebonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentAutofireAPReduction")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentautofireapreduction  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentBurstFireAPReduction")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentburstfireapreduction  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "AutoFireToHitBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.autofiretohitbonus   = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "APBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.APBonus = ParseItemIntegerValue<INT16>(pData);
			pData->curItem.APBonus = (INT16)DynamicAdjustAPConstants(pData->curItem.APBonus, pData->curItem.APBonus);
		}
		else if(strcmp(name, "RateOfFireBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.rateoffirebonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "BurstSizeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.burstsizebonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "BurstToHitBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bursttohitbonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentReadyTimeAPReduction")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentreadytimeapreduction  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentReloadTimeAPReduction")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentreloadtimeapreduction  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "BulletSpeedBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bulletspeedbonus  = ParseItemIntegerValue<INT16>(pData);
		}

		else if(strcmp(name, "PercentStatusDrainReduction")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentstatusdrainreduction  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "GrenadeLauncher")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_grenadelauncher;
		}
		else if(strcmp(name, "LockBomb")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_lockbomb;
		}
		else if(strcmp(name, "Flare")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_flare;
		}
		else if(strcmp(name, "Duckbill")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_duckbill;
		}
		else if(strcmp(name, "ThermalOptics")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_thermaloptics;
		}
		else if(strcmp(name, "SciFi")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_scifi;
		}
		else if(strcmp(name, "NewInv")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_newinv;
		}
		else if(strcmp(name, "AttachmentSystem")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubAttachmentSystem   = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "HideMuzzleFlash")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_hidemuzzleflash;
		}
		else if(strcmp(name, "Cannon")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_cannon;
		}
		else if(strcmp(name, "RocketRifle")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_rocketrifle;
		}
		else if(strcmp(name, "Alcohol")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.alcohol  = ParseItemFloatValue(pData);

			pData->curItem.alcohol = max( 0.0f, pData->curItem.alcohol );
		}
		else if(strcmp(name, "Hardware")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_hardware;
		}
		else if(strcmp(name, "Medical")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_medical;
		}
		else if(strcmp(name, "DamageBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.damagebonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "MeleeDamageBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.meleedamagebonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "Mortar")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_mortar;
		}
		else if(strcmp(name, "RocketLauncher")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_rocketlauncher;
		}
		else if(strcmp(name, "SingleShotRocketLauncher")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_singleshotrocketlauncher;
		}
		else if(strcmp(name, "DiscardedLauncherItem")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.discardedlauncheritem  = ParseItemIntegerValue<UINT16>(pData);
		}
		else if(strcmp(name, "BloodiedItem")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bloodieditem  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "DefaultAttachment")	 == 0)
		{
			pData->curElement = ELEMENT;
			const UINT16 attachment = ParseItemIntegerValue<UINT16>(pData);
			for(UINT8 cnt = 0; cnt < MAX_DEFAULT_ATTACHMENTS; cnt++){
				if(pData->curItem.defaultattachments[cnt] == 0){
					pData->curItem.defaultattachments[cnt] = attachment;
					break;
				}
			}
		}
		else if(strcmp(name, "BrassKnuckles")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_brassknuckles;
		}
		else if(strcmp(name, "Crowbar")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_crowbar;
		}
		else if(strcmp(name, "GLGrenade")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_glgrenade;
		}
		else if(strcmp(name, "FlakJacket")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_flakjacket;
		}
		else if(strcmp(name, "HearingRangeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.hearingrangebonus  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "VisionRangeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.visionrangebonus   = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "NightVisionRangeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.nightvisionrangebonus   = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "DayVisionRangeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.dayvisionrangebonus   = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "CaveVisionRangeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.cavevisionrangebonus   = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "BrightLightVisionRangeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.brightlightvisionrangebonus    = ParseItemIntegerValue<INT16>(pData);
		}
		if(strcmp(name, "ItemSizeBonus")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.itemsizebonus    = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "LeatherJacket")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_leatherjacket;
		}
		else if(strcmp(name, "NeedsBatteries")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_needsbatteries;
		}
		else if(strcmp(name, "Batteries")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_batteries;
		}
		else if(strcmp(name, "XRay")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_xray;
		}
		else if(strcmp(name, "WireCutters")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_wirecutters;
		}
		else if(strcmp(name, "Toolkit")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_toolkit;
		}
		else if(strcmp(name, "Canteen")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_canteen;
		}
		else if(strcmp(name, "Marbles")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_marbles;
		}
		else if(strcmp(name, "Walkman")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_walkman;
		}
		else if(strcmp(name, "RemoteTrigger")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_remotetrigger;
		}
		else if(strcmp(name, "RobotRemoteControl")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_robotremotecontrol;
		}
		else if(strcmp(name, "CamouflageKit")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_camouflagekit;
		}
		else if(strcmp(name, "LocksmithKit")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_locksmithkit;
		}
		else if(strcmp(name, "Mine")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_mine;
		}
		else if ( strcmp( name, "AntitankMine" ) == 0 )
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_antitankmine;
		}
		else if(strcmp(name, "GasCan")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_gascan;
		}
		else if(strcmp(name, "CanAndString")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_canandstring;
		}
		else if(strcmp(name, "ContainsLiquid")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_containsliquid;
		}
		else if(strcmp(name, "FingerPrintID")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_fingerprintid;
		}
		else if(strcmp(name, "Rock")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_rock;
		}
		else if(strcmp(name, "MedicalKit")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_medicalkit;
		}
		else if(strcmp(name, "FirstAidKit")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_firstaidkit;
		}
		else if(strcmp(name, "MetalDetector")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag |= ITEM_metaldetector;
		}
		if(strcmp(name, "PercentTunnelVision")	 == 0) //Madd: had to scrap the "else" due to a compiler limit
		{
			pData->curElement = ELEMENT;
			pData->curItem.percenttunnelvision  = ParseItemIntegerValue<UINT8>(pData);
		}
		if(strcmp(name, "Jar")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_jar;
		}
		if(strcmp(name, "BestLaserRange")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bestlaserrange    = ParseItemIntegerValue<INT16>(pData);
		}
		//zilpin: pellet spread patterns externalized in XML
		if(strcmp(name, "spreadPattern") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.spreadPattern = FindSpreadPatternIndex( pData->characterData.c_str() );
		}

		//////////////////////////////////////////////////////////////////
		// HEADROCK HAM 4: Read new variables from XML
		else if(strcmp(name, "ScopeMagFactor")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.scopemagfactor  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "ProjectionFactor")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.projectionfactor  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "PercentAccuracyModifier")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.percentaccuracymodifier  = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "RecoilModifierX")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.RecoilModifierX  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "RecoilModifierY")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.RecoilModifierY  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "PercentRecoilModifier") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.PercentRecoilModifier = ParseItemIntegerValue<INT16>(pData);
		}


		//////////////////////////////////////////////////////////////////
		// HEADROCK HAM 4: Read stance-based variables and put them into the right place.
		else if(strcmp(name, "FlatBase") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.flatbasemodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentBase") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.percentbasemodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "FlatAim") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.flataimmodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentAim") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.percentaimmodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentCap") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.percentcapmodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentHandling") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.percenthandlingmodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentTargetTrackingSpeed") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.targettrackingmodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentDropCompensation") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.percentdropcompensationmodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentMaxCounterForce") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.maxcounterforcemodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "PercentCounterForceAccuracy") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.counterforceaccuracymodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}
		else if(strcmp(name, "AimLevels") == 0)
		{
			pData->curElement = ELEMENT_SUBLIST;
			pData->curItem.aimlevelsmodifier[pData->curStance] = ParseItemIntegerValue<INT16>(pData);
		}

		//////////////////////////////////////////////////////////////////
		// HEADROCK HAM 4: Close opened Stance Tags.
		else if(strcmp(name, "STAND_MODIFIERS") == 0 ||
				strcmp(name, "CROUCH_MODIFIERS") == 0 ||
				strcmp(name, "PRONE_MODIFIERS") == 0)
		{
			pData->curElement = ELEMENT;
		}

		// Flugente
		else if(strcmp(name, "Barrel")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_barrel;
		}
		else if(strcmp(name, "usOverheatingCooldownFactor") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.usOverheatingCooldownFactor  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "overheatTemperatureModificator") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.overheatTemperatureModificator  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "overheatCooldownModificator") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.overheatCooldownModificator  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "overheatJamThresholdModificator") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.overheatJamThresholdModificator  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "overheatDamageThresholdModificator") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.overheatDamageThresholdModificator  = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "AttachmentClass")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.attachmentclass   = ParseItemIntegerValue<UINT32>(pData, ItemDataStagingModel::IntegerSyntax::CStyle);
		}
		else if(strcmp(name, "TripWireActivation")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_tripwireactivation;
		}
		else if(strcmp(name, "TripWire")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_tripwire;
		}
		else if(strcmp(name, "Directional")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_directional;
		}
		else if(strcmp(name, "DrugType")	 == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.drugtype   = ParseItemIntegerValue<UINT32>(pData, ItemDataStagingModel::IntegerSyntax::CStyle);
			if ( pData->curItem.drugtype >= NEW_DRUGS_MAX )	// clamp: unbounded DrugType -> OOB into NewDrug[NEW_DRUGS_MAX] in ApplyDrugs_New
				pData->curItem.drugtype = 0;
		}
		else if(strcmp(name, "BlockIronSight")	 == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_blockironsight;
		}
		else if(strcmp(name, "FoodType") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.foodtype = ParseItemIntegerValue<UINT32>(pData, ItemDataStagingModel::IntegerSyntax::CStyle);
		}
		//JMich_SkillsModifiers: Parse new values
		else if(strcmp(name, "LockPickModifier") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.LockPickModifier = ParseItemIntegerValue<INT8>(pData);
		}
		else if(strcmp(name, "CrowbarModifier") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.CrowbarModifier = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "DisarmModifier") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.DisarmModifier = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "RepairModifier") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.RepairModifier = ParseItemIntegerValue<INT8>(pData);
		}
		else if ( strcmp( name, "usHackingModifier" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curItem.usHackingModifier =
				ParseItemClampedIntegerValue<UINT8>(pData, 0, 100);
		}
		else if ( strcmp( name, "usBurialModifier" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curItem.usBurialModifier =
				ParseItemClampedIntegerValue<UINT8>(pData, 0, 100);
		}
		else if(strcmp(name, "DamageChance") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.usDamageChance = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "DirtIncreaseFactor") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.dirtIncreaseFactor = ParseItemFloatValue(pData);
		}
		else if(strcmp(name, "usActionItemFlag") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.usActionItemFlag = ParseItemIntegerValue<UINT32>(pData, ItemDataStagingModel::IntegerSyntax::CStyle);
		}
		else if(strcmp(name, "clothestype") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.clothestype = ParseItemIntegerValue<UINT32>(pData, ItemDataStagingModel::IntegerSyntax::CStyle);
		}
		else if(strcmp(name, "randomitem") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.randomitem = ParseItemIntegerValue<UINT16>(pData);
		}
		else if(strcmp(name, "randomitemcoolnessmodificator") == 0)
		{
			pData->curElement = ELEMENT;
			// no nonsense, only values between -20 and + 20
			pData->curItem.randomitemcoolnessmodificator =
				ParseItemClampedIntegerValue<INT8>(pData, -20, 20);
		}
		else if(strcmp(name, "FlashLightRange") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.usFlashLightRange = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "ItemChoiceTimeSetting") == 0)
		{
			pData->curElement = ELEMENT;
			// no nonsense, only values between 0 and + 2
			pData->curItem.usItemChoiceTimeSetting =
				ParseItemClampedIntegerValue<UINT8>(pData, 0, 2);
		}
		else if(strcmp(name, "buddyitem") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.usBuddyItem = ParseItemIntegerValue<UINT16>(pData);
		}
		else if(strcmp(name, "SleepModifier") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.ubSleepModifier = ParseItemIntegerValue<UINT8>(pData);
		}
		else if(strcmp(name, "usSpotting") == 0)
		{
			pData->curElement = ELEMENT;
			// values between 0 and 100 only
			pData->curItem.usSpotting =
				ParseItemClampedIntegerValue<INT16>(pData, 0, 100);
		}
		else if (strcmp(name, "sBackpackWeightModifier") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.sBackpackWeightModifier = ParseItemIntegerValue<INT16>(pData);
		}
		else if (strcmp(name, "AllowClimbing") == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_fAllowClimbing;
		}
		else if ( strcmp( name, "Cigarette" ) == 0 ||
			strcmp( name, "cigarette" ) == 0 )
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_cigarette;
		}
		else if ( strcmp( name, "usPortionSize" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curItem.usPortionSize = ParseItemIntegerValue<UINT8>(pData);
		}
		// Flugente: simple tags in the xml get translated into flags
		else if ( strcmp( name, "DiseaseprotectionFace" ) == 0 )
		{
			pData->curElement = ELEMENT;
			BOOLEAN val = ParseItemBooleanValue(pData);

			if ( val )
				pData->curItem.usItemFlag |= DISEASEPROTECTION_1;
		}
		else if ( strcmp( name, "DiseaseprotectionHand" ) == 0 )
		{
			pData->curElement = ELEMENT;
			BOOLEAN val = ParseItemBooleanValue(pData);

			if ( val )
				pData->curItem.usItemFlag |= DISEASEPROTECTION_2;
		}
		else if ( strcmp( name, "usRiotShieldStrength" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curItem.usRiotShieldStrength =
				ParseItemClampedIntegerValue<UINT16>(pData, 0, 100);
		}
		else if ( strcmp( name, "usRiotShieldGraphic" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curItem.usRiotShieldGraphic = ParseItemIntegerValue<UINT16>(pData);
		}
		else if ( strcmp( name, "Bloodbag" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= BLOOD_BAG;
		}
		else if ( strcmp( name, "Manpad" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= MANPAD;
		}
		else if ( strcmp( name, "Beartrap" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= BEARTRAP;
		}
		else if ( strcmp( name, "Camera" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= CAMERA;
		}
		else if ( strcmp( name, "Waterdrum" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= WATER_DRUM;
		}
		else if ( strcmp( name, "BloodcatMeat" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= MEAT_BLOODCAT;
		}
		else if ( strcmp( name, "CowMeat" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= MEAT_COW;
		}
		else if ( strcmp( name, "Beltfed" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= BELT_FED;
		}
		else if ( strcmp( name, "Ammobelt" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= AMMO_BELT;
		}
		else if ( strcmp( name, "AmmobeltVest" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= AMMO_BELT_VEST;
		}
		else if ( strcmp( name, "CamoRemoval" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= CAMO_REMOVAL;
		}
		else if ( strcmp( name, "Cleaningkit" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= CLEANING_KIT;
		}
		else if ( strcmp( name, "AttentionItem" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= ATTENTION_ITEM;
		}
		else if ( strcmp( name, "Garotte" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= GAROTTE;
		}
		else if ( strcmp( name, "Covert" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= COVERT;
		}
		else if ( strcmp( name, "Corpse" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= CORPSE;
		}
		else if ( strcmp( name, "BloodcatSkin" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= SKIN_BLOODCAT;
		}
		else if ( strcmp( name, "NoMetalDetection" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= NO_METAL_DETECTION;
		}
		else if ( strcmp( name, "JumpGrenade" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= JUMP_GRENADE;
		}
		else if ( strcmp( name, "Handcuffs" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= HANDCUFFS;
		}
		else if ( strcmp( name, "Taser" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= TASER;
		}
		else if ( strcmp( name, "ScubaBottle" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= SCUBA_BOTTLE;
		}
		else if ( strcmp( name, "ScubaMask" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= SCUBA_MASK;
		}
		else if ( strcmp( name, "ScubaFins" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= SCUBA_FINS;
		}
		else if ( strcmp( name, "TripwireRoll" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= TRIPWIREROLL;
		}
		else if ( strcmp( name, "Radioset" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= RADIO_SET;
		}
		else if ( strcmp( name, "SignalShell" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= SIGNAL_SHELL;
		}
		else if ( strcmp( name, "Soda" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= SODA;
		}
		else if ( strcmp( name, "RoofcollapseItem" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= ROOF_COLLAPSE_ITEM;
		}
		else if ( strcmp( name, "LBEexplosionproof" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= LBE_EXPLOSIONPROOF;
		}
		else if ( strcmp( name, "EmptyBloodbag" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= EMPTY_BLOOD_BAG;
		}
		else if ( strcmp( name, "MedicalSplint" ) == 0 )
		{
			pData->curElement = ELEMENT;

			if ( ParseItemBooleanValue(pData) )
				pData->curItem.usItemFlag |= MEDICAL_SPLINT;
		}
		else if ( strcmp( name, "sFireResistance" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curItem.sFireResistance =
				ParseItemClampedIntegerValue<INT16>(pData,
					std::numeric_limits<INT16>::min(), 100);
		}
		else if ( strcmp( name, "usAdministrationModifier" ) == 0 )
		{
			pData->curElement = ELEMENT;
			pData->curItem.usAdministrationModifier = ParseItemIntegerValue<UINT8>(pData);
		}
		else if (strcmp(name, "RobotDamageReduction") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.fRobotDamageReductionModifier = ParseItemFloatValue(pData);
		}
		else if (strcmp(name, "RobotStrBonus") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bRobotStrBonus = ParseItemIntegerValue<INT8>(pData);
		}
		else if (strcmp(name, "RobotAgiBonus") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bRobotAgiBonus = ParseItemIntegerValue<INT8>(pData);
		}
		else if (strcmp(name, "RobotDexBonus") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bRobotDexBonus = ParseItemIntegerValue<INT8>(pData);
		}
		else if (strcmp(name, "RobotTargetingSkillGrant") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bRobotTargetingSkillGrant = ParseItemIntegerValue<INT8>(pData);
		}
		else if (strcmp(name, "RobotChassisSkillGrant") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bRobotChassisSkillGrant = ParseItemIntegerValue<INT8>(pData);
		}
		else if (strcmp(name, "RobotUtilitySkillGrant") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.bRobotUtilitySkillGrant = ParseItemIntegerValue<INT8>(pData);
		}
		else if (strcmp(name, "ProvidesRobotCamo") == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_fProvidesRobotCamo;
		}
		else if (strcmp(name, "ProvidesRobotNightVision") == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_fProvidesRobotNightVision;
		}
		else if (strcmp(name, "ProvidesRobotLaserBonus") == 0)
		{
			pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_fProvidesRobotLaserBonus;
		}
		else if (strcmp(name, "DiseaseSystemExclusive") == 0)
		{
		    pData->curElement = ELEMENT;
			if (ParseItemBooleanValue(pData))
				pData->curItem.usItemFlag2 |= ITEM_DiseaseSystemExclusive;
		}
		else if (strcmp(name, "TransportGroupMinProgress") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.iTransportGroupMinProgress = ParseItemIntegerValue<INT8>(pData);
		}
		else if (strcmp(name, "TransportGroupMaxProgress") == 0)
		{
			pData->curElement = ELEMENT;
			pData->curItem.iTransportGroupMaxProgress = ParseItemIntegerValue<INT8>(pData);
		}
		else if (strcmp(name, "ItemFlag") == 0 ||
			strcmp(name, "fFlags") == 0 ||
			strcmp(name, "Detonator") == 0 ||
			strcmp(name, "RemoteDetonator") == 0)
		{
			// These established schema tags have never populated INVTYPE here.
			// Keep them as explicit compatibility no-ops so they cannot trap the
			// callback state at ELEMENT_PROPERTY and hide later fields.
			pData->curElement = ELEMENT;
		}
		else
		{
			// Defensively restore callback state if the recognized start/end tag
			// tables ever drift apart.
			if ( pData->curElement == ELEMENT_PROPERTY )
				pData->curElement = ELEMENT;
			else if ( pData->curElement == ELEMENT_SUBLIST_PROPERTY )
				pData->curElement = ELEMENT_SUBLIST;
		}

		--pData->maxReadDepth;
	}

	--pData->currentDepth;
}

static void XMLCALL itemStartElementHandle(
	void* userData, const XML_Char* name, const XML_Char** atts) noexcept
{
	auto* pData = static_cast<itemParseData*>(userData);
	if (pData->failed)
	{
		++pData->currentDepth;
		return;
	}
	try
	{
		itemStartElementHandleImpl(userData, name, atts);
	}
	catch (...)
	{
		// The implementation increments depth only after all throwing work.
		// Account for this start event, then make later callbacks inert.
		++pData->currentDepth;
		FailItemParse(pData, ItemDataStagingModel::Failure::StagingFailed);
	}
}

static void XMLCALL itemCharacterDataHandle(
	void* userData, const XML_Char* str, int len) noexcept
{
	auto* pData = static_cast<itemParseData*>(userData);
	if (pData->failed) return;
	try
	{
		itemCharacterDataHandleImpl(userData, str, len);
	}
	catch (...)
	{
		FailItemParse(pData, ItemDataStagingModel::Failure::StagingFailed);
	}
}

static void XMLCALL itemEndElementHandle(
	void* userData, const XML_Char* name) noexcept
{
	auto* pData = static_cast<itemParseData*>(userData);
	if (pData->failed)
	{
		if (pData->currentDepth > 0) --pData->currentDepth;
		return;
	}
	try
	{
		itemEndElementHandleImpl(userData, name);
	}
	catch (...)
	{
		// The implementation decrements depth only after all throwing work.
		// Account for this end event before suppressing later callback work.
		if (pData->currentDepth > 0) --pData->currentDepth;
		FailItemParse(pData, ItemDataStagingModel::Failure::StagingFailed);
	}
}

static_assert(BOBBY_RAY_LISTS == 2,
	"item XML staging models the established new/used store inventory schema");
static_assert(std::is_trivially_copyable_v<INVTYPE>,
	"transactional base publication requires an exact no-throw item copy");
static_assert(std::is_nothrow_move_constructible_v<itemLocalizedTextPatch> &&
	std::is_nothrow_move_assignable_v<itemLocalizedTextPatch>,
	"localized parser patches must move without throwing before staging");
static_assert(static_cast<std::uintmax_t>(MAXITEMS) <=
	std::numeric_limits<UINT32>::max(),
	"the exclusive loaded-item bound must fit gMAXITEMS_READ");

static ItemDataStagingModel::AuxiliaryTables SnapshotItemAuxiliaryTables()
{
	ItemDataStagingModel::AuxiliaryTables tables(MAXITEMS);
	for (std::size_t index = 0; index < MAXITEMS; ++index)
	{
		tables.storeInventory[index].newInventory =
			StoreInventory[index][BOBBY_RAY_NEW];
			tables.storeInventory[index].usedInventory =
			StoreInventory[index][BOBBY_RAY_USED];
		tables.weaponRateOfFire[index] = WeaponROF[index];
	}
	return tables;
}

static void PublishBaseItemTables(
	const ItemDataStagingModel::BasePublicationView<INVTYPE>& publication)
	noexcept
{
	// RequiredBaseLoadTransaction guarantees maxItemsRead <= its capacity.
	// MAXITEMS is statically proven to fit UINT32 above, so derive the complete
	// high-water value before the first live-table write.
	const UINT32 maxItemsRead =
		static_cast<UINT32>(publication.maxItemsRead);
	// A complete base file replaces Item[] with a fresh zero table. Sparse
	// staging retains only authored nonzero-class records, so clear the full
	// production capacity before applying that prevalidated set.
	std::memset(Item, 0, sizeof(Item));
	for (const auto& stagedItem : publication.items)
	{
		std::memcpy(&Item[stagedItem.index], &stagedItem.item,
			sizeof(INVTYPE));
	}
	for (std::size_t index = 0; index < MAXITEMS; ++index)
	{
		StoreInventory[index][BOBBY_RAY_NEW] =
			publication.auxiliary.storeInventory[index].newInventory;
		StoreInventory[index][BOBBY_RAY_USED] =
			publication.auxiliary.storeInventory[index].usedInventory;
		WeaponROF[index] = publication.auxiliary.weaponRateOfFire[index];
	}
	gMAXITEMS_READ = maxItemsRead;
}

static BOOLEAN CommitLocalizedItemLoad(LocalizedItemLoad& load)
{
	return load.commit(MAXITEMS, ValidateLocalizedItemText,
		PublishLocalizedItemText) ? TRUE : FALSE;
}

BOOLEAN ReadInItemStats(STR fileName, BOOLEAN localizedVersion) noexcept try
{
	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading Items.xml");

	std::optional<ItemDataStagingModel::AuxiliaryTables> liveAuxiliary;
	std::optional<BaseItemLoad> baseLoad;
	std::optional<LocalizedItemLoad> localizedLoad;
	itemParseData pData{};
	pData.localizedTextOnly = localizedVersion != FALSE;
	if (pData.localizedTextOnly)
	{
		localizedLoad.emplace(MAXITEMS);
		pData.localizedLoad = &*localizedLoad;
	}
	else
	{
		liveAuxiliary.emplace(SnapshotItemAuxiliaryTables());
		baseLoad.emplace(MAXITEMS, *liveAuxiliary);
		pData.baseLoad = &*baseLoad;
	}

	const LegacyXmlCallbacks callbacks{
		&pData, itemStartElementHandle, itemEndElementHandle,
		itemCharacterDataHandle};
	const LegacyXmlResult result = ParseLegacyXmlFile(fileName, callbacks);
	if (!result)
	{
		if (result.status == LegacyXmlStatus::NotFound)
		{
			if (pData.localizedTextOnly)
			{
				localizedLoad->resourceMissing();
				return CommitLocalizedItemLoad(*localizedLoad);
			}
			baseLoad->resourceMissing();
			return FALSE;
		}
		FailItemParse(&pData, result.status == LegacyXmlStatus::ReadError
			? ItemDataStagingModel::Failure::TruncatedInput
			: ItemDataStagingModel::Failure::MalformedInput);
		if (result.status != LegacyXmlStatus::ReadError)
		{
			const auto message = FormatLegacyXmlFailure(fileName, result);
			LiveMessage(message.data());
		}
		return FALSE;
	}

	if (pData.failed || !pData.sawItemList || !pData.completedItemList)
	{
		FailItemParse(&pData, ItemDataStagingModel::Failure::MalformedInput);
		return FALSE;
	}

	if (pData.localizedTextOnly)
	{
		localizedLoad->complete();
		return CommitLocalizedItemLoad(*localizedLoad);
	}

	baseLoad->complete();
	return baseLoad->commit(MAXITEMS, PublishBaseItemTables) ? TRUE : FALSE;
}
namespace
{
	template <typename Integer>
	void AddItemInteger(XMLWriter& writer, const char* tag, Integer value)
	{
		static_assert(std::is_integral_v<Integer>);
		if constexpr (std::is_signed_v<Integer>)
			writer.addValue(tag, static_cast<std::intmax_t>(value));
		else
			writer.addValue(tag, static_cast<std::uintmax_t>(value));
	}

	bool AddItemFloat(XMLWriter& writer, const char* tag, FLOAT value)
	{
		if (!std::isfinite(value)) return false;
		std::ostringstream formatted;
		formatted.imbue(std::locale::classic());
		formatted << std::setprecision(
			std::numeric_limits<double>::max_digits10)
			<< static_cast<double>(value);
		if (!formatted) return false;
		writer.addValue(tag, formatted.str());
		return true;
	}

	void AddItemBoolean(XMLWriter& writer, const char* tag, bool value)
	{
		if (value) AddItemInteger(writer, tag, 1);
	}

	bool AppendUtf8CodePoint(std::uint32_t codePoint, std::string& destination)
	{
		// XML 1.0 excludes these Unicode noncharacters even though their UTF-8
		// byte sequences contain no ASCII control byte for XMLWriter to reject.
		if (codePoint == 0xfffe || codePoint == 0xffff) return false;
		if (codePoint <= 0x7f)
		{
			destination.push_back(static_cast<char>(codePoint));
		}
		else if (codePoint <= 0x7ff)
		{
			destination.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
			destination.push_back(static_cast<char>(
				0x80 | (codePoint & 0x3f)));
		}
		else if (codePoint <= 0xffff)
		{
			if (codePoint >= 0xd800 && codePoint <= 0xdfff) return false;
			destination.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
			destination.push_back(static_cast<char>(
				0x80 | ((codePoint >> 6) & 0x3f)));
			destination.push_back(static_cast<char>(
				0x80 | (codePoint & 0x3f)));
		}
		else if (codePoint <= 0x10ffff)
		{
			destination.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
			destination.push_back(static_cast<char>(
				0x80 | ((codePoint >> 12) & 0x3f)));
			destination.push_back(static_cast<char>(
				0x80 | ((codePoint >> 6) & 0x3f)));
			destination.push_back(static_cast<char>(
				0x80 | (codePoint & 0x3f)));
		}
		else
		{
			return false;
		}
		return true;
	}

	template <std::size_t Capacity>
	bool ItemTextToUtf8(
		const CHAR16 (&source)[Capacity], std::string& destination)
	{
		std::size_t length = 0;
		while (length < Capacity && source[length] != L'\0') ++length;
		if (length == Capacity) return false;

		std::string staged;
		staged.reserve(length * (sizeof(CHAR16) == 2 ? 3 : 4));
		for (std::size_t index = 0; index < length; ++index)
		{
			if constexpr (std::is_signed_v<CHAR16>)
			{
				if (source[index] < 0) return false;
			}
			std::uint32_t codePoint = static_cast<std::uint32_t>(
				static_cast<std::make_unsigned_t<CHAR16>>(source[index]));
			if constexpr (sizeof(CHAR16) == 2)
			{
				if (codePoint >= 0xd800 && codePoint <= 0xdbff)
				{
					if (++index >= length) return false;
					const std::uint32_t low = static_cast<std::uint32_t>(
						static_cast<std::make_unsigned_t<CHAR16>>(
							source[index]));
					if (low < 0xdc00 || low > 0xdfff) return false;
					codePoint = 0x10000 +
						((codePoint - 0xd800) << 10) + (low - 0xdc00);
				}
				else if (codePoint >= 0xdc00 && codePoint <= 0xdfff)
				{
					return false;
				}
			}
			if (!AppendUtf8CodePoint(codePoint, staged)) return false;
		}
		destination = std::move(staged);
		return true;
	}

	template <std::size_t Capacity>
	bool AddItemText(XMLWriter& writer, const char* tag,
		const CHAR16 (&source)[Capacity])
	{
		std::string utf8;
		if (!ItemTextToUtf8(source, utf8)) return false;
		writer.addValue(tag, utf8);
		return true;
	}

	template <std::size_t Capacity>
	bool ValidateItemText(const CHAR16 (&source)[Capacity])
	{
		std::string utf8;
		return ItemTextToUtf8(source, utf8);
	}

	bool TryAdjustApBonus(INT16 value, INT16 maximum, bool reverse,
		INT16& adjusted)
	{
		if (maximum <= 0) return false;
		const FLOAT expression = reverse
			? static_cast<FLOAT>(value) / static_cast<FLOAT>(maximum) *
				static_cast<FLOAT>(100) + (value < 0 ? -0.5f : 0.5f)
			: static_cast<FLOAT>(value) * static_cast<FLOAT>(maximum) /
				static_cast<FLOAT>(100) + (value < 0 ? -0.5f : 0.5f);
		if (!std::isfinite(expression)) return false;
		const double truncated = std::trunc(static_cast<double>(expression));
		if (truncated < static_cast<double>(std::numeric_limits<INT16>::min()) ||
			truncated > static_cast<double>(std::numeric_limits<INT16>::max()))
		{
			return false;
		}
		adjusted = static_cast<INT16>(truncated);
		return true;
	}

	bool TrySerializeApBonus(INT16 liveValue, INT16& serialized)
	{
		const INT16 maximum = APBPConstants[AP_MAXIMUM];
		if (maximum <= 0) return false;
		const FLOAT inverse = static_cast<FLOAT>(liveValue) /
			static_cast<FLOAT>(maximum) * static_cast<FLOAT>(100) +
			(liveValue < 0 ? -0.5f : 0.5f);
		if (!std::isfinite(inverse)) return false;
		const double truncated = std::trunc(static_cast<double>(inverse));
		const double bounded = std::max(
			static_cast<double>(std::numeric_limits<INT16>::min()),
			std::min(static_cast<double>(std::numeric_limits<INT16>::max()),
				truncated));
		const int center = static_cast<int>(bounded);
		constexpr int offsets[] = {0, -1, 1, -2, 2};
		for (const int offset : offsets)
		{
			const int candidate = center + offset;
			if (candidate < std::numeric_limits<INT16>::min() ||
				candidate > std::numeric_limits<INT16>::max())
			{
				continue;
			}
			const INT16 encoded = static_cast<INT16>(candidate);
			INT16 roundTripped = 0;
			if (TryAdjustApBonus(
					encoded, maximum, false, roundTripped) &&
				roundTripped == liveValue)
			{
				serialized = encoded;
				return true;
			}
		}
		return false;
	}

	bool ValidateSpreadPattern(INT32 patternIndex)
	{
		if (patternIndex < 0) return false;
		if (patternIndex == 0) return true;
		if (giSpreadPatternCount <= patternIndex) return false;
		const std::string serialized = std::to_string(patternIndex);
		return FindSpreadPatternIndex(
			const_cast<char*>(serialized.c_str())) == patternIndex;
	}

	bool IsSemanticallyEmptyItem(const INVTYPE& item)
	{
		if (item.szItemDesc[0] != L'\0' || item.szBRDesc[0] != L'\0' ||
			item.szItemName[0] != L'\0' || item.szLongItemName[0] != L'\0' ||
			item.szBRName[0] != L'\0')
		{
			return false;
		}
		for (const UINT16 attachment : item.defaultattachments)
			if (attachment != 0) return false;
		for (std::size_t stance = 0; stance < 3; ++stance)
		{
			if (item.flatbasemodifier[stance] != 0 ||
				item.percentbasemodifier[stance] != 0 ||
				item.flataimmodifier[stance] != 0 ||
				item.percentaimmodifier[stance] != 0 ||
				item.percentcapmodifier[stance] != 0 ||
				item.percenthandlingmodifier[stance] != 0 ||
				item.percentdropcompensationmodifier[stance] != 0 ||
				item.maxcounterforcemodifier[stance] != 0 ||
				item.counterforceaccuracymodifier[stance] != 0 ||
				item.targettrackingmodifier[stance] != 0 ||
				item.aimlevelsmodifier[stance] != 0)
			{
				return false;
			}
		}
		const FLOAT zeroFloats[] = {
			item.alcohol, item.RecoilModifierX, item.RecoilModifierY,
			item.scopemagfactor, item.projectionfactor,
			item.usOverheatingCooldownFactor,
			item.overheatTemperatureModificator,
			item.overheatCooldownModificator,
			item.overheatJamThresholdModificator,
			item.overheatDamageThresholdModificator,
			item.dirtIncreaseFactor, item.fRobotDamageReductionModifier};
		for (const FLOAT value : zeroFloats)
		{
			if (value != 0.0f || std::signbit(value)) return false;
		}

#define ITEM_FIELD_MUST_BE_ZERO(field) if (item.field != 0) return false
		ITEM_FIELD_MUST_BE_ZERO(nasAttachmentClass);
		ITEM_FIELD_MUST_BE_ZERO(nasLayoutClass);
		ITEM_FIELD_MUST_BE_ZERO(ulAvailableAttachmentPoint);
		ITEM_FIELD_MUST_BE_ZERO(ulAttachmentPoint);
		ITEM_FIELD_MUST_BE_ZERO(usItemFlag);
		ITEM_FIELD_MUST_BE_ZERO(usItemFlag2);
		ITEM_FIELD_MUST_BE_ZERO(uiIndex);
		ITEM_FIELD_MUST_BE_ZERO(usItemClass);
		ITEM_FIELD_MUST_BE_ZERO(attachmentclass);
		ITEM_FIELD_MUST_BE_ZERO(drugtype);
		ITEM_FIELD_MUST_BE_ZERO(foodtype);
		ITEM_FIELD_MUST_BE_ZERO(usActionItemFlag);
		ITEM_FIELD_MUST_BE_ZERO(clothestype);
		ITEM_FIELD_MUST_BE_ZERO(spreadPattern);
		ITEM_FIELD_MUST_BE_ZERO(alcohol);
		ITEM_FIELD_MUST_BE_ZERO(RecoilModifierX);
		ITEM_FIELD_MUST_BE_ZERO(RecoilModifierY);
		ITEM_FIELD_MUST_BE_ZERO(scopemagfactor);
		ITEM_FIELD_MUST_BE_ZERO(projectionfactor);
		ITEM_FIELD_MUST_BE_ZERO(usOverheatingCooldownFactor);
		ITEM_FIELD_MUST_BE_ZERO(overheatTemperatureModificator);
		ITEM_FIELD_MUST_BE_ZERO(overheatCooldownModificator);
		ITEM_FIELD_MUST_BE_ZERO(overheatJamThresholdModificator);
		ITEM_FIELD_MUST_BE_ZERO(overheatDamageThresholdModificator);
		ITEM_FIELD_MUST_BE_ZERO(dirtIncreaseFactor);
		ITEM_FIELD_MUST_BE_ZERO(fRobotDamageReductionModifier);
		ITEM_FIELD_MUST_BE_ZERO(ubClassIndex);
		ITEM_FIELD_MUST_BE_ZERO(ubGraphicNum);
		ITEM_FIELD_MUST_BE_ZERO(ubWeight);
		ITEM_FIELD_MUST_BE_ZERO(ItemSize);
		ITEM_FIELD_MUST_BE_ZERO(usPrice);
		ITEM_FIELD_MUST_BE_ZERO(discardedlauncheritem);
		ITEM_FIELD_MUST_BE_ZERO(randomitem);
		ITEM_FIELD_MUST_BE_ZERO(usBuddyItem);
		ITEM_FIELD_MUST_BE_ZERO(usRiotShieldStrength);
		ITEM_FIELD_MUST_BE_ZERO(usRiotShieldGraphic);
		ITEM_FIELD_MUST_BE_ZERO(percentnoisereduction);
		ITEM_FIELD_MUST_BE_ZERO(bipod);
		ITEM_FIELD_MUST_BE_ZERO(tohitbonus);
		ITEM_FIELD_MUST_BE_ZERO(bestlaserrange);
		ITEM_FIELD_MUST_BE_ZERO(rangebonus);
		ITEM_FIELD_MUST_BE_ZERO(percentrangebonus);
		ITEM_FIELD_MUST_BE_ZERO(aimbonus);
		ITEM_FIELD_MUST_BE_ZERO(minrangeforaimbonus);
		ITEM_FIELD_MUST_BE_ZERO(percentapreduction);
		ITEM_FIELD_MUST_BE_ZERO(percentstatusdrainreduction);
		ITEM_FIELD_MUST_BE_ZERO(bloodieditem);
		ITEM_FIELD_MUST_BE_ZERO(hearingrangebonus);
		ITEM_FIELD_MUST_BE_ZERO(visionrangebonus);
		ITEM_FIELD_MUST_BE_ZERO(nightvisionrangebonus);
		ITEM_FIELD_MUST_BE_ZERO(dayvisionrangebonus);
		ITEM_FIELD_MUST_BE_ZERO(cavevisionrangebonus);
		ITEM_FIELD_MUST_BE_ZERO(brightlightvisionrangebonus);
		ITEM_FIELD_MUST_BE_ZERO(itemsizebonus);
		ITEM_FIELD_MUST_BE_ZERO(damagebonus);
		ITEM_FIELD_MUST_BE_ZERO(meleedamagebonus);
		ITEM_FIELD_MUST_BE_ZERO(magsizebonus);
		ITEM_FIELD_MUST_BE_ZERO(percentautofireapreduction);
		ITEM_FIELD_MUST_BE_ZERO(autofiretohitbonus);
		ITEM_FIELD_MUST_BE_ZERO(APBonus);
		ITEM_FIELD_MUST_BE_ZERO(rateoffirebonus);
		ITEM_FIELD_MUST_BE_ZERO(burstsizebonus);
		ITEM_FIELD_MUST_BE_ZERO(bursttohitbonus);
		ITEM_FIELD_MUST_BE_ZERO(percentreadytimeapreduction);
		ITEM_FIELD_MUST_BE_ZERO(bulletspeedbonus);
		ITEM_FIELD_MUST_BE_ZERO(percentreloadtimeapreduction);
		ITEM_FIELD_MUST_BE_ZERO(percentburstfireapreduction);
		ITEM_FIELD_MUST_BE_ZERO(camobonus);
		ITEM_FIELD_MUST_BE_ZERO(stealthbonus);
		ITEM_FIELD_MUST_BE_ZERO(urbanCamobonus);
		ITEM_FIELD_MUST_BE_ZERO(desertCamobonus);
		ITEM_FIELD_MUST_BE_ZERO(snowCamobonus);
		ITEM_FIELD_MUST_BE_ZERO(PercentRecoilModifier);
		ITEM_FIELD_MUST_BE_ZERO(percentaccuracymodifier);
		ITEM_FIELD_MUST_BE_ZERO(usSpotting);
		ITEM_FIELD_MUST_BE_ZERO(sBackpackWeightModifier);
		ITEM_FIELD_MUST_BE_ZERO(sFireResistance);
		ITEM_FIELD_MUST_BE_ZERO(ubAttachToPointAPCost);
		ITEM_FIELD_MUST_BE_ZERO(ubCursor);
		ITEM_FIELD_MUST_BE_ZERO(ubGraphicType);
		ITEM_FIELD_MUST_BE_ZERO(ubPerPocket);
		ITEM_FIELD_MUST_BE_ZERO(ubCoolness);
		ITEM_FIELD_MUST_BE_ZERO(percenttunnelvision);
		ITEM_FIELD_MUST_BE_ZERO(ubAttachmentSystem);
		ITEM_FIELD_MUST_BE_ZERO(CrowbarModifier);
		ITEM_FIELD_MUST_BE_ZERO(DisarmModifier);
		ITEM_FIELD_MUST_BE_ZERO(usHackingModifier);
		ITEM_FIELD_MUST_BE_ZERO(usBurialModifier);
		ITEM_FIELD_MUST_BE_ZERO(usDamageChance);
		ITEM_FIELD_MUST_BE_ZERO(usFlashLightRange);
		ITEM_FIELD_MUST_BE_ZERO(usItemChoiceTimeSetting);
		ITEM_FIELD_MUST_BE_ZERO(ubSleepModifier);
		ITEM_FIELD_MUST_BE_ZERO(usPortionSize);
		ITEM_FIELD_MUST_BE_ZERO(usAdministrationModifier);
		ITEM_FIELD_MUST_BE_ZERO(inseparable);
		ITEM_FIELD_MUST_BE_ZERO(bSoundType);
		ITEM_FIELD_MUST_BE_ZERO(bReliability);
		ITEM_FIELD_MUST_BE_ZERO(bRepairEase);
		ITEM_FIELD_MUST_BE_ZERO(LockPickModifier);
		ITEM_FIELD_MUST_BE_ZERO(RepairModifier);
		ITEM_FIELD_MUST_BE_ZERO(randomitemcoolnessmodificator);
		ITEM_FIELD_MUST_BE_ZERO(bRobotStrBonus);
		ITEM_FIELD_MUST_BE_ZERO(bRobotAgiBonus);
		ITEM_FIELD_MUST_BE_ZERO(bRobotDexBonus);
		ITEM_FIELD_MUST_BE_ZERO(bRobotTargetingSkillGrant);
		ITEM_FIELD_MUST_BE_ZERO(bRobotChassisSkillGrant);
		ITEM_FIELD_MUST_BE_ZERO(bRobotUtilitySkillGrant);
		ITEM_FIELD_MUST_BE_ZERO(iTransportGroupMinProgress);
		ITEM_FIELD_MUST_BE_ZERO(iTransportGroupMaxProgress);
#undef ITEM_FIELD_MUST_BE_ZERO
		return true;
	}

	bool ValidateItemRecord(std::size_t index)
	{
		const INVTYPE& item = Item[index];
		if (!ValidateItemText(item.szItemName) ||
			!ValidateItemText(item.szLongItemName) ||
			!ValidateItemText(item.szItemDesc) ||
			!ValidateItemText(item.szBRName) ||
			!ValidateItemText(item.szBRDesc))
		{
			return false;
		}

		constexpr UINT64 allowedItemFlags = UINT64_C(0xCFFFFFFFFFFFFFFF);
		constexpr UINT64 allowedItemFlags2 = UINT64_C(0x000007FFFFFFFFFF);
		if ((item.usItemFlag & ~allowedItemFlags) != 0 ||
			(item.usItemFlag2 & ~allowedItemFlags2) != 0)
		{
			return false;
		}
		if (item.usItemClass == 0)
		{
			if (!IsSemanticallyEmptyItem(item)) return false;
		}
		else if (item.uiIndex != index)
		{
			return false;
		}

		bool sawZeroAttachment = false;
		for (const UINT16 attachment : item.defaultattachments)
		{
			if (attachment == 0) sawZeroAttachment = true;
			else if (sawZeroAttachment) return false;
		}

		for (std::size_t stance = 0; stance < 3; ++stance)
		{
			if (item.flatbasemodifier[stance] == -10000 ||
				item.percentbasemodifier[stance] == -10000 ||
				item.flataimmodifier[stance] == -10000 ||
				item.percentaimmodifier[stance] == -10000 ||
				item.percentcapmodifier[stance] == -10000 ||
				item.percenthandlingmodifier[stance] == -10000 ||
				item.percentdropcompensationmodifier[stance] == -10000 ||
				item.maxcounterforcemodifier[stance] == -10000 ||
				item.counterforceaccuracymodifier[stance] == -10000 ||
				item.targettrackingmodifier[stance] == -10000 ||
				item.aimlevelsmodifier[stance] == -10000)
			{
				return false;
			}
		}

		const FLOAT finiteValues[] = {
			item.alcohol, item.scopemagfactor, item.projectionfactor,
			item.RecoilModifierX, item.RecoilModifierY,
			item.usOverheatingCooldownFactor,
			item.overheatTemperatureModificator,
			item.overheatCooldownModificator,
			item.overheatJamThresholdModificator,
			item.overheatDamageThresholdModificator,
			item.dirtIncreaseFactor, item.fRobotDamageReductionModifier};
		for (const FLOAT value : finiteValues)
			if (!std::isfinite(value)) return false;

		INT16 serializedApBonus = 0;
		return item.alcohol >= 0.0f && !std::signbit(item.alcohol) &&
			item.drugtype < NEW_DRUGS_MAX &&
			item.usHackingModifier <= 100 && item.usBurialModifier <= 100 &&
			item.usSpotting >= 0 && item.usSpotting <= 100 &&
			item.usRiotShieldStrength <= 100 &&
			item.randomitemcoolnessmodificator >= -20 &&
			item.randomitemcoolnessmodificator <= 20 &&
			item.usItemChoiceTimeSetting <= 2 && item.sFireResistance <= 100 &&
			TrySerializeApBonus(item.APBonus, serializedApBonus) &&
			ValidateSpreadPattern(item.spreadPattern);
	}

	bool AddSpreadPattern(XMLWriter& writer, INT32 patternIndex)
	{
		if (!ValidateSpreadPattern(patternIndex)) return false;
		writer.addValue("spreadPattern", std::to_string(patternIndex));
		return true;
	}

	void AddStanceModifiers(
		XMLWriter& writer, const INVTYPE& item, const char* node,
		std::size_t stance)
	{
		writer.openNode(node);
		AddItemInteger(writer, "FlatBase", item.flatbasemodifier[stance]);
		AddItemInteger(writer, "PercentBase", item.percentbasemodifier[stance]);
		AddItemInteger(writer, "FlatAim", item.flataimmodifier[stance]);
		AddItemInteger(writer, "PercentAim", item.percentaimmodifier[stance]);
		AddItemInteger(writer, "PercentCap", item.percentcapmodifier[stance]);
		AddItemInteger(
			writer, "PercentHandling", item.percenthandlingmodifier[stance]);
		AddItemInteger(writer, "PercentTargetTrackingSpeed",
			item.targettrackingmodifier[stance]);
		AddItemInteger(writer, "PercentDropCompensation",
			item.percentdropcompensationmodifier[stance]);
		AddItemInteger(writer, "PercentMaxCounterForce",
			item.maxcounterforcemodifier[stance]);
		AddItemInteger(writer, "PercentCounterForceAccuracy",
			item.counterforceaccuracymodifier[stance]);
		AddItemInteger(writer, "AimLevels", item.aimlevelsmodifier[stance]);
	}

	bool AddItemRecord(XMLWriter& writer, std::size_t index)
	{
		const INVTYPE& item = Item[index];
		writer.openNode("ITEM");
		AddItemInteger(writer, "uiIndex", index);
		if (!AddItemText(writer, "szItemName", item.szItemName) ||
			!AddItemText(writer, "szLongItemName", item.szLongItemName) ||
			!AddItemText(writer, "szItemDesc", item.szItemDesc) ||
			!AddItemText(writer, "szBRName", item.szBRName) ||
			!AddItemText(writer, "szBRDesc", item.szBRDesc))
		{
			return false;
		}

		AddItemInteger(writer, "usItemClass", item.usItemClass);
		AddItemInteger(writer, "nasAttachmentClass", item.nasAttachmentClass);
		AddItemInteger(writer, "nasLayoutClass", item.nasLayoutClass);
		AddItemInteger(writer, "AvailableAttachmentPoint",
			item.ulAvailableAttachmentPoint);
		AddItemInteger(writer, "AttachmentPoint", item.ulAttachmentPoint);
		AddItemInteger(writer, "AttachToPointAPCost",
			item.ubAttachToPointAPCost);
		AddItemInteger(writer, "ubClassIndex", item.ubClassIndex);
		AddItemInteger(writer, "ubCursor", item.ubCursor);
		AddItemInteger(writer, "bSoundType", item.bSoundType);
		AddItemInteger(writer, "ubGraphicType", item.ubGraphicType);
		AddItemInteger(writer, "ubGraphicNum", item.ubGraphicNum);
		AddItemInteger(writer, "ubWeight", item.ubWeight);
		AddItemInteger(writer, "ubPerPocket", item.ubPerPocket);
		AddItemInteger(writer, "ItemSize", item.ItemSize);
		AddItemInteger(writer, "usPrice", item.usPrice);
		AddItemInteger(writer, "ubCoolness", item.ubCoolness);
		AddItemInteger(writer, "bReliability", item.bReliability);
		AddItemInteger(writer, "bRepairEase", item.bRepairEase);
		AddItemInteger(writer, "AttachmentSystem", item.ubAttachmentSystem);
		AddItemInteger(writer, "Inseparable", item.inseparable);

		AddItemInteger(writer, "BR_NewInventory",
			StoreInventory[index][BOBBY_RAY_NEW]);
		AddItemInteger(writer, "BR_UsedInventory",
			StoreInventory[index][BOBBY_RAY_USED]);
		AddItemInteger(writer, "BR_ROF", WeaponROF[index]);

		AddItemInteger(writer, "PercentNoiseReduction",
			item.percentnoisereduction);
		AddItemInteger(writer, "Bipod", item.bipod);
		AddItemInteger(writer, "RangeBonus", item.rangebonus);
		AddItemInteger(writer, "PercentRangeBonus", item.percentrangebonus);
		AddItemInteger(writer, "ToHitBonus", item.tohitbonus);
		AddItemInteger(writer, "AimBonus", item.aimbonus);
		AddItemInteger(writer, "MinRangeForAimBonus",
			item.minrangeforaimbonus);
		AddItemInteger(writer, "MagSizeBonus", item.magsizebonus);
		AddItemInteger(writer, "RateOfFireBonus", item.rateoffirebonus);
		AddItemInteger(writer, "BulletSpeedBonus", item.bulletspeedbonus);
		AddItemInteger(writer, "BurstSizeBonus", item.burstsizebonus);
		AddItemInteger(writer, "BestLaserRange", item.bestlaserrange);
		AddItemInteger(writer, "BurstToHitBonus", item.bursttohitbonus);
		AddItemInteger(writer, "AutoFireToHitBonus",
			item.autofiretohitbonus);
		INT16 serializedApBonus = 0;
		if (!TrySerializeApBonus(item.APBonus, serializedApBonus)) return false;
		AddItemInteger(writer, "APBonus", serializedApBonus);
		AddItemInteger(writer, "PercentBurstFireAPReduction",
			item.percentburstfireapreduction);
		AddItemInteger(writer, "PercentAutofireAPReduction",
			item.percentautofireapreduction);
		AddItemInteger(writer, "PercentReadyTimeAPReduction",
			item.percentreadytimeapreduction);
		AddItemInteger(writer, "PercentReloadTimeAPReduction",
			item.percentreloadtimeapreduction);
		AddItemInteger(writer, "PercentAPReduction", item.percentapreduction);
		AddItemInteger(writer, "PercentStatusDrainReduction",
			item.percentstatusdrainreduction);
		AddItemInteger(writer, "DamageBonus", item.damagebonus);
		AddItemInteger(writer, "MeleeDamageBonus", item.meleedamagebonus);
		AddItemInteger(writer, "DiscardedLauncherItem",
			item.discardedlauncheritem);
		for (const UINT16 attachment : item.defaultattachments)
		{
			if (attachment != 0)
				AddItemInteger(writer, "DefaultAttachment", attachment);
		}
		AddItemInteger(writer, "BloodiedItem", item.bloodieditem);
		AddItemInteger(writer, "CamoBonus", item.camobonus);
		AddItemInteger(writer, "UrbanCamoBonus", item.urbanCamobonus);
		AddItemInteger(writer, "DesertCamoBonus", item.desertCamobonus);
		AddItemInteger(writer, "SnowCamoBonus", item.snowCamobonus);
		AddItemInteger(writer, "StealthBonus", item.stealthbonus);
		AddItemInteger(writer, "HearingRangeBonus", item.hearingrangebonus);
		AddItemInteger(writer, "VisionRangeBonus", item.visionrangebonus);
		AddItemInteger(writer, "NightVisionRangeBonus",
			item.nightvisionrangebonus);
		AddItemInteger(writer, "DayVisionRangeBonus",
			item.dayvisionrangebonus);
		AddItemInteger(writer, "CaveVisionRangeBonus",
			item.cavevisionrangebonus);
		AddItemInteger(writer, "BrightLightVisionRangeBonus",
			item.brightlightvisionrangebonus);
		AddItemInteger(writer, "ItemSizeBonus", item.itemsizebonus);
		AddItemInteger(writer, "PercentTunnelVision",
			item.percenttunnelvision);

		if (!AddItemFloat(writer, "Alcohol", item.alcohol) ||
			!AddItemFloat(writer, "ScopeMagFactor", item.scopemagfactor) ||
			!AddItemFloat(writer, "ProjectionFactor", item.projectionfactor) ||
			!AddItemFloat(writer, "RecoilModifierX", item.RecoilModifierX) ||
			!AddItemFloat(writer, "RecoilModifierY", item.RecoilModifierY))
		{
			return false;
		}
		AddItemInteger(writer, "PercentAccuracyModifier",
			item.percentaccuracymodifier);
		AddItemInteger(writer, "PercentRecoilModifier",
			item.PercentRecoilModifier);
		if (!AddSpreadPattern(writer, item.spreadPattern)) return false;

		AddStanceModifiers(writer, item, "STAND_MODIFIERS", 0);
		if (!writer.closeNode()) return false;
		AddStanceModifiers(writer, item, "CROUCH_MODIFIERS", 1);
		if (!writer.closeNode()) return false;
		AddStanceModifiers(writer, item, "PRONE_MODIFIERS", 2);
		if (!writer.closeNode()) return false;

		if (!AddItemFloat(writer, "usOverheatingCooldownFactor",
				item.usOverheatingCooldownFactor) ||
			!AddItemFloat(writer, "overheatTemperatureModificator",
				item.overheatTemperatureModificator) ||
			!AddItemFloat(writer, "overheatCooldownModificator",
				item.overheatCooldownModificator) ||
			!AddItemFloat(writer, "overheatJamThresholdModificator",
				item.overheatJamThresholdModificator) ||
			!AddItemFloat(writer, "overheatDamageThresholdModificator",
				item.overheatDamageThresholdModificator))
		{
			return false;
		}

		AddItemInteger(writer, "AttachmentClass", item.attachmentclass);
		AddItemInteger(writer, "DrugType", item.drugtype);
		AddItemInteger(writer, "FoodType", item.foodtype);
		AddItemInteger(writer, "LockPickModifier", item.LockPickModifier);
		AddItemInteger(writer, "CrowbarModifier", item.CrowbarModifier);
		AddItemInteger(writer, "DisarmModifier", item.DisarmModifier);
		AddItemInteger(writer, "RepairModifier", item.RepairModifier);
		AddItemInteger(writer, "usHackingModifier", item.usHackingModifier);
		AddItemInteger(writer, "usBurialModifier", item.usBurialModifier);
		AddItemInteger(writer, "DamageChance", item.usDamageChance);
		if (!AddItemFloat(
				writer, "DirtIncreaseFactor", item.dirtIncreaseFactor))
		{
			return false;
		}
		AddItemInteger(writer, "usActionItemFlag", item.usActionItemFlag);
		AddItemInteger(writer, "clothestype", item.clothestype);
		AddItemInteger(writer, "randomitem", item.randomitem);
		AddItemInteger(writer, "randomitemcoolnessmodificator",
			item.randomitemcoolnessmodificator);
		AddItemInteger(writer, "FlashLightRange", item.usFlashLightRange);
		AddItemInteger(writer, "ItemChoiceTimeSetting",
			item.usItemChoiceTimeSetting);
		AddItemInteger(writer, "buddyitem", item.usBuddyItem);
		AddItemInteger(writer, "SleepModifier", item.ubSleepModifier);
		AddItemInteger(writer, "usSpotting", item.usSpotting);
		AddItemInteger(writer, "sBackpackWeightModifier",
			item.sBackpackWeightModifier);
		AddItemInteger(writer, "usPortionSize", item.usPortionSize);
		AddItemInteger(writer, "usRiotShieldStrength",
			item.usRiotShieldStrength);
		AddItemInteger(writer, "usRiotShieldGraphic",
			item.usRiotShieldGraphic);
		AddItemInteger(writer, "sFireResistance", item.sFireResistance);
		AddItemInteger(writer, "usAdministrationModifier",
			item.usAdministrationModifier);
		if (!AddItemFloat(writer, "RobotDamageReduction",
				item.fRobotDamageReductionModifier))
		{
			return false;
		}
		AddItemInteger(writer, "RobotStrBonus", item.bRobotStrBonus);
		AddItemInteger(writer, "RobotAgiBonus", item.bRobotAgiBonus);
		AddItemInteger(writer, "RobotDexBonus", item.bRobotDexBonus);
		AddItemInteger(writer, "RobotTargetingSkillGrant",
			item.bRobotTargetingSkillGrant);
		AddItemInteger(writer, "RobotChassisSkillGrant",
			item.bRobotChassisSkillGrant);
		AddItemInteger(writer, "RobotUtilitySkillGrant",
			item.bRobotUtilitySkillGrant);
		AddItemInteger(writer, "TransportGroupMinProgress",
			item.iTransportGroupMinProgress);
		AddItemInteger(writer, "TransportGroupMaxProgress",
			item.iTransportGroupMaxProgress);

		const UINT16 itemIndex = static_cast<UINT16>(index);
		AddItemBoolean(writer, "Bloodbag", HasItemFlag(itemIndex, BLOOD_BAG));
		AddItemBoolean(writer, "Manpad", HasItemFlag(itemIndex, MANPAD));
		AddItemBoolean(writer, "Beartrap", HasItemFlag(itemIndex, BEARTRAP));
		AddItemBoolean(writer, "Camera", HasItemFlag(itemIndex, CAMERA));
		AddItemBoolean(writer, "Waterdrum", HasItemFlag(itemIndex, WATER_DRUM));
		AddItemBoolean(writer, "BloodcatMeat",
			HasItemFlag(itemIndex, MEAT_BLOODCAT));
		AddItemBoolean(writer, "CowMeat", HasItemFlag(itemIndex, MEAT_COW));
		AddItemBoolean(writer, "Beltfed", HasItemFlag(itemIndex, BELT_FED));
		AddItemBoolean(writer, "Ammobelt", HasItemFlag(itemIndex, AMMO_BELT));
		AddItemBoolean(writer, "AmmobeltVest",
			HasItemFlag(itemIndex, AMMO_BELT_VEST));
		AddItemBoolean(writer, "CamoRemoval",
			HasItemFlag(itemIndex, CAMO_REMOVAL));
		AddItemBoolean(writer, "Cleaningkit",
			HasItemFlag(itemIndex, CLEANING_KIT));
		AddItemBoolean(writer, "AttentionItem",
			HasItemFlag(itemIndex, ATTENTION_ITEM));
		AddItemBoolean(writer, "Garotte", HasItemFlag(itemIndex, GAROTTE));
		AddItemBoolean(writer, "Covert", HasItemFlag(itemIndex, COVERT));
		AddItemBoolean(writer, "Corpse", HasItemFlag(itemIndex, CORPSE));
		AddItemBoolean(writer, "BloodcatSkin",
			HasItemFlag(itemIndex, SKIN_BLOODCAT));
		AddItemBoolean(writer, "NoMetalDetection",
			HasItemFlag(itemIndex, NO_METAL_DETECTION));
		AddItemBoolean(writer, "JumpGrenade",
			HasItemFlag(itemIndex, JUMP_GRENADE));
		AddItemBoolean(writer, "Handcuffs",
			HasItemFlag(itemIndex, HANDCUFFS));
		AddItemBoolean(writer, "Taser", HasItemFlag(itemIndex, TASER));
		AddItemBoolean(writer, "ScubaBottle",
			HasItemFlag(itemIndex, SCUBA_BOTTLE));
		AddItemBoolean(writer, "ScubaMask",
			HasItemFlag(itemIndex, SCUBA_MASK));
		AddItemBoolean(writer, "ScubaFins",
			HasItemFlag(itemIndex, SCUBA_FINS));
		AddItemBoolean(writer, "TripwireRoll",
			HasItemFlag(itemIndex, TRIPWIREROLL));
		AddItemBoolean(writer, "Radioset",
			HasItemFlag(itemIndex, RADIO_SET));
		AddItemBoolean(writer, "SignalShell",
			HasItemFlag(itemIndex, SIGNAL_SHELL));
		AddItemBoolean(writer, "Soda", HasItemFlag(itemIndex, SODA));
		AddItemBoolean(writer, "RoofcollapseItem",
			HasItemFlag(itemIndex, ROOF_COLLAPSE_ITEM));
		AddItemBoolean(writer, "DiseaseprotectionFace",
			HasItemFlag(itemIndex, DISEASEPROTECTION_1));
		AddItemBoolean(writer, "DiseaseprotectionHand",
			HasItemFlag(itemIndex, DISEASEPROTECTION_2));
		AddItemBoolean(writer, "LBEexplosionproof",
			HasItemFlag(itemIndex, LBE_EXPLOSIONPROOF));
		AddItemBoolean(writer, "EmptyBloodbag",
			HasItemFlag(itemIndex, EMPTY_BLOOD_BAG));
		AddItemBoolean(writer, "MedicalSplint",
			HasItemFlag(itemIndex, MEDICAL_SPLINT));
		AddItemBoolean(writer, "Damageable", ItemIsDamageable(itemIndex));
		AddItemBoolean(writer, "Repairable", ItemIsRepairable(itemIndex));
		AddItemBoolean(writer, "WaterDamages",
			ItemIsDamagedByWater(itemIndex));
		AddItemBoolean(writer, "Metal", ItemIsMetal(itemIndex));
		AddItemBoolean(writer, "Sinks", ItemSinks(itemIndex));
		AddItemBoolean(writer, "ShowStatus",
			HasItemFlag(itemIndex, ITEM_showstatus));
		AddItemBoolean(writer, "HiddenAddon", ItemIsHiddenAddon(itemIndex));
		AddItemBoolean(writer, "TwoHanded", ItemIsTwoHanded(itemIndex));
		AddItemBoolean(writer, "NotBuyable", ItemIsNotBuyable(itemIndex));
		AddItemBoolean(writer, "Attachment", ItemIsAttachment(itemIndex));
		AddItemBoolean(writer, "HiddenAttachment",
			ItemIsHiddenAttachment(itemIndex));
		AddItemBoolean(writer, "BigGunList",
			ItemIsOnlyInTonsOfGuns(itemIndex));
		AddItemBoolean(writer, "NotInEditor", ItemIsNotInEditor(itemIndex));
		AddItemBoolean(writer, "DefaultUndroppable",
			ItemIsUndroppableByDefault(itemIndex));
		AddItemBoolean(writer, "Unaerodynamic",
			ItemIsUnaerodynamic(itemIndex));
		AddItemBoolean(writer, "Electronic", ItemIsElectronic(itemIndex));
		AddItemBoolean(writer, "Cannon", ItemIsCannon(itemIndex));
		AddItemBoolean(writer, "RocketRifle", ItemIsRocketRifle(itemIndex));
		AddItemBoolean(writer, "FingerPrintID",
			ItemHasFingerPrintID(itemIndex));
		AddItemBoolean(writer, "MetalDetector",
			ItemIsMetalDetector(itemIndex));
		AddItemBoolean(writer, "GasMask", ItemIsGasmask(itemIndex));
		AddItemBoolean(writer, "LockBomb", ItemIsLockBomb(itemIndex));
		AddItemBoolean(writer, "Flare", ItemIsFlare(itemIndex));
		AddItemBoolean(writer, "GrenadeLauncher",
			ItemIsGrenadeLauncher(itemIndex));
		AddItemBoolean(writer, "Mortar", ItemIsMortar(itemIndex));
		AddItemBoolean(writer, "Duckbill", ItemIsDuckbill(itemIndex));
		AddItemBoolean(writer, "HideMuzzleFlash",
			ItemHasHiddenMuzzleFlash(itemIndex));
		AddItemBoolean(writer, "RocketLauncher",
			ItemIsRocketLauncher(itemIndex));
		AddItemBoolean(writer, "SingleShotRocketLauncher",
			ItemIsSingleShotRocketLauncher(itemIndex));
		AddItemBoolean(writer, "BrassKnuckles",
			ItemIsBrassKnuckles(itemIndex));
		AddItemBoolean(writer, "Crowbar", ItemIsCrowbar(itemIndex));
		AddItemBoolean(writer, "GLGrenade", ItemIsGLgrenade(itemIndex));
		AddItemBoolean(writer, "FlakJacket", ItemIsFlakJacket(itemIndex));
		AddItemBoolean(writer, "LeatherJacket",
			ItemIsLeatherJacket(itemIndex));
		AddItemBoolean(writer, "Batteries", ItemIsBatteries(itemIndex));
		AddItemBoolean(writer, "NeedsBatteries",
			ItemNeedsBatteries(itemIndex));
		AddItemBoolean(writer, "XRay", ItemHasXRay(itemIndex));
		AddItemBoolean(writer, "WireCutters", ItemIsWirecutters(itemIndex));
		AddItemBoolean(writer, "Toolkit", ItemIsToolkit(itemIndex));
		AddItemBoolean(writer, "FirstAidKit", ItemIsFirstAidKit(itemIndex));
		AddItemBoolean(writer, "MedicalKit", ItemIsMedicalKit(itemIndex));
		AddItemBoolean(writer, "Canteen", ItemIsCanteen(itemIndex));
		AddItemBoolean(writer, "Jar", ItemIsJar(itemIndex));
		AddItemBoolean(writer, "CanAndString",
			ItemIsCanAndString(itemIndex));
		AddItemBoolean(writer, "Marbles", ItemIsMarbles(itemIndex));
		AddItemBoolean(writer, "Walkman", ItemIsWalkman(itemIndex));
		AddItemBoolean(writer, "RemoteTrigger",
			ItemIsRemoteTrigger(itemIndex));
		AddItemBoolean(writer, "RobotRemoteControl",
			ItemIsRobotRemote(itemIndex));
		AddItemBoolean(writer, "CamouflageKit", ItemIsCamoKit(itemIndex));
		AddItemBoolean(writer, "LocksmithKit",
			ItemIsLocksmithKit(itemIndex));
		AddItemBoolean(writer, "Mine", ItemIsMine(itemIndex));
		AddItemBoolean(writer, "AntitankMine", ItemIsATMine(itemIndex));
		AddItemBoolean(writer, "Hardware", ItemIsHardware(itemIndex));
		AddItemBoolean(writer, "Medical", ItemIsMedical(itemIndex));
		AddItemBoolean(writer, "GasCan", ItemIsGascan(itemIndex));
		AddItemBoolean(writer, "ContainsLiquid",
			ItemContainsLiquid(itemIndex));
		AddItemBoolean(writer, "Rock", ItemIsRock(itemIndex));
		AddItemBoolean(writer, "ThermalOptics",
			ItemIsThermalOptics(itemIndex));
		AddItemBoolean(writer, "SciFi", ItemIsOnlyInScifi(itemIndex));
		AddItemBoolean(writer, "NewInv", ItemIsOnlyInNIV(itemIndex));
		AddItemBoolean(writer, "DiseaseSystemExclusive",
			ItemIsOnlyInDisease(itemIndex));
		AddItemBoolean(writer, "Barrel", ItemIsBarrel(itemIndex));
		AddItemBoolean(writer, "TripWireActivation",
			ItemHasTripwireActivation(itemIndex));
		AddItemBoolean(writer, "TripWire", ItemIsTripwire(itemIndex));
		AddItemBoolean(writer, "Directional",
			ItemIsDirectional(itemIndex));
		AddItemBoolean(writer, "BlockIronSight",
			ItemBlocksIronsight(itemIndex));
		AddItemBoolean(writer, "AllowClimbing",
			ItemAllowsClimbing(itemIndex));
		AddItemBoolean(writer, "Cigarette", ItemIsCigarette(itemIndex));
		AddItemBoolean(writer, "ProvidesRobotCamo",
			ItemProvidesRobotCamo(itemIndex));
		AddItemBoolean(writer, "ProvidesRobotNightVision",
			ItemProvidesRobotNightvision(itemIndex));
		AddItemBoolean(writer, "ProvidesRobotLaserBonus",
			ItemProvidesRobotLaserBonus(itemIndex));

		return writer.closeNode();
	}

	bool BuildItemStatsXml(XMLWriter& writer, std::size_t requestedCount)
	{
		static_assert(static_cast<std::uintmax_t>(MAXITEMS) <=
			std::numeric_limits<UINT16>::max(),
			"item writer predicates require a representable item index");
		const std::size_t itemCount = ItemXmlWriter::BoundedItemCount(
			requestedCount, static_cast<std::size_t>(MAXITEMS));
		for (std::size_t index = 0; index < itemCount; ++index)
		{
			if (!ValidateItemRecord(index)) return false;
		}
		// The reader derives gMAXITEMS_READ from the highest nonzero-class item,
		// not from auxiliary-only gap records.  A trailing gap would silently
		// lower the requested live-table high-water mark after reload.
		if (itemCount != 0 && Item[itemCount - 1].usItemClass == 0) return false;
		writer.openNode("ITEMLIST");
		for (std::size_t index = 0; index < itemCount; ++index)
		{
			if (!AddItemRecord(writer, index)) return false;
		}
		return writer.closeNode();
	}
}

namespace ItemXmlWriter
{
	bool Write(const vfs::Path& path, std::size_t requestedCount)
	{
		try
		{
			XMLWriter writer;
			return BuildItemStatsXml(writer, requestedCount) &&
				writer.writeToFile(path);
		}
		catch (...)
		{
			return false;
		}
	}

	bool Write(vfs::tWritableFile* file, std::size_t requestedCount)
	{
		if (!file) return false;
		try
		{
			XMLWriter writer;
			return BuildItemStatsXml(writer, requestedCount) &&
				writer.writeToFile(file);
		}
		catch (...)
		{
			return false;
		}
	}
}

BOOLEAN WriteItemStatsToFile(STR fileName, UINT32 itemCount)
{
	if (!fileName) return FALSE;
	try
	{
		return ItemXmlWriter::Write(
			vfs::Path(fileName), static_cast<std::size_t>(itemCount))
			? TRUE : FALSE;
	}
	catch (...)
	{
		return FALSE;
	}
}

BOOLEAN WriteItemStats()
{
	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "writeitemstats");
	const std::size_t itemCount = std::min<std::size_t>(
		gMAXITEMS_READ, static_cast<std::size_t>(MAXITEMS));
	return ItemXmlWriter::Write(
		vfs::Path("TABLEDATA\\Items out.xml"), itemCount) ? TRUE : FALSE;
}
// HEADROCK HAM 4: This function runs just before the items are written into the item array. It causes all stance bonuses
// to inherit their properties from the bonus "above" them, as long as they don't already have their own value defined.
void InheritStanceModifiers( itemParseData *pData )
{
	ItemDataStagingModel::StanceModifierMatrix modifiers;
	for (std::size_t stance = 0;
		stance < ItemDataStagingModel::StanceCount; ++stance)
	{
		modifiers[0][stance] = pData->curItem.flatbasemodifier[stance];
		modifiers[1][stance] = pData->curItem.percentbasemodifier[stance];
		modifiers[2][stance] = pData->curItem.flataimmodifier[stance];
		modifiers[3][stance] = pData->curItem.percentaimmodifier[stance];
		modifiers[4][stance] = pData->curItem.percentcapmodifier[stance];
		modifiers[5][stance] = pData->curItem.percenthandlingmodifier[stance];
		modifiers[6][stance] = pData->curItem.targettrackingmodifier[stance];
		modifiers[7][stance] =
			pData->curItem.percentdropcompensationmodifier[stance];
		modifiers[8][stance] = pData->curItem.maxcounterforcemodifier[stance];
		modifiers[9][stance] =
			pData->curItem.counterforceaccuracymodifier[stance];
		modifiers[10][stance] = pData->curItem.aimlevelsmodifier[stance];
	}

	ItemDataStagingModel::ResolveStanceInheritance(modifiers);

	for (std::size_t stance = 0;
		stance < ItemDataStagingModel::StanceCount; ++stance)
	{
		pData->curItem.flatbasemodifier[stance] = modifiers[0][stance];
		pData->curItem.percentbasemodifier[stance] = modifiers[1][stance];
		pData->curItem.flataimmodifier[stance] = modifiers[2][stance];
		pData->curItem.percentaimmodifier[stance] = modifiers[3][stance];
		pData->curItem.percentcapmodifier[stance] = modifiers[4][stance];
		pData->curItem.percenthandlingmodifier[stance] = modifiers[5][stance];
		pData->curItem.targettrackingmodifier[stance] = modifiers[6][stance];
		pData->curItem.percentdropcompensationmodifier[stance] =
			modifiers[7][stance];
		pData->curItem.maxcounterforcemodifier[stance] = modifiers[8][stance];
		pData->curItem.counterforceaccuracymodifier[stance] =
			modifiers[9][stance];
		pData->curItem.aimlevelsmodifier[stance] = modifiers[10][stance];
	}
}
