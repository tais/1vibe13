#include "Soldier Profile.h"

#include <string.h>

MERCPROFILEGEAR::MERCPROFILEGEAR()
{
	clearInventory();
	initialize();
}

MERCPROFILEGEAR& MERCPROFILEGEAR::operator=(const MERCPROFILEGEAR& src)
{
	if (this != &src)
	{
		memcpy((void*)this, &src, SIZEOF_MERCPROFILEGEAR_POD);
		inv = src.inv;
		iStatus = src.iStatus;
		iDrop = src.iDrop;
		iNumber = src.iNumber;
		lbe = src.lbe;
		lStatus = src.lStatus;
		invCnt = src.invCnt;
		lbeCnt = src.lbeCnt;
		PriceModifier = src.PriceModifier;
		AbsolutePrice = src.AbsolutePrice;
	}
	return *this;
}

MERCPROFILEGEAR::MERCPROFILEGEAR(const MERCPROFILEGEAR& src)
{
	if (this != &src)
	{
		memcpy((void*)this, &src, SIZEOF_MERCPROFILEGEAR_POD);
		inv = src.inv;
		iStatus = src.iStatus;
		iDrop = src.iDrop;
		iNumber = src.iNumber;
		lbe = src.lbe;
		lStatus = src.lStatus;
		invCnt = src.invCnt;
		lbeCnt = src.lbeCnt;
		PriceModifier = src.PriceModifier;
		AbsolutePrice = src.AbsolutePrice;
	}
}

MERCPROFILEGEAR::~MERCPROFILEGEAR()
{
}

void MERCPROFILEGEAR::initialize()
{
	memset((void*)this, 0, SIZEOF_MERCPROFILEGEAR_POD);
	clearInventory();
}

void MERCPROFILEGEAR::clearInventory()
{
	invCnt = 55;
	lbeCnt = 5;
	PriceModifier = 0;

	inv.clear();
	iStatus.clear();
	iDrop.clear();
	iNumber.clear();

	inv.resize(invCnt);
	iStatus.resize(invCnt);
	iDrop.resize(invCnt);
	iNumber.resize(invCnt);

	lbe.clear();
	lStatus.clear();

	lbe.resize(lbeCnt);
	lStatus.resize(lbeCnt);
}

UINT32 MERCPROFILESTRUCT::GetChecksum()
{
	UINT32 uiChecksum = 1;

	uiChecksum += (bLife + 1);
	uiChecksum *= (bLifeMax + 1);
	uiChecksum += (bAgility + 1);
	uiChecksum *= (bDexterity + 1);
	uiChecksum += (bStrength + 1);
	uiChecksum *= (bMarksmanship + 1);
	uiChecksum += (bMedical + 1);
	uiChecksum *= (bMechanical + 1);
	uiChecksum += (bExplosive + 1);
	uiChecksum *= (bExpLevel + 1);

	for (UINT32 index = 0; index < inv.size(); ++index)
	{
		uiChecksum += inv[index];
		uiChecksum += bInvNumber[index];
	}

	return uiChecksum;
}

OLD_MERCPROFILESTRUCT_101::OLD_MERCPROFILESTRUCT_101()
{
	memset((void*)this, 0, SIZEOF_OLD_MERCPROFILESTRUCT_101_POD);
}

MERCPROFILESTRUCT::MERCPROFILESTRUCT()
{
	initialize();
}

MERCPROFILESTRUCT::MERCPROFILESTRUCT(const MERCPROFILESTRUCT& src)
{
	memcpy((void*)this, &src, SIZEOF_MERCPROFILESTRUCT_POD);
	inv = src.inv;
	bInvStatus = src.bInvStatus;
	bInvNumber = src.bInvNumber;
}

MERCPROFILESTRUCT& MERCPROFILESTRUCT::operator=(const MERCPROFILESTRUCT& src)
{
	if (this != &src)
	{
		memcpy((void*)this, &src, SIZEOF_MERCPROFILESTRUCT_POD);
		inv = src.inv;
		bInvStatus = src.bInvStatus;
		bInvNumber = src.bInvNumber;
	}
	return *this;
}

MERCPROFILESTRUCT::~MERCPROFILESTRUCT()
{
}

