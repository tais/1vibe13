/**
* @file
* @author Flugente (bears-pit.com)
*/

#include "laptop.h"
#include "insurance.h"
#include "WCheck.h"
#include "Utilities.h"
#include "WordWrap.h"
#include "line.h"
#include "Text.h"
#include "FacilityProduction.h"
#include "Map Screen Interface.h"
#include "Quests.h"
#include "finances.h"
#include "Strategic Town Loyalty.h"
#include "strategic.h"
#include "BaseTable.h"
#include "Campaign Types.h"
#include "GameSettings.h"
#include "LaptopPageResourceOwner.h"
#include "LaptopUiStateModel.h"

// Forward-declare the TestTableTemplate<4> specializations defined
// later in this file so clang doesn't error out on
// 'explicit specialization after instantiation' when the type is
// first used at file scope above the specialization's definition.
template<> void TestTableTemplate<4>::SetRefresh();
template<> void TestTableTemplate<4>::Init( UINT16 sX, UINT16 sY, UINT16 sX_End, UINT16 sY_End );

#define		MERCOMP_FONT_SHADOW								FONT_MCOLOR_WHITE

#define		NUM_LINKS										0
#define		MCA_START_CONTENT_Y								(LAPTOP_SCREEN_WEB_UL_Y + NUM_LINKS * 17)

UINT32 gCHECKBOXLib = 0;
static LaptopPageResourceOwner gFacilityProductionResources;

static BOOLEAN LoadDefaults_Production(LaptopPageResourceOwner& owner)
{
	VOBJECT_DESC	VObjectDesc;

	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( "Interface\\OptionsCheckBoxes.sti", VObjectDesc.ImageFile );
	CHECKF(owner.addVideoObject(&VObjectDesc, gCHECKBOXLib));

	return TRUE;
}

void DisplayDefaults_Production()
{	
	SetFontShadow( MERCOMP_FONT_SHADOW );
}

////////////////////////// MAIN PAGE ////////////////////////////////
struct PRODUCTIONLINE_DATA
{
	UINT16 sector;
	int facilitytype;
	int productionline;

	PRODUCTION_LINE xmldata;

	bool active;
	bool old_state;
	bool can_turnon;
};

std::vector<PRODUCTIONLINE_DATA> gProductionVector;

extern INT32 GetFactoryLeftoverProgress( INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ, UINT16 usFacilityType, UINT16 usProductionNumber );
extern void SetFacilityProductionState( INT16 sSectorX, INT16 sSectorY, INT8 bSectorZ, UINT16 usFacilityType, UINT16 usProductionNumber, BOOLEAN aOn );

void FilterProductionData()
{
	gProductionVector.clear();

	for ( UINT16 sector = 0; sector < 256; ++sector )
	{
		if ( !StrategicMap[SECTOR_INFO_TO_STRATEGIC_INDEX( sector )].fEnemyControlled )
		{
			UINT8 ubTownID = GetTownIdForSector( SECTORX( sector ), SECTORY( sector ) );

			for ( UINT16 cnt = 0; cnt < NUM_FACILITY_TYPES; ++cnt )
			{
				// Is this facility here?
				if ( gFacilityLocations[sector][cnt].fFacilityHere )
				{
					UINT16 productioncnt = 0;
					for ( std::vector<PRODUCTION_LINE>::iterator prodit = gFacilityTypes[cnt].ProductionData.begin(), proditend = gFacilityTypes[cnt].ProductionData.end(); prodit != proditend;
						++prodit, ++productioncnt )
					{
						PRODUCTIONLINE_DATA data;
						data.sector = sector;
						data.facilitytype = cnt;
						data.productionline = productioncnt;
						data.xmldata = ( *prodit );

						INT32 progress = GetFactoryLeftoverProgress( SECTORX( sector ), SECTORY( sector ), 0, cnt, productioncnt );
						
						// despite what is set, several other things can turn a factory off, like loyalty or lua-defined quest checks
						if ( progress < -10
							|| (ubTownID != BLANK_SECTOR &&
								ubTownID < NUM_TOWNS
							&& gTownLoyalty[ubTownID].ubRating < prodit->usOptional_LoyaltyRequired ) )
						{
							data.active = false;
							data.can_turnon = false;
						}
						else
						{
							data.active = ( progress > -1 );
							data.can_turnon = true;
						}

						data.old_state = data.active;

						gProductionVector.push_back( data );
					}
				}
			}
		}
	}
}

