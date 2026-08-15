#include "SelectedCatalogExport.h"
#include "TextCatalog.h"
#include "Map Screen Interface.h"
#include "personnel.h"
#include "soldier profile type.h"
#include "Interface.h"
#include "Keys.h"
#include "Merc Contract.h"
#include "Campaign Types.h"
#include "GameSettings.h"
#include "finances.h"
#include "laptop.h"
#include "Assignments.h"
#include "history.h"
#include "Text.h"

extern STR16 pBullseyeStrings[];
extern STR16 pContractButtonString[];
extern STR16 pUpdatePanelButtons[];
extern STR16 gzIntroScreen[];
extern STR16 sRepairsDoneString[];

namespace
{
template<typename T>
void ExportSection(i18n::SelectedCatalogExportSink& sink,
	std::wstring_view section, T* strings, int first, int limit)
{
	for (int index = first; index < limit; ++index)
	{
		const std::wstring_view text{strings[index]};
		if (!text.empty()) sink.copyEntry(section, index, text);
	}
}

template<>
void ExportSection<wchar_t>(i18n::SelectedCatalogExportSink& sink,
	std::wstring_view section, wchar_t* strings, int first, int limit)
{
	ExportSection(sink, section, &strings, first, limit);
}

void ExportTextPackEntry(i18n::SelectedCatalogExportSink& sink,
	i18n::TextKey key)
{
	const auto* descriptor = i18n::FindTextKey(key);
	const auto text = i18n::GetCompiledTextPack().lookup(key);
	if (!descriptor || !text || text.text.empty()) return;
	sink.copyEntry(descriptor->legacyExportSection, 0, text.text);
}

void ExportTextPackTable(i18n::SelectedCatalogExportSink& sink,
	i18n::TextTableKey key)
{
	const auto* descriptor = i18n::FindTextTable(key);
	if (!descriptor) return;
	const auto& pack = i18n::GetCompiledTextPack();
	const auto exportEnd =
		descriptor->legacyExportFirst + descriptor->legacyExportCount;
	for (std::size_t index = descriptor->legacyExportFirst;
		index < exportEnd; ++index)
	{
		const auto text = pack.lookup(key, index);
		if (!text || text.text.empty()) continue;
		sink.copyEntry(descriptor->legacyExportSection,
			static_cast<int>(index), text.text);
	}
}
}