void MERCPROFILESTRUCT::initialize()
{
	memset((void*)this, 0, SIZEOF_MERCPROFILESTRUCT_POD);
	clearInventory();
	memset(&records, 0, sizeof(STRUCT_Records));
	memset(&usBackground, 0, sizeof(UINT16));
	memset(&usDynamicOpinionFlagmask, 0, sizeof(usDynamicOpinionFlagmask));
	memset(&sDynamicOpinionLongTerm, 0, sizeof(sDynamicOpinionLongTerm));
	memset(&usVoiceIndex, 0, sizeof(UINT32));
	memset(&Type, 0, sizeof(UINT32));
}

void MERCPROFILESTRUCT::clearInventory()
{
	inv.clear();
	bInvStatus.clear();
	bInvNumber.clear();

	inv.resize(NUM_INV_SLOTS);
	bInvStatus.resize(NUM_INV_SLOTS);
	bInvNumber.resize(NUM_INV_SLOTS);
}

void MERCPROFILESTRUCT::CopyOldInventoryToNew(const OLD_MERCPROFILESTRUCT_101& src)
{
	inv[HELMETPOS] = src.DO_NOT_USE_inv[OldInventory::HELMETPOS];
	inv[VESTPOS] = src.DO_NOT_USE_inv[OldInventory::VESTPOS];
	inv[LEGPOS] = src.DO_NOT_USE_inv[OldInventory::LEGPOS];
	inv[HEAD1POS] = src.DO_NOT_USE_inv[OldInventory::HEAD1POS];
	inv[HEAD2POS] = src.DO_NOT_USE_inv[OldInventory::HEAD2POS];
	inv[HANDPOS] = src.DO_NOT_USE_inv[OldInventory::HANDPOS];
	inv[SECONDHANDPOS] = src.DO_NOT_USE_inv[OldInventory::SECONDHANDPOS];
	inv[BIGPOCK1POS] = src.DO_NOT_USE_inv[OldInventory::BIGPOCK1POS];
	inv[BIGPOCK2POS] = src.DO_NOT_USE_inv[OldInventory::BIGPOCK2POS];
	inv[BIGPOCK3POS] = src.DO_NOT_USE_inv[OldInventory::BIGPOCK3POS];
	inv[BIGPOCK4POS] = src.DO_NOT_USE_inv[OldInventory::BIGPOCK4POS];
	inv[SMALLPOCK1POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK1POS];
	inv[SMALLPOCK2POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK2POS];
	inv[SMALLPOCK3POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK3POS];
	inv[SMALLPOCK4POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK4POS];
	inv[SMALLPOCK5POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK5POS];
	inv[SMALLPOCK6POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK6POS];
	inv[SMALLPOCK7POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK7POS];
	inv[SMALLPOCK8POS] = src.DO_NOT_USE_inv[OldInventory::SMALLPOCK8POS];

	bInvStatus[HELMETPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::HELMETPOS];
	bInvStatus[VESTPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::VESTPOS];
	bInvStatus[LEGPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::LEGPOS];
	bInvStatus[HEAD1POS] = src.DO_NOT_USE_bInvStatus[OldInventory::HEAD1POS];
	bInvStatus[HEAD2POS] = src.DO_NOT_USE_bInvStatus[OldInventory::HEAD2POS];
	bInvStatus[HANDPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::HANDPOS];
	bInvStatus[SECONDHANDPOS] = src.DO_NOT_USE_bInvStatus[OldInventory::SECONDHANDPOS];
	bInvStatus[BIGPOCK1POS] = src.DO_NOT_USE_bInvStatus[OldInventory::BIGPOCK1POS];
	bInvStatus[BIGPOCK2POS] = src.DO_NOT_USE_bInvStatus[OldInventory::BIGPOCK2POS];
	bInvStatus[BIGPOCK3POS] = src.DO_NOT_USE_bInvStatus[OldInventory::BIGPOCK3POS];
	bInvStatus[BIGPOCK4POS] = src.DO_NOT_USE_bInvStatus[OldInventory::BIGPOCK4POS];
	bInvStatus[SMALLPOCK1POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK1POS];
	bInvStatus[SMALLPOCK2POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK2POS];
	bInvStatus[SMALLPOCK3POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK3POS];
	bInvStatus[SMALLPOCK4POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK4POS];
	bInvStatus[SMALLPOCK5POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK5POS];
	bInvStatus[SMALLPOCK6POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK6POS];
	bInvStatus[SMALLPOCK7POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK7POS];
	bInvStatus[SMALLPOCK8POS] = src.DO_NOT_USE_bInvStatus[OldInventory::SMALLPOCK8POS];

	bInvNumber[HELMETPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::HELMETPOS];
	bInvNumber[VESTPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::VESTPOS];
	bInvNumber[LEGPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::LEGPOS];
	bInvNumber[HEAD1POS] = src.DO_NOT_USE_bInvNumber[OldInventory::HEAD1POS];
	bInvNumber[HEAD2POS] = src.DO_NOT_USE_bInvNumber[OldInventory::HEAD2POS];
	bInvNumber[HANDPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::HANDPOS];
	bInvNumber[SECONDHANDPOS] = src.DO_NOT_USE_bInvNumber[OldInventory::SECONDHANDPOS];
	bInvNumber[BIGPOCK1POS] = src.DO_NOT_USE_bInvNumber[OldInventory::BIGPOCK1POS];
	bInvNumber[BIGPOCK2POS] = src.DO_NOT_USE_bInvNumber[OldInventory::BIGPOCK2POS];
	bInvNumber[BIGPOCK3POS] = src.DO_NOT_USE_bInvNumber[OldInventory::BIGPOCK3POS];
	bInvNumber[BIGPOCK4POS] = src.DO_NOT_USE_bInvNumber[OldInventory::BIGPOCK4POS];
	bInvNumber[SMALLPOCK1POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK1POS];
	bInvNumber[SMALLPOCK2POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK2POS];
	bInvNumber[SMALLPOCK3POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK3POS];
	bInvNumber[SMALLPOCK4POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK4POS];
	bInvNumber[SMALLPOCK5POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK5POS];
	bInvNumber[SMALLPOCK6POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK6POS];
	bInvNumber[SMALLPOCK7POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK7POS];
	bInvNumber[SMALLPOCK8POS] = src.DO_NOT_USE_bInvNumber[OldInventory::SMALLPOCK8POS];
}