void WriteAlteredProductionData()
{
	for ( std::vector<PRODUCTIONLINE_DATA>::iterator it = gProductionVector.begin(), itend = gProductionVector.end(); it != itend; ++it )
	{
		if ( (*it).active != ( *it ).old_state )
		{
			SetFacilityProductionState( SECTORX( ( *it ).sector ), SECTORY( ( *it ).sector ), 0, ( *it ).facilitytype, ( *it ).productionline, ( *it ).active );
		}
	}
}

BOOLEAN gfFacilityProductionRedraw = FALSE;

BOOLEAN EnterFacilityProduction()
{
	LaptopPageResourceOwner stagedResources;
	CHECKF(LoadDefaults_Production(stagedResources));

	FilterProductionData();

	gTestPanel3.Create( LAPTOP_SCREEN_UL_X, MCA_START_CONTENT_Y, LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y );
	gTestPanel3.SetColorLine( Get16BPPColor( FROMRGB( 128, 128, 128 ) ) );
	gTestPanel3.SetColorLineShadow( Get16BPPColor( FROMRGB( 255, 255, 255 ) ) );
	gTestPanel3.SetColorMarked( Get16BPPColor( FROMRGB( 230, 230, 230 ) ) );
	
	TestTableTemplate<4>::getInstance().Create( LAPTOP_SCREEN_UL_X + 4, MCA_START_CONTENT_Y + 4, LAPTOP_SCREEN_LR_X - 4, LAPTOP_SCREEN_WEB_LR_Y - 4 );
	TestTableTemplate<4>::getInstance().SetColorSeparator( Get16BPPColor( FROMRGB( 30, 30, 30 ) ) );

	gTestPanel3.RegisterMemberThingy( &TestTableTemplate<4>::getInstance() );

	TestTableTemplate<4>::getInstance().SetMouseRegionActive( TRUE );
	gFacilityProductionResources = std::move(stagedResources);

	RenderFacilityProduction();

	return( TRUE );
}

void ExitFacilityProduction()
{
	gTestPanel3.Destroy();

	WriteAlteredProductionData();

	gFacilityProductionResources.clear();
}

void HandleFacilityProduction()
{
	if ( gfFacilityProductionRedraw )
	{
		TestTableTemplate<4>::getInstance().Create( TestTableTemplate<4>::getInstance().GetX(), TestTableTemplate<4>::getInstance().GetY(),
		TestTableTemplate<4>::getInstance().GetX() + TestTableTemplate<4>::getInstance().GetWidth(),
		TestTableTemplate<4>::getInstance().GetY() + TestTableTemplate<4>::getInstance().GetHeight() );
		
		RenderFacilityProduction();

		gfFacilityProductionRedraw = FALSE;
	}
}

void RenderFacilityProduction()
{
	DisplayDefaults_Production();

	SetFontShadow( MERCOMP_FONT_SHADOW );

	gTestPanel3.Display();
	
	SetFontShadow( DEFAULT_SHADOW );

	MarkButtonsDirty();
	RenderWWWProgramTitleBar();
	InvalidateRegion( LAPTOP_SCREEN_UL_X, LAPTOP_SCREEN_WEB_UL_Y, LAPTOP_SCREEN_LR_X, LAPTOP_SCREEN_WEB_LR_Y );
}

static CHAR16	gProductionHelpText[100];

STR16 Sectornamegetter_Production( UINT32 aNum )
{
	LaptopUiStateModel::CopyText(gProductionHelpText, L"NAME NOT FOUND");

	if ( aNum < gProductionVector.size() )
	{
		GetShortSectorString( SECTORX( gProductionVector[aNum].sector ), SECTORY( gProductionVector[aNum].sector ), gProductionHelpText );
	}

	return gProductionHelpText;
}

STR16 Product_Production( UINT32 aNum )
{
	LaptopUiStateModel::CopyText(gProductionHelpText, L"NOTHING");

	if ( aNum < gProductionVector.size() )
	{
		if (gProductionVector[aNum].xmldata.szProductionName[0] != L'\0')
			LaptopUiStateModel::CopyText(gProductionHelpText,
				gProductionVector[aNum].xmldata.szProductionName);
		else if (gProductionVector[aNum].xmldata.usItemToCreate != NOTHING &&
			gProductionVector[aNum].xmldata.usItemToCreate < MAXITEMS)
			LaptopUiStateModel::CopyText(gProductionHelpText,
				Item[gProductionVector[aNum].xmldata.usItemToCreate].szItemName);
	}

	return gProductionHelpText;
}