void i18n::ExportSelectedCatalog(SelectedCatalogExportSink& sink)
{
#ifdef static_assert
#error "GameStrings export limits require the built-in static_assert keyword"
#endif
#include "ExportStringLimitContract.inc"

	//not_required ExportSection(sink, L"Ja2Credits", ::pCreditsJA2113, 0, 7);
	ExportSection(sink, L"WeaponType",					::WeaponType,					0,	GUN_TYPES_MAX);
	ExportSection(sink, L"TeamTurn",					::TeamTurnString,				0,	10);
	ExportSection(sink, L"Message",					::Message,						0,	TEXT_NUM_STR_MESSAGE);
	ExportSection(sink, L"TownNames",					::pTownNames,					0,	MAX_TOWNS);
	ExportTextPackTable(sink, i18n::TextTableKey::TimeCompression);
	ExportSection(sink, L"Assignment",					::pAssignmentStrings,			0,	NUM_ASSIGNMENTS);
	ExportSection(sink, L"PersonnelAssignment",		::pPersonnelAssignmentStrings,	0,	NUM_ASSIGNMENTS);
	ExportSection(sink, L"LongAssignment",				::pLongAssignmentStrings,		0,	NUM_ASSIGNMENTS);
	ExportSection(sink, L"Militia",					::pMilitiaString,				0,	3);

	ExportSection(sink, L"MilitiaButton",				::pMilitiaButtonString,			0,	2);
	ExportSection(sink, L"Condition",					::pConditionStrings,				0,	9);
	ExportSection(sink, L"EpcMenu",					::pEpcMenuStrings,				0,	MAX_EPC_MENU_STRING_COUNT);
	ExportSection(sink, L"Contract",					::pContractStrings,				0,	MAX_CONTRACT_MENU_STRING_COUNT);
	ExportSection(sink, L"POW",						::pPOWStrings,					0,	2);
	ExportSection(sink, L"InvPanelTitle",				::pInvPanelTitleStrings,			0,	5);
	ExportTextPackTable(sink, i18n::TextTableKey::LongAttribute);
	ExportSection(sink, L"ShortAttribute",				::pShortAttributeStrings,		0,	10);
	ExportSection(sink, L"UpperLeftMapScreen",			::pUpperLeftMapScreenStrings,	0,	6);
	ExportTextPackTable(sink, i18n::TextTableKey::Training);

	ExportTextPackTable(sink, i18n::TextTableKey::GuardMenu);
	ExportTextPackTable(sink, i18n::TextTableKey::OtherGuardMenu);
	ExportSection(sink, L"AssignMenu",					::pAssignMenuStrings,			0,	MAX_ASSIGN_STRING_COUNT);
	ExportSection(sink, L"MilitiaControlMenu",			::pMilitiaControlMenuStrings,	0,	MAX_MILCON_STRING_COUNT);
	ExportSection(sink, L"RemoveMerc",					::pRemoveMercStrings,			0,	MAX_REMOVE_MERC_COUNT);
	ExportSection(sink, L"AttributeMenu",				::pAttributeMenuStrings,			0,	MAX_ATTRIBUTE_STRING_COUNT);
	ExportSection(sink, L"TrainingMenu",				::pTrainingMenuStrings,			0,	MAX_TRAIN_STRING_COUNT);
	ExportSection(sink, L"SquadMenu",					::pSquadMenuStrings,				0,	MAX_SQUAD_MENU_STRING_COUNT);

	ExportSection(sink, L"SnitchMenu",					::pSnitchMenuStrings,			0,	MAX_SNITCH_MENU_STRING_COUNT);
	ExportSection(sink, L"SnitchMenuDesc",				::pSnitchMenuDescStrings,		0,	MAX_SNITCH_MENU_STRING_COUNT-1);
	ExportSection(sink, L"SnitchToggleMenu",			::pSnitchToggleMenuStrings,		0,	MAX_SNITCH_TOGGLE_MENU_STRING_COUNT);
	ExportSection(sink, L"SnitchToggleMenuDesc",		::pSnitchToggleMenuDescStrings,	0,	MAX_SNITCH_TOGGLE_MENU_STRING_COUNT-1);
	ExportSection(sink, L"SnitchSectorMenu",			::pSnitchSectorMenuStrings,		0,	MAX_SNITCH_SECTOR_MENU_STRING_COUNT);
	ExportSection(sink, L"SnitchSectorMenuDesc",		::pSnitchSectorMenuDescStrings,	0,	MAX_SNITCH_SECTOR_MENU_STRING_COUNT-1);
	ExportSection(sink, L"PrisonerMenu",				::pPrisonerMenuStrings,			0,  MAX_PRISONER_MENU_STRING_COUNT );
	ExportSection(sink, L"PrisonerMenuDesc",			::pPrisonerMenuDescStrings,		0,  MAX_PRISONER_MENU_STRING_COUNT - 1 );
	ExportSection(sink, L"SnitchPrisonExposed",		::pSnitchPrisonExposedStrings,	0,	NUM_SNITCH_PRISON_EXPOSED);
	ExportSection(sink, L"SnitchGatheringRumoursResult",	::pSnitchGatheringRumoursResultStrings,	0,	NUM_SNITCH_GATHERING_RUMOURS_RESULT);

	ExportTextPackEntry(sink, i18n::TextKey::PersonnelTitle);
	ExportSection(sink, L"PersonnelScreen",			::pPersonnelScreenStrings,		0,	TEXT_NUM_PRSNL);

	ExportSection(sink, L"MercSkill",					::gzMercSkillText,				0,	NUM_SKILLTRAITS_OT);
	ExportSection(sink, L"TacticalPopupButton",		::pTacticalPopupButtonStrings,	0,	NUM_ICONS);
	ExportSection(sink, L"DoorTrap",					::pDoorTrapStrings,				0,	NUM_DOOR_TRAPS);
	ExportTextPackTable(sink, i18n::TextTableKey::ContractExtend);
	ExportSection(sink, L"MapScreenMouseRegionHelp",	::pMapScreenMouseRegionHelpText,	0,	6);
	ExportSection(sink, L"NoiseVol",					::pNoiseVolStr,					0,	4);
	ExportTextPackTable(sink, i18n::TextTableKey::NoiseType);
	ExportSection(sink, L"Direction",					::pDirectionStr,					0,	8);
	ExportSection(sink, L"LandType",					::pLandTypeStrings,				0,	NUM_TRAVTERRAIN_TYPES);
	ExportSection(sink, L"Strategic",					::gpStrategicString,				0,	TEXT_NUM_STRATEGIC_TEXT);

	ExportTextPackEntry(sink, i18n::TextKey::GameClockDay);
	ExportSection(sink, L"KeyDescription",				::sKeyDescriptionStrings,		0,	2);
	ExportSection(sink, L"WeaponStatsDesc",			::gWeaponStatsDesc,				0,	17);
	ExportSection(sink, L"WeaponStatsFasthelpTactical",::gzWeaponStatsFasthelpTactical, 0,	29);
	ExportSection(sink, L"MiscItemStatsFasthelp",		::gzMiscItemStatsFasthelp,		0,	34);
	ExportSection(sink, L"MoneyStatsDesc",				::gMoneyStatsDesc,				0,	TEXT_NUM_MONEY_DESC);

	ExportSection(sink, L"Health",						::zHealthStr,					0,	7);
	ExportSection(sink, L"MoneyAmounts",				::gzMoneyAmounts,				0,	6);
	ExportSection(sink, L"ProsLabel",					::gzProsLabel,					0,	1);
	ExportSection(sink, L"ConsLabel",					::gzConsLabel,					0,	1);
	ExportSection(sink, L"TalkMenu",					::zTalkMenuStrings,				0,	6);
	ExportSection(sink, L"Dealer",						::zDealerStrings,				0,	4);
	ExportSection(sink, L"DialogActions",				::zDialogActions,				0,	1);
	ExportSection(sink, L"Vehicle",					::pVehicleStrings,				0,	6);
	ExportSection(sink, L"ShortVehicle",				::pShortVehicleStrings,			0,	6);
	ExportSection(sink, L"VehicleName",				::zVehicleName,					0,	6);
	ExportSection(sink, L"VehicleSeatsStrings",		::pVehicleSeatsStrings,			0,	2);

	ExportSection(sink, L"Tactical",					::TacticalStr,					0,	TEXT_NUM_TACTICAL_STR);
	ExportSection(sink, L"ExitingSectorHelp",			::pExitingSectorHelpText,		0,	TEXT_NUM_EXIT_GUI);
	ExportSection(sink, L"Repair",						::pRepairStrings,				0,	4);
	ExportSection(sink, L"PreStatBuild",				::sPreStatBuildString,			0,	6);
	ExportSection(sink, L"StatGain",					::sStatGainStrings,				0,	11);
	ExportSection(sink, L"HelicopterEta",				::pHelicopterEtaStrings,			0,	TEXT_NUM_STR_HELI_ETA);
	ExportSection(sink, L"HelicopterRepair",			::pHelicopterRepairRefuelStrings,		0,	TEXT_NUM_STR_HELI_REPAIRS);
	ExportSection(sink, L"MapLevel",					::sMapLevelString,				0,	1);
	ExportSection(sink, L"Loyal",						::gsLoyalString,					0,	1);
	ExportSection(sink, L"Underground",				::gsUndergroundString,			0,	1);
	ExportTextPackTable(sink, i18n::TextTableKey::TimeUnits);

	ExportSection(sink, L"Facilities",					::sFacilitiesStrings,			0,	7);
	ExportSection(sink, L"MapPopUpInventory",			::pMapPopUpInventoryText,		0,	2);
	ExportSection(sink, L"TownInfo",					::pwTownInfoStrings,				0,	12);
	ExportSection(sink, L"Mine",						::pwMineStrings,					0,	14);
	ExportSection(sink, L"MiscSector",					::pwMiscSectorStrings,			0,	7);
	ExportSection(sink, L"MapInventoryError",			::pMapInventoryErrorString,		0,	7);
	ExportSection(sink, L"MapInventory",				::pMapInventoryStrings,			0,	2);
	ExportSection(sink, L"MapScreenFastHelp",			::pMapScreenFastHelpTextList,	0,	10);
	ExportSection(sink, L"MovementMenu",				::pMovementMenuStrings,			0,	4);
	ExportSection(sink, L"UpdateMerc",					::pUpdateMercStrings,			0,	6);

	ExportSection(sink, L"MapScreenBorderButtonHelp",	::pMapScreenBorderButtonHelpText,0,	6);
	ExportSection(sink, L"MapScreenBottomFastHelp",	::pMapScreenBottomFastHelp,		0,	8);
	ExportSection(sink, L"MapScreenBottom",			::pMapScreenBottomText,			0,	1);
	ExportSection(sink, L"MercDead",					::pMercDeadString,				0,	1);
	ExportTextPackTable(sink, i18n::TextTableKey::Day);
	ExportSection(sink, L"SenderName",					::pSenderNameList,				0,	51);
	ExportTextPackTable(sink, i18n::TextTableKey::Traverse);
	ExportSection(sink, L"NewMail",					::pNewMailStrings,				0,	1);
	ExportSection(sink, L"DeleteMail",					::pDeleteMailStrings,			0,	2);
	ExportSection(sink, L"EmailHeader",				::pEmailHeaders,					0,	3);

	ExportTextPackEntry(sink, i18n::TextKey::EmailTitle);
	ExportTextPackEntry(sink, i18n::TextKey::FinanceTitle);
	ExportSection(sink, L"FinanceSummary",				::pFinanceSummary,				0,	12);
	ExportSection(sink, L"FinanceHeader",				::pFinanceHeaders,				0,	7);
	ExportSection(sink, L"Transaction",				::pTransactionText,				0,	TEXT_NUM_FINCANCES);
	ExportSection(sink, L"TransactionAlternate",		::pTransactionAlternateText,		0,	4);
	ExportSection(sink, L"Skyrider",					::pSkyriderText,					0,	7);
	ExportSection(sink, L"Moral",						::pMoralStrings,					0,	6);
	ExportSection(sink, L"LeftEquipment",				::pLeftEquipmentString,			0,	2);
	ExportSection(sink, L"MapScreenStatus",			::pMapScreenStatusStrings,		0,	5);

	ExportSection(sink, L"MapScreenPrevNextCharButtonHelp",	::pMapScreenPrevNextCharButtonHelpText,	0,	2);
	ExportTextPackTable(sink, i18n::TextTableKey::Eta);
	ExportSection(sink, L"TrashItem",							::pTrashItemText,						0,	2);
	ExportSection(sink, L"MapError",							::pMapErrorString,						0,	50);
	ExportSection(sink, L"MapPlot",							::pMapPlotStrings,						0,	5);
	ExportSection(sink, L"Bullseye",							::pBullseyeStrings,						0,	5);
	ExportSection(sink, L"MiscMapScreenMouseRegionHelp",		::pMiscMapScreenMouseRegionHelpText,		0,	3);
	ExportSection(sink, L"MercHeLeave",						::pMercHeLeaveString,					0,	2);
	ExportSection(sink, L"MercSheLeave",						::pMercSheLeaveString,					0,	2);
	ExportTextPackTable(sink, i18n::TextTableKey::MercContractOver);

	ExportSection(sink, L"ImpPopUp",					::pImpPopUpStrings,				0,	12);
	ExportSection(sink, L"ImpButton",					::pImpButtonText,				0,	26);
	ExportSection(sink, L"ExtraIMP",					::pExtraIMPStrings,				0,	4);
	ExportTextPackEntry(sink, i18n::TextKey::FilesTitle);
	ExportSection(sink, L"FilesSender",				::pFilesSenderList,				0,	7);
	ExportTextPackEntry(sink, i18n::TextKey::HistoryTitle);
	ExportSection(sink, L"HistoryHeader",				::pHistoryHeaders,				0,	5);
	//ExportSection(sink, L"History",					::pHistoryStrings,				0,	TEXT_NUM_HISTORY);
	ExportSection(sink, L"HistoryLocation",			::pHistoryLocations,				0,	1);
	ExportSection(sink, L"LaptopIcon",					::pLaptopIcons,					0,	8);

	ExportSection(sink, L"BookMark",					::pBookMarkStrings,				0,	TEXT_NUM_LAPTOP_BOOKMARKS);
	ExportSection(sink, L"BookmarkTitle",				::pBookmarkTitle,				0,	2);
	ExportSection(sink, L"Download",					::pDownloadString,				0,	2);
	ExportSection(sink, L"AtmStartButton",				::gsAtmStartButtonText,			0,	4);
	ExportSection(sink, L"Error",						::pErrorStrings,					0,	5);
	ExportSection(sink, L"Personnel",					::pPersonnelString,				0,	1);
	ExportSection(sink, L"WebTitle",					::pWebTitle,						0,	1);
	ExportSection(sink, L"WebPagesTitle",				::pWebPagesTitles,				0,	36);

	ExportSection(sink, L"ShowBookmark",				::pShowBookmarkString,				0,	2);
	ExportSection(sink, L"LaptopTitle",				::pLaptopTitles,						0,	5);
	ExportSection(sink, L"PersonnelDepartedState",		::pPersonnelDepartedStateStrings,	0,	TEXT_NUM_DEPARTED);
	ExportSection(sink, L"PersonelTeam",				::pPersonelTeamStrings,				0,	8);
	ExportSection(sink, L"PersonnelCurrentTeamStats",	::pPersonnelCurrentTeamStatsStrings, 0,	3);
	ExportSection(sink, L"PersonnelTeamStats",			::pPersonnelTeamStatsStrings,		0,	11);
	ExportSection(sink, L"MapVertIndex",				::pMapVertIndex,						0,	17);
	ExportSection(sink, L"MapHortIndex",				::pMapHortIndex,						0,	17);
	ExportSection(sink, L"MapDepthIndex",				::pMapDepthIndex,					0,	4);
	ExportSection(sink, L"ContractButton",				::pContractButtonString,				0,	1);

	ExportSection(sink, L"UpdatePanelButton",			::pUpdatePanelButtons,			0,	2);
	ExportSection(sink, L"LargeTactical",				::LargeTacticalStr,				0,	TEXT_NUM_LARGESTR);
	ExportSection(sink, L"InsContract",				::InsContractText,				0,	TEXT_NUM_INS_CONTRACT);
	ExportSection(sink, L"InsInfo",					::InsInfoText,					0,	TEXT_NUM_INS_INFO);
	ExportSection(sink, L"MercAccount",				::MercAccountText,				0,	TEXT_NUM_MERC_ACCOUNT);
	ExportSection(sink, L"MercAccountPage",			::MercAccountPageText,			0,	2);
	ExportSection(sink, L"MercInfo",					::MercInfo,						0,	TEXT_NUM_MERC_FILES);
	ExportSection(sink, L"MercNoAccount",				::MercNoAccountText,				0,	TEXT_NUM_MERC_NO_ACC);
	ExportSection(sink, L"MercHomePage",				::MercHomePageText,				0,	TEXT_NUM_MERC);
	ExportSection(sink, L"Funeral",					::sFuneralString,				0,	TEXT_NUM_FUNERAL);

	ExportSection(sink, L"Florist",					::sFloristText,					0,	TEXT_NUM_FLORIST);
	ExportSection(sink, L"OrderForm",					::sOrderFormText,				0,	TEXT_NUM_FLORIST_ORDER);
	ExportSection(sink, L"FloristGallery",				::sFloristGalleryText,			0,	TEXT_NUM_FLORIST_GALLERY);
	ExportSection(sink, L"FloristCards",				::sFloristCards,					0,	TEXT_NUM_FLORIST_CARDS);
	ExportSection(sink, L"BobbyROrderForm",			::BobbyROrderFormText,			0,	TEXT_NUM_BOBBYR_MAILORDER);
	ExportSection(sink, L"BobbyRFilter",				::BobbyRFilter,					0,	TEXT_NUM_BOBBYR_FILTER);
	ExportSection(sink, L"BobbyR",						::BobbyRText,					0,	TEXT_NUM_BOBBYR_GUNS);
	ExportSection(sink, L"BobbyRaysFront",				::BobbyRaysFrontText,			0,	TEXT_NUM_BOBBYR);
	ExportTextPackTable(sink, i18n::TextTableKey::AimSort);
	ExportSection(sink, L"AimPolicy", ::AimPolicyText, 0, TEXT_NUM_AIM_POLICIES);

	ExportSection(sink, L"AimMember",					::AimMemberText,					0,	4);
	ExportSection(sink, L"CharacterInfo",				::CharacterInfo,					0,	TEXT_NUM_AIM_MEMBER_CHARINFO);
	ExportSection(sink, L"VideoConfercing",			::VideoConfercingText,			0,	TEXT_NUM_AIM_MEMBER_VCONF);
	ExportSection(sink, L"AimPopUp",					::AimPopUpText,					0,	TEXT_NUM_AIM_MEMBER_POPUP);
	ExportTextPackEntry(sink, i18n::TextKey::AimLinksTitle);
	ExportSection(sink, L"AimHistory",					::AimHistoryText,				0,	TEXT_NUM_AIM_HISTORY);
	ExportSection(sink, L"AimFi",						::AimFiText,						0,	TEXT_NUM_AIM_FI);
	ExportSection(sink, L"AimAlumni",					::AimAlumniText,					0,	TEXT_NUM_AIM_ALUMNI);
	ExportSection(sink, L"AimScreen",					::AimScreenText,					0,	TEXT_NUM_AIM_SCREEN);
	ExportSection(sink, L"AimBottomMenu",				::AimBottomMenuText,				0,	TEXT_NUM_AIM_MENU);

	ExportSection(sink, L"SKI",						::SKI_Text,						0, TEXT_NUM_SKI_TEXT);
	ExportTextPackTable(sink, i18n::TextTableKey::SkiAtm);
	ExportSection(sink, L"SkiAtmText",					::gzSkiAtmText,					0, TEXT_NUM_SKI_ATM_MODE_TEXT);
	ExportSection(sink, L"SkiMessageBox",				::SkiMessageBoxText,				0, TEXT_NUM_SKI_MBOX_TEXT);
	ExportSection(sink, L"Options",					::zOptionsText,					0, TEXT_NUM_OPT_TEXT);
	ExportSection(sink, L"SaveLoad",					::zSaveLoadText,					0, TEXT_NUM_SLG_TEXT);
	ExportSection(sink, L"MarksMapScreen",				::zMarksMapScreenText,			0, 25);
	ExportSection(sink, L"LandMarkInSector",			::pLandMarkInSectorString,		0, 1);
	ExportSection(sink, L"MilitiaConfirm",				::pMilitiaConfirmStrings,		0, 11);
	ExportSection(sink, L"MoneyWithdrawMessage",		::gzMoneyWithdrawMessageText,	0, TEXT_NUM_MONEY_WITHDRAW);

	ExportSection(sink, L"Copyright",					::gzCopyrightText,				0,	1);
	ExportSection(sink, L"OptionsToggle",				::zOptionsToggleText,			0,	49);
	ExportSection(sink, L"OptionsScreenHelp",			::zOptionsScreenHelpText,		0,	49);
	ExportSection(sink, L"GIOScreen",					::gzGIOScreenText,				0,	TEXT_NUM_GIO_TEXT);
	ExportSection(sink, L"MPJScreen",					::gzMPJScreenText,				0,	TEXT_NUM_MPJ_TEXT);
	ExportSection(sink, L"MPJHelpText",				::gzMPJHelpText,					0,	10);
	ExportSection(sink, L"MPHScreen",					::gzMPHScreenText,				0,	TEXT_NUM_MPH_TEXT);
	ExportSection(sink, L"DeliveryLocation",			::pDeliveryLocationStrings,		0,	17);
	ExportSection(sink, L"SkillAtZeroWarning",			::pSkillAtZeroWarning,			0,	1);
	ExportSection(sink, L"IMPBeginScreen",				::pIMPBeginScreenStrings,		0,	1);
	ExportTextPackTable(sink, i18n::TextTableKey::ImpFinishButton);

	ExportSection(sink, L"IMPFinish",					::pIMPFinishStrings,				0,	1);
	ExportTextPackTable(sink, i18n::TextTableKey::ImpVoices);
	ExportTextPackTable(sink, i18n::TextTableKey::DepartedMercPortrait);
	ExportSection(sink, L"PersTitle",					::pPersTitleText,				0,	1);
	ExportTextPackTable(sink, i18n::TextTableKey::PausedGame);
	ExportSection(sink, L"MessageStrings",				::pMessageStrings,				0,	TEXT_NUM_MSG);
	ExportSection(sink, L"ItemPickupHelpPopup",		::ItemPickupHelpPopup,			0,	5);
	ExportSection(sink, L"DoctorWarning",				::pDoctorWarningString,			0,	2);
	ExportSection(sink, L"MilitiaButtonsHelp",			::pMilitiaButtonsHelpText,		0,	4);
	ExportSection(sink, L"MapScreenJustStartedHelp",	::pMapScreenJustStartedHelpText,	0,	2);

	ExportSection(sink, L"AntiHacker",					::pAntiHackerString,				0,	TEXT_NUM_ANTIHACKERSTR);
	ExportSection(sink, L"LaptopHelp",					::gzLaptopHelpText,				0,	TEXT_NUM_LAPTOP_BN_BOOKMARK_TEXT);
	ExportTextPackEntry(sink, i18n::TextKey::HelpScreenExit);
	ExportSection(sink, L"NonPersistantPBI",			::gzNonPersistantPBIText,		0,	10);
	ExportTextPackTable(sink, i18n::TextTableKey::MiscString);
	ExportSection(sink, L"IntroScreen",				::gzIntroScreen,					0,	1);
	ExportSection(sink, L"NewNoise",					::pNewNoiseStr,					0,	11/*MAX_NOISES*/);
	ExportSection(sink, L"MapScreenSortButtonHelp",	::wMapScreenSortButtonHelpText,	0,	6);
	ExportSection(sink, L"BrokenLink",					::BrokenLinkText,				0,	TEXT_NUM_BROKEN_LINK);
	ExportSection(sink, L"BobbyRShipment",				::gzBobbyRShipmentText,			0,	TEXT_NUM_BOBBYR_SHIPMENT);

	ExportSection(sink, L"CreditNames",				::gzCreditNames,					0,	15);
	ExportSection(sink, L"CreditNameTitle",			::gzCreditNameTitle,				0,	15);
	ExportSection(sink, L"CreditNameFunny",			::gzCreditNameFunny,				0,	15);
	ExportSection(sink, L"RepairsDone",				::sRepairsDoneString,			0,	7);
	ExportTextPackTable(sink, i18n::TextTableKey::GioDifConfirm);
	ExportSection(sink, L"LateLocalized",				::gzLateLocalizedString,			0,	64);
	ExportSection(sink, L"CWStrings",					::gzCWStrings,					0,	1);
	ExportSection(sink, L"TooltipStrings",				::gzTooltipStrings,				0,	TEXT_NUM_STR_TT);
	ExportSection(sink, L"New113Message",				::New113Message,					0,	TEXT_NUM_MSG113);

	ExportSection(sink, L"New113HAMMessage",			::New113HAMMessage,				0,	25);
	ExportSection(sink, L"New113MERCMercMail",			::New113MERCMercMailTexts,		0,	4);
	ExportSection(sink, L"New113AIMMercMail",			::New113AIMMercMailTexts,		0,	16);
	ExportSection(sink, L"MissingIMPSkills",			::MissingIMPSkillsDescriptions,	0,	2);
	ExportSection(sink, L"NewInvMessage",				::NewInvMessage,					0,	TEXT_NUM_NIV);
	ExportSection(sink, L"MPServerMessage",			::MPServerMessage,				0,	13);
	ExportSection(sink, L"MPClientMessage",			::MPClientMessage,				0,	69);
	ExportSection(sink, L"MPEdges",					::gszMPEdgesText,				0,	5);
	ExportSection(sink, L"MPTeamName",					::gszMPTeamNames,				0,	5);
	ExportSection(sink, L"MPMapscreen",				::gszMPMapscreenText,			0,	9);

	ExportSection(sink, L"MPSScreen",					::gzMPSScreenText,				0,	TEXT_NUM_MPS_TEXT);
	ExportSection(sink, L"MPCScreen",					::gzMPCScreenText,				0,	TEXT_NUM_MPC_TEXT);
	ExportSection(sink, L"MPChatToggle",				::gzMPChatToggleText,			0,	2);
	ExportSection(sink, L"MPChatbox",					::gzMPChatboxText,				0,	2);
}