MERCPROFILESTRUCT& MERCPROFILESTRUCT::operator=(const OLD_MERCPROFILESTRUCT_101& src)
{
	if ((void*)this != (void*)&src)
	{
		CopyOldInventoryToNew(src);

		for (int i = 0; i < NAME_LENGTH; ++i)
			zName[i] = (CHAR16)src.zName[i];
		for (int i = 0; i < NICKNAME_LENGTH; ++i)
			zNickname[i] = (CHAR16)src.zNickname[i];
		memcpy(&PANTS, &src.PANTS, sizeof(PaletteRepID));
		memcpy(&VEST, &src.VEST, sizeof(PaletteRepID));
		memcpy(&SKIN, &src.SKIN, sizeof(PaletteRepID));
		memcpy(&HAIR, &src.HAIR, sizeof(PaletteRepID));

		memcpy(&bBuddy, &src.bBuddy, 5 * sizeof(UINT8));
		memcpy(&bHated, &src.bHated, 5 * sizeof(UINT8));
		memcpy(&usRoomRangeStart, &src.ubRoomRangeStart, 2 * sizeof(UINT8));
		memcpy(&bMercTownReputation, &src.bMercTownReputation, 20 * sizeof(INT8));
		memcpy(&usApproachFactor, &src.usApproachFactor, 4 * sizeof(UINT16));
		memcpy(&ubApproachVal, &src.ubApproachVal, 4 * sizeof(UINT8));
		memcpy(&ubApproachMod, &src.ubApproachMod, 3 * 4 * sizeof(UINT8));
		memcpy(&bMercOpinion, &src.bMercOpinion, NUMBER_OF_OPINIONS_OLD * sizeof(INT8));
		memcpy(&usStatChangeChances, &src.usStatChangeChances, 12 * sizeof(UINT16));
		memcpy(&usStatChangeSuccesses, &src.usStatChangeSuccesses, 12 * sizeof(UINT16));
		memcpy(&usRoomRangeEnd, &src.ubRoomRangeEnd, 2 * sizeof(UINT8));
		memcpy(&bHatedTime, &src.bHatedTime, 5 * sizeof(INT8));
		memcpy(&bHatedCount, &src.bHatedCount, 5 * sizeof(INT8));

		bLearnToLike = src.bLearnToLike;
		uiAttnSound = src.uiAttnSound;
		uiCurseSound = src.uiCurseSound;
		uiDieSound = src.uiDieSound;
		uiGoodSound = src.uiGoodSound;
		uiGruntSound = src.uiGruntSound;
		uiGrunt2Sound = src.uiGrunt2Sound;
		uiOkSound = src.uiOkSound;
		ubFaceIndex = src.ubFaceIndex;
		bSex = src.bSex;
		bArmourAttractiveness = src.bArmourAttractiveness;
		ubMiscFlags2 = src.ubMiscFlags2;
		fRegresses = src.bEvolution == 2;
		ubMiscFlags = src.ubMiscFlags;
		bSexist = src.bSexist;
		bLearnToHate = src.bLearnToHate;

		bStealRate = src.bStealRate;
		bVocalVolume = src.bVocalVolume;
		ubQuoteRecord = src.ubQuoteRecord;
		bDeathRate = src.bDeathRate;
		bScientific = src.bScientific;
		sExpLevelGain = src.sExpLevelGain;
		sLifeGain = src.sLifeGain;
		sAgilityGain = src.sAgilityGain;
		sDexterityGain = src.sDexterityGain;
		sWisdomGain = src.sWisdomGain;
		sMarksmanshipGain = src.sMarksmanshipGain;
		sMedicalGain = src.sMedicalGain;
		sMechanicGain = src.sMechanicGain;
		sExplosivesGain = src.sExplosivesGain;
		ubBodyType = src.ubBodyType;
		bMedical = src.bMedical;

		usEyesX = src.usEyesX;
		usEyesY = src.usEyesY;
		usMouthX = src.usMouthX;
		usMouthY = src.usMouthY;
		uiEyeDelay = src.uiEyeDelay;
		uiMouthDelay = src.uiMouthDelay;
		uiBlinkFrequency = src.uiBlinkFrequency;
		uiExpressionFrequency = src.uiExpressionFrequency;
		sSectorX = src.sSectorX;
		sSectorY = src.sSectorY;
		uiDayBecomesAvailable = src.uiDayBecomesAvailable;
		bStrength = src.bStrength;
		bLifeMax = src.bLifeMax;
		bExpLevelDelta = src.bExpLevelDelta;
		bLifeDelta = src.bLifeDelta;
		bAgilityDelta = src.bAgilityDelta;
		bDexterityDelta = src.bDexterityDelta;
		bWisdomDelta = src.bWisdomDelta;
		bMarksmanshipDelta = src.bMarksmanshipDelta;
		bMedicalDelta = src.bMedicalDelta;
		bMechanicDelta = src.bMechanicDelta;
		bExplosivesDelta = src.bExplosivesDelta;
		bStrengthDelta = src.bStrengthDelta;
		bLeadershipDelta = src.bLeadershipDelta;

		records.usKillsElites = src.usKills / 4;
		records.usKillsRegulars = src.usKills / 2;
		records.usKillsAdmins = src.usKills / 4;
		records.usKillsHostiles = 0;
		records.usKillsCreatures = 0;
		records.usKillsZombies = 0;
		records.usKillsTanks = 0;
		records.usKillsOthers = 0;
		records.usAssistsMercs = src.usAssists * 3 / 4;
		records.usAssistsMilitia = src.usAssists / 4;
		records.usAssistsOthers = 0;
		records.usShotsFired = src.usShotsFired;
		records.usMissilesLaunched = 0;
		records.usGrenadesThrown = 0;
		records.usKnivesThrown = 0;
		records.usBladeAttacks = 0;
		records.usHtHAttacks = 0;
		records.usShotsHit = src.usShotsHit;
		records.usBattlesTactical = src.usBattlesFought * 3 / 4;
		records.usBattlesAutoresolve = src.usBattlesFought / 4;
		records.usBattlesRetreated = 0;
		records.usAmbushesExperienced = 0;
		records.usLargestBattleFought = 0;
		records.usTimesWoundedShot = src.usTimesWounded;
		records.usTimesWoundedStabbed = 0;
		records.usTimesWoundedPunched = 0;
		records.usTimesWoundedBlasted = 0;
		records.usTimesStatDamaged = 0;
		records.usTimesSurgeryUndergoed = 0;
		records.usFacilityAccidents = 0;
		records.usLocksPicked = 0;
		records.usLocksBreached = 0;
		records.usTrapsRemoved = 0;
		records.usExpDetonated = 0;
		records.usItemsRepaired = 0;
		records.usItemsCombined = 0;
		records.usItemsStolen = 0;
		records.usMercsBandaged = 0;
		records.usSurgeriesMade = 0;
		records.usNPCsDiscovered = 0;
		records.usSectorsDiscovered = 0;
		records.usMilitiaTrained = 0;
		records.ubQuestsHandled = 0;

		usTotalDaysServed = src.usTotalDaysServed;
		sLeadershipGain = src.sLeadershipGain;
		sStrengthGain = src.sStrengthGain;
		uiBodyTypeSubFlags = src.uiBodyTypeSubFlags;
		sSalary = src.sSalary;
		bLife = src.bLife;
		bDexterity = src.bDexterity;
		bDisability = src.bDisability;
		bSkillTraits[0] = src.bSkillTrait;
		bSkillTraits[1] = src.bSkillTrait2;
		bReputationTolerance = src.bReputationTolerance;
		bExplosive = src.bExplosive;
		bLeadership = src.bLeadership;
		bExpLevel = src.bExpLevel;
		bMarksmanship = src.bMarksmanship;
		bMinService = src.bMinService;
		bWisdom = src.bWisdom;
		bResigned = src.bResigned;
		bActive = src.bActive;
		bMainGunAttractiveness = src.bMainGunAttractiveness;
		bAgility = src.bAgility;
		fUseProfileInsertionInfo = src.fUseProfileInsertionInfo;
		sGridNo = src.sGridNo;
		ubQuoteActionID = src.ubQuoteActionID;
		bMechanical = src.bMechanical;
		ubInvUndroppable = src.ubInvUndroppable;
		ubStrategicInsertionCode = src.ubStrategicInsertionCode;
		ubLastQuoteSaid = src.ubLastQuoteSaid;
		bRace = src.bRace;
		bNationality = src.bNationality;
		bAppearance = src.bAppearance;
		bAppearanceCareLevel = src.bAppearanceCareLevel;
		bRefinement = src.bRefinement;
		bRefinementCareLevel = src.bRefinementCareLevel;
		bHatedNationality = src.bHatedNationality;
		bHatedNationalityCareLevel = src.bHatedNationalityCareLevel;
		bRacist = src.bRacist;
		uiWeeklySalary = src.uiWeeklySalary;
		uiBiWeeklySalary = src.uiBiWeeklySalary;
		bMedicalDeposit = src.bMedicalDeposit;
		bAttitude = src.bAttitude;
		bBaseMorale = src.bBaseMorale;
		sMedicalDepositAmount = src.sMedicalDepositAmount;
		bTown = src.bTown;
		bTownAttachment = src.bTownAttachment;
		usOptionalGearCost = src.usOptionalGearCost;
		bApproached = src.bApproached;
		bMercStatus = src.bMercStatus;
		bLearnToLikeTime = src.bLearnToLikeTime;
		bLearnToHateTime = src.bLearnToHateTime;
		bLearnToLikeCount = src.bLearnToLikeCount;
		bLearnToHateCount = src.bLearnToHateCount;
		ubLastDateSpokenTo = src.ubLastDateSpokenTo;
		bLastQuoteSaidWasSpecial = src.bLastQuoteSaidWasSpecial;
		bSectorZ = src.bSectorZ;
		usStrategicInsertionData = src.usStrategicInsertionData;
		bFriendlyOrDirectDefaultResponseUsedRecently = src.bFriendlyOrDirectDefaultResponseUsedRecently;
		bRecruitDefaultResponseUsedRecently = src.bRecruitDefaultResponseUsedRecently;
		bThreatenDefaultResponseUsedRecently = src.bThreatenDefaultResponseUsedRecently;
		bNPCData = src.bNPCData;
		iBalance = src.iBalance;
		sTrueSalary = src.sTrueSalary;
		ubCivilianGroup = src.ubCivilianGroup;
		ubNeedForSleep = src.ubNeedForSleep;
		uiMoney = src.uiMoney;
		bNPCData2 = src.bNPCData2;
		ubMiscFlags3 = src.ubMiscFlags3;
		ubDaysOfMoraleHangover = src.ubDaysOfMoraleHangover;
		ubNumTimesDrugUseInLifetime = src.ubNumTimesDrugUseInLifetime;
		uiPrecedentQuoteSaid = src.uiPrecedentQuoteSaid;
		uiProfileChecksum = src.uiProfileChecksum;
		sPreCombatGridNo = src.sPreCombatGridNo;
		ubTimeTillNextHatedComplaint = src.ubTimeTillNextHatedComplaint;
		ubSuspiciousDeath = src.ubSuspiciousDeath;
		iMercMercContractLength = src.iMercMercContractLength;
		uiTotalCostToDate = src.uiTotalCostToDate;
	}
	return *this;
}