STR16 PreProduct_Production( UINT32 aNum )
{
	LaptopUiStateModel::CopyText(gProductionHelpText, L"--");

	if ( aNum < gProductionVector.size() )
	{
		bool first = true;
		for ( std::vector<PRODUCTION_LINE_PREPRODUCT>::iterator it = gProductionVector[aNum].xmldata.usOptional_PreProducts.begin(), itend = gProductionVector[aNum].xmldata.usOptional_PreProducts.end(); it != itend; ++it )
		{
			if (it->item >= MAXITEMS) continue;
			CHAR16 itemText[100] = {};
			if ( it->requiredforonecreation > 1 )
			{
				sgp_swprintf(itemText, 100, L"%s (%d)",
					Item[it->item].szItemName, it->requiredforonecreation);
			}
			else
			{
				LaptopUiStateModel::CopyText(
					itemText, Item[it->item].szItemName);
			}
			if (first)
			{
				LaptopUiStateModel::CopyText(gProductionHelpText, itemText);
				first = false;
			}
			else
			{
				LaptopUiStateModel::AppendText(gProductionHelpText, L", ");
				LaptopUiStateModel::AppendText(gProductionHelpText, itemText);
			}
		}
	}

	return gProductionHelpText;
}

UINT8 ColourGetter_Production( UINT32 aNum )
{
	if ( aNum < gProductionVector.size() )
	{
		UINT8 ubTownID = GetTownIdForSector( SECTORX( gProductionVector[aNum].sector ), SECTORY( gProductionVector[aNum].sector ) );

		if ( ubTownID != BLANK_SECTOR && ubTownID < NUM_TOWNS
			&& gTownLoyalty[ubTownID].ubRating < gProductionVector[aNum].xmldata.usOptional_LoyaltyRequired )
		{
			return 164;
		}
	}

	return 2;
}

STR16 Loyalty_Production( UINT32 aNum )
{
	LaptopUiStateModel::CopyText(gProductionHelpText, L"--");

	if ( aNum < gProductionVector.size() && gProductionVector[aNum].xmldata.usOptional_LoyaltyRequired > 0 )
	{
		sgp_swprintf(gProductionHelpText, std::size(gProductionHelpText),
			L"%d+", gProductionVector[aNum].xmldata.usOptional_LoyaltyRequired);
	}

	return gProductionHelpText;
}

STR16 Costgetter_Production( UINT32 aNum )
{
	LaptopUiStateModel::CopyText(gProductionHelpText, L"--");

	if ( aNum < gProductionVector.size() && gProductionVector[aNum].xmldata.sHourlyCost )
	{
		sgp_swprintf(gProductionHelpText, std::size(gProductionHelpText),
			L"%d$", gProductionVector[aNum].xmldata.sHourlyCost);
	}

	return gProductionHelpText;
}

STR16 Minutesgetter_Production( UINT32 aNum )
{
	LaptopUiStateModel::CopyText(gProductionHelpText, L"--:--");

	if ( aNum < gProductionVector.size() )
	{
		sgp_swprintf(gProductionHelpText, std::size(gProductionHelpText),
			L"%02d:%02d",
			gProductionVector[aNum].xmldata.sMinutesRequired / 60,
			gProductionVector[aNum].xmldata.sMinutesRequired % 60);
	}

	return gProductionHelpText;
}

BOOLEAN Activegetter_Production( UINT32 aNum )
{
	if ( aNum < gProductionVector.size() )
	{
		return gProductionVector[aNum].active;
	}

	return FALSE;
}

void CheckboxImagegetter_Production( UINT32 aNum, UINT32& arImageLib, UINT16& arImage )
{
	arImageLib = gCHECKBOXLib;
	arImage = 0;

	if ( aNum < gProductionVector.size() )
	{
		if ( !gProductionVector[aNum].can_turnon )
			arImage = 4;
		else if ( gProductionVector[aNum].active )
			arImage = 2;
	}
}

void Factory_ProgressBar( UINT32 aId, PIXEL& arCol1, UINT16& arVal1, PIXEL& arCol2, UINT16& arVal2, PIXEL& arCol3, UINT16& arVal3, PIXEL& arCol4, UINT16& arVal4 )
{
	if ( aId < gProductionVector.size() )
{
		INT32 productiontime = GetFactoryLeftoverProgress( SECTORX( gProductionVector[aId].sector ), SECTORY( gProductionVector[aId].sector ), 0, gProductionVector[aId].facilitytype, gProductionVector[aId].productionline );

		// use stat bar colours, because why not
		arCol1 = Get16BPPColor( FROMRGB( gGameExternalOptions.ubStatProgressBarsRed, gGameExternalOptions.ubStatProgressBarsGreen, gGameExternalOptions.ubStatProgressBarsBlue ) );

		if ( productiontime > -1 && gProductionVector[aId].xmldata.sMinutesRequired > 0 )
			arVal1 = static_cast<UINT16>(std::min<INT64>(100,
				100LL * productiontime /
					gProductionVector[aId].xmldata.sMinutesRequired));
		else
			arVal1 = 0;
	}
}

void callbacktoggle( INT32 usId )
{
	if ( usId < gProductionVector.size() && gProductionVector[usId].can_turnon )
	{
		gProductionVector[usId].active = !gProductionVector[usId].active;

		TestTableTemplate<4>::getInstance().SetRefresh();
	}
}

template<>	void	TestTableTemplate<4>::SetRefresh() { gfFacilityProductionRedraw = TRUE; }

template<>  void	TestTableTemplate<4>::Init( UINT16 sX, UINT16 sY, UINT16 sX_End, UINT16 sY_End )
{
	ClearColumnDataProvider();

	// on/off toggle
	{
		ColumnDataProvider col( L"" );
		col.SetRequiredHeigth( 12 );
		col.SetRequiredLength( 24 );
		col.SetMethodImage( CheckboxImagegetter_Production );
		col.SetNumberOfEntries( gProductionVector.size() );
		col.SetCallBackType( ColumnDataProvider::CDP_TOGGLE );

		AddColumnDataProvider( col );
	}

	// sector
	{
		ColumnDataProvider col( pUpdateMapInfoText[15] );
		col.SetMethodString( Sectornamegetter_Production );
		col.SetNumberOfEntries( gProductionVector.size() );
		col.SetCallBackType( ColumnDataProvider::CDP_MILITIA_LIST );

		AddColumnDataProvider( col );
	}

	// name
	{
		ColumnDataProvider col( szFactoryText[4] );
		col.SetMethodString( Product_Production );
		col.SetNumberOfEntries( gProductionVector.size() );
		col.SetCallBackType( ColumnDataProvider::CDP_MILITIA_LIST );

		AddColumnDataProvider( col );
	}

	// required items to use up
	{
		ColumnDataProvider col( szFactoryText[5] );
		col.SetMethodString( PreProduct_Production );
		col.SetNumberOfEntries( gProductionVector.size() );
		col.SetCallBackType( ColumnDataProvider::CDP_MILITIA_LIST );

		AddColumnDataProvider( col );
	}

	// loyalty
	{
		ColumnDataProvider col( pwTownInfoStrings[5] );
		col.SetMethodString( Loyalty_Production );
		col.SetMethodTextColour( ColourGetter_Production );
		col.SetNumberOfEntries( gProductionVector.size() );
		col.SetCallBackType( ColumnDataProvider::CDP_MILITIA_LIST );

		AddColumnDataProvider( col );
	}

	// cost
	{
		ColumnDataProvider col( L"$/h" );
		col.SetMethodString( Costgetter_Production );
		col.SetNumberOfEntries( gProductionVector.size() );
		col.SetCallBackType( ColumnDataProvider::CDP_MILITIA_LIST );

		AddColumnDataProvider( col );
	}

	// current progress
	{
		ColumnDataProvider col( L"" );
		col.SetMethodStatusBar( Factory_ProgressBar );
		col.SetNumberOfEntries( gProductionVector.size() );
		col.SetCallBackType( ColumnDataProvider::CDP_MILITIA_LIST );

		AddColumnDataProvider( col );
	}

	// time required
	{
		ColumnDataProvider col( szFactoryText[6] );
		col.SetMethodString( Minutesgetter_Production );
		col.SetNumberOfEntries( gProductionVector.size() );
		col.SetCallBackType( ColumnDataProvider::CDP_MILITIA_LIST );

		AddColumnDataProvider( col );
	}
	
	TestTable::Init( sX, sY, sX_End, sY_End );
}
////////////////////////// MAIN PAGE ////////////////////////////////
