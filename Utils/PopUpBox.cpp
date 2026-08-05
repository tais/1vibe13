#include "PopUpBox.h"
#include "sysutil.h"
#include "UtilsUiStateModel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>


#define BORDER_WIDTH	16
#define BORDER_HEIGHT	8
#define TOP_LEFT_CORNER	 0
#define TOP_EDGE			4
#define TOP_RIGHT_CORNER	1
#define SIDE_EDGE			5
#define BOTTOM_LEFT_CORNER	2
#define BOTTOM_EDGE		 4
#define BOTTOM_RIGHT_CORNER 3


BOOLEAN DrawBox( UINT32 uiCounter );
BOOLEAN DrawBoxText( UINT32 uiCounter );

void RemoveCurrentBoxPrimaryText( INT32 hStringHandle );
void RemoveCurrentBoxSecondaryText( INT32 hStringHandle );
void RemoveCurrentBoxText( INT32 hStringHandle, UINT8 column );

namespace
{
	std::array<PopUpBoxPt, MAX_POPUP_BOX_COUNT> PopUpBoxList{};
	INT32 guiCurrentBox = -1;

	template <typename Index>
	PopUpBo* GetPopupBox(Index handle)
	{
		if (!UtilsUiStateModel::IsValidIndex(PopUpBoxList.size(), handle)) return nullptr;
		return PopUpBoxList[static_cast<std::size_t>(handle)];
	}

	PopUpBo* GetCurrentPopupBox()
	{
		return GetPopupBox(guiCurrentBox);
	}

	bool IsValidTextLocation(INT32 line, UINT8 column)
	{
		return UtilsUiStateModel::IsValidIndex(MAX_POPUP_BOX_STRING_COUNT, line) &&
			UtilsUiStateModel::IsValidIndex(MAX_POPUP_BOX_COLUMNS, column);
	}

	POPUPSTRINGPTR GetPopupString(PopUpBo* box, INT32 line, UINT8 column)
	{
		if (!box || !IsValidTextLocation(line, column)) return nullptr;
		return box->Text[column][line];
	}

	void DestroyPopupString(POPUPSTRINGPTR& entry)
	{
		if (!entry) return;
		MemFree(entry->pString);
		MemFree(entry);
		entry = nullptr;
	}

	POPUPSTRINGPTR CreatePopupString(STR16 text, BOOLEAN colored)
	{
		if (!text) return nullptr;
		auto* entry = static_cast<POPUPSTRINGPTR>(MemAlloc(sizeof(POPUPSTRING)));
		if (!entry) return nullptr;
		std::memset(entry, 0, sizeof(*entry));
		const std::size_t length = std::wcslen(text);
		entry->pString = static_cast<CHAR16*>(
			MemAlloc((length + 1) * sizeof(CHAR16)));
		if (!entry->pString)
		{
			MemFree(entry);
			return nullptr;
		}
		std::wmemcpy(entry->pString, text, length + 1);
		entry->fColorFlag = colored;
		return entry;
	}
}



void InitPopUpBoxList()
{
	for (auto*& box : PopUpBoxList)
	{
		if (!box) continue;
		for (auto& column : box->Text)
			for (auto*& entry : column) DestroyPopupString(entry);
		MemFree(box);
		box = nullptr;
	}
	guiCurrentBox = -1;
}


void InitPopUpBox( INT32 hBoxHandle )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;
	std::memset(box, 0, sizeof(*box));
}



void SetLineSpace( INT32 hBoxHandle, UINT32 uiLineSpace )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;
	box->uiLineSpace = uiLineSpace;
}


UINT32 GetLineSpace( INT32 hBoxHandle )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return 0;
	// return number of pixels between lines for this box
	return box->uiLineSpace;
}



void SpecifyBoxMinWidth( INT32 hBoxHandle, INT32 iMinWidth )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	box->uiBoxMinWidth = iMinWidth;

	// check if the box is currently too small
	if ( box->Dimensions.iRight < iMinWidth )
	{
		box->Dimensions.iRight = iMinWidth;
	}

	return;
}


BOOLEAN CreatePopUpBox( INT32 *phBoxHandle, SGPRect Dimensions, SGPPoint Position, UINT32 uiFlags )
{
	INT32 iCounter = 0;
	INT32 iCount = 0;
	PopUpBoxPt pBox = NULL;


	if (!phBoxHandle) return FALSE;
	*phBoxHandle = -1;

	// find first free box
	for ( iCounter = 0; (iCounter < MAX_POPUP_BOX_COUNT) && (PopUpBoxList[iCounter] != NULL); iCounter++ );

	if ( iCounter >= MAX_POPUP_BOX_COUNT )
	{
		// ran out of available popup boxes - probably not freeing them up right!
		Assert( 0 );
		return FALSE;
	}

	iCount = iCounter;
	pBox = (PopUpBoxPt)MemAlloc( sizeof( PopUpBo ) );
	if ( pBox == NULL )
	{
		return FALSE;
	}
	PopUpBoxList[iCount] = pBox;
	*phBoxHandle = iCount;

	InitPopUpBox( iCount );
	SetBoxPosition( iCount, Position );
	SetBoxSize( iCount, Dimensions );
	SetBoxFlags( iCount, uiFlags );

	for ( UINT8 col = 0; col < MAX_POPUP_BOX_COLUMNS; ++col )
	{
		for ( iCounter = 0; iCounter < MAX_POPUP_BOX_STRING_COUNT; ++iCounter )
		{
			PopUpBoxList[iCount]->Text[col][iCounter] = NULL;
		}
	}

	SetCurrentBox( iCount );
	SpecifyBoxMinWidth( iCount, 0 );
	SetBoxSecondColumnMinimumOffset( iCount, 0 );
	SetBoxSecondColumnCurrentOffset( iCount, 0 );

	PopUpBoxList[iCount]->fUpdated = FALSE;

	return TRUE;
}


void SetBoxFlags( INT32 hBoxHandle, UINT32 uiFlags )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	box->uiFlags = uiFlags;
	box->fUpdated = FALSE;

	return;
}


void SetMargins( INT32 hBoxHandle, UINT32 uiLeft, UINT32 uiTop, UINT32 uiBottom, UINT32 uiRight )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	box->uiLeftMargin = uiLeft;
	box->uiRightMargin = uiRight;
	box->uiTopMargin = uiTop;
	box->uiBottomMargin = uiBottom;

	box->fUpdated = FALSE;

	return;
}


UINT32 GetTopMarginSize( INT32 hBoxHandle )
{
	// return size of top margin, for mouse region offsets

	auto* box = GetPopupBox(hBoxHandle);
	return box ? box->uiTopMargin : 0;
}


void ShadeStringInBox( INT32 hBoxHandle, INT32 iLineNumber, UINT8 column )
{
	// shade iLineNumber Line in box indexed by hBoxHandle

	auto* box = GetPopupBox(hBoxHandle);
	if (!box || !IsValidTextLocation(iLineNumber, column)) return;

	if ( box->Text[column][iLineNumber] != NULL )
	{
		// set current box
		SetCurrentBox( hBoxHandle );

		// shade line
		box->Text[column][iLineNumber]->fShadeFlag = TRUE;
	}
}

void UnShadeStringInBox( INT32 hBoxHandle, INT32 iLineNumber, UINT8 column )
{
	// unshade iLineNumber in box indexed by hBoxHandle

	auto* box = GetPopupBox(hBoxHandle);
	if (!box || !IsValidTextLocation(iLineNumber, column)) return;

	if ( box->Text[column][iLineNumber] != NULL )
	{
		// set current box
		SetCurrentBox( hBoxHandle );

		// shade line
		box->Text[column][iLineNumber]->fShadeFlag = FALSE;
	}
}


void SecondaryShadeStringInBox( INT32 hBoxHandle, INT32 iLineNumber, UINT8 column )
{
	// shade iLineNumber Line in box indexed by hBoxHandle

	auto* box = GetPopupBox(hBoxHandle);
	if (!box || !IsValidTextLocation(iLineNumber, column)) return;

	if ( box->Text[column][iLineNumber] != NULL )
	{
		// set current box
		SetCurrentBox( hBoxHandle );

		// shade line
		box->Text[column][iLineNumber]->fSecondaryShadeFlag = TRUE;
	}
}

void UnSecondaryShadeStringInBox( INT32 hBoxHandle, INT32 iLineNumber, UINT8 column )
{
	// unshade iLineNumber in box indexed by hBoxHandle

	auto* box = GetPopupBox(hBoxHandle);
	if (!box || !IsValidTextLocation(iLineNumber, column)) return;

	if ( box->Text[column][iLineNumber] != NULL )
	{
		// set current box
		SetCurrentBox( hBoxHandle );

		// shade line
		box->Text[column][iLineNumber]->fSecondaryShadeFlag = FALSE;
	}
}



void SetBoxBuffer( INT32 hBoxHandle, UINT32 uiBuffer )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	box->uiBuffer = uiBuffer;

	box->fUpdated = FALSE;
}


void SetBoxPosition( INT32 hBoxHandle, SGPPoint Position )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	box->Position.iX = Position.iX;
	box->Position.iY = Position.iY;

	box->fUpdated = FALSE;
}


void GetBoxPosition( INT32 hBoxHandle, SGPPoint *Position )
{
	if (!Position) return;
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	Position->iX = box->Position.iX;
	Position->iY = box->Position.iY;
}

void SetBoxSize( INT32 hBoxHandle, SGPRect Dimensions )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	box->Dimensions.iLeft = Dimensions.iLeft;
	box->Dimensions.iBottom = Dimensions.iBottom;
	box->Dimensions.iRight = Dimensions.iRight;
	box->Dimensions.iTop = Dimensions.iTop;

	box->fUpdated = FALSE;
}


void GetBoxSize( INT32 hBoxHandle, SGPRect *Dimensions )
{
	if (!Dimensions) return;
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	Dimensions->iLeft = box->Dimensions.iLeft;
	Dimensions->iBottom = box->Dimensions.iBottom;
	Dimensions->iRight = box->Dimensions.iRight;
	Dimensions->iTop = box->Dimensions.iTop;
}


void SetBorderType( INT32 hBoxHandle, INT32 iBorderObjectIndex )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (box) box->iBorderObjectIndex = iBorderObjectIndex;
}

void SetBackGroundSurface( INT32 hBoxHandle, INT32 iBackGroundSurfaceIndex )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (box) box->iBackGroundSurface = iBackGroundSurfaceIndex;
}


// Adds a string to the first available slot in the column
void AddMonoString( UINT32 *hStringHandle, STR16 pString, UINT8 column )
{
	INT32 iCounter = 0;

	if (!hStringHandle) return;
	*hStringHandle = std::numeric_limits<UINT32>::max();
	auto* box = GetCurrentPopupBox();
	if (!box || !pString || column >= MAX_POPUP_BOX_COLUMNS) return;

	// find first free slot in list
	for ( iCounter = 0; (iCounter < MAX_POPUP_BOX_STRING_COUNT) && (box->Text[column][iCounter] != NULL); iCounter++ );

	if ( iCounter >= MAX_POPUP_BOX_STRING_COUNT )
	{
		// using too many text lines, or not freeing them up properly
		Assert( 0 );
		return;
	}

	auto* entry = CreatePopupString(pString, FALSE);
	if (!entry) return;
	box->Text[column][iCounter] = entry;

	*hStringHandle = iCounter;
	box->fUpdated = FALSE;
	return;
}


// Adds string to the current popup box 2nd column. !!! String's position is the LAST used position in the 1st column !!!
void AddSecondColumnMonoString( UINT32 *hStringHandle, STR16 pString )
{
	INT32 iCounter = 0;

	if (!hStringHandle) return;
	*hStringHandle = std::numeric_limits<UINT32>::max();
	auto* box = GetCurrentPopupBox();
	if (!box || !pString || !box->Text[0][0]) return;

	// find the LAST USED text string index
	for ( iCounter = 0; (iCounter + 1 < MAX_POPUP_BOX_STRING_COUNT) && (box->Text[0][iCounter + 1] != NULL); iCounter++ );

	if ( iCounter >= MAX_POPUP_BOX_STRING_COUNT )
	{
		// using too many text lines, or not freeing them up properly
		Assert( 0 );
		return;
	}

	auto* entry = CreatePopupString(pString, FALSE);
	if (!entry) return;
	DestroyPopupString(box->Text[1][iCounter]);
	box->Text[1][iCounter] = entry;

	*hStringHandle = iCounter;
	box->fUpdated = FALSE;
}


// Adds a COLORED first column string to the CURRENT box
void AddColorString( INT32 *hStringHandle, STR16 pString, UINT8 column )
{
	INT32 iCounter = 0;

	if (!hStringHandle) return;
	*hStringHandle = -1;
	auto* box = GetCurrentPopupBox();
	if (!box || !pString || column >= MAX_POPUP_BOX_COLUMNS) return;

	// find first free slot in list
	for ( iCounter = 0; (iCounter < MAX_POPUP_BOX_STRING_COUNT) && (box->Text[column][iCounter] != NULL); iCounter++ );

	if ( iCounter >= MAX_POPUP_BOX_STRING_COUNT )
	{
		// using too many text lines, or not freeing them up properly
		Assert( 0 );
		return;
	}

	auto* entry = CreatePopupString(pString, TRUE);
	if (!entry) return;
	box->Text[column][iCounter] = entry;

	*hStringHandle = iCounter;

	box->fUpdated = FALSE;

	return;
}



void ResizeBoxForSecondStrings( INT32 hBoxHandle )
{
	INT32 iCounter = 0;
	PopUpBoxPt pBox;
	UINT32 uiBaseWidth, uiThisWidth;


	pBox = GetPopupBox(hBoxHandle);
	if (!pBox) return;

	uiBaseWidth = pBox->uiLeftMargin + pBox->uiSecondColumnMinimunOffset;

	// check string sizes
	for ( iCounter = 0; iCounter < MAX_POPUP_BOX_STRING_COUNT; iCounter++ )
	{
		if ( pBox->Text[0][iCounter] )
		{
			uiThisWidth = uiBaseWidth + StringPixLength( pBox->Text[0][iCounter]->pString, pBox->Text[0][iCounter]->uiFont );

			if ( uiThisWidth > pBox->uiSecondColumnCurrentOffset )
			{
				pBox->uiSecondColumnCurrentOffset = uiThisWidth;
			}
		}
	}
}

static void ResizeStrategicMvtBoxForSecondStrings( INT32 hBoxHandle )
{
	INT32 iCounter = 0;
	PopUpBoxPt pBox;
	UINT32 uiBaseWidth, uiThisWidth;


	pBox = GetPopupBox(hBoxHandle);
	if (!pBox) return;

	// Determine last line for next loop
	INT32 last = MAX_POPUP_BOX_STRING_COUNT;
	for ( iCounter = 1; iCounter < MAX_POPUP_BOX_STRING_COUNT; iCounter++ )
	{
		if ( pBox->Text[0][iCounter] == nullptr )
		{
			// Do not consider the last blank, "cancel" & "Plot Travel route" lines
			last = std::max<INT32>(1, iCounter - 3);
			break;
		}
	}

	uiBaseWidth = pBox->uiLeftMargin + pBox->uiSecondColumnMinimunOffset;

	// check string sizes, skip title line
	for ( iCounter = 1; iCounter < last; iCounter++ )
	{
		if ( pBox->Text[0][iCounter] )
		{
			uiThisWidth = uiBaseWidth + StringPixLength( pBox->Text[0][iCounter]->pString, pBox->Text[0][iCounter]->uiFont );

			if ( uiThisWidth > pBox->uiSecondColumnCurrentOffset )
			{
				// Maintain width if something is selected and the '*' characters have been added
				if ( wcsncmp( pBox->Text[0][iCounter]->pString, L"  *", 3 ) == 0 )
				{
					uiThisWidth -= 10;
				}
				pBox->uiSecondColumnCurrentOffset = uiThisWidth;
			}
		}
	}
}


UINT32 GetNumberOfLinesOfTextInBox( INT32 hBoxHandle, UINT8 column )
{
	INT32 iCounter = 0;

	auto* box = GetPopupBox(hBoxHandle);
	if (!box || column >= MAX_POPUP_BOX_COLUMNS) return 0;

	// count number of lines
	// check string size
	for ( iCounter = 0; iCounter < MAX_POPUP_BOX_STRING_COUNT; iCounter++ )
	{
		if ( box->Text[column][iCounter] == NULL )
		{
			break;
		}
	}

	return(iCounter);
}


UINT32 GetTotalNumberOfLinesOfTextInBox( INT32 hBoxHandle )
{
	UINT32 lines = 0;
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return 0;

	// count number of lines
	// check string size
	for ( size_t i = 0; i < MAX_POPUP_BOX_COLUMNS; i++ )
	{
		for ( INT32 iCounter = 0; iCounter < MAX_POPUP_BOX_STRING_COUNT; iCounter++ )
		{
			if ( box->Text[i][iCounter] == NULL )
			{
				break;
			}
			lines += 1;
		}
	}

	return(lines);
}


void SetBoxFont( INT32 hBoxHandle, UINT32 uiFont )
{
	UINT32 uiCounter;

	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	for ( size_t i = 0; i < MAX_POPUP_BOX_COLUMNS; i++ )
	{
		for ( uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
		{
			if ( box->Text[i][uiCounter] != NULL )
			{
				box->Text[i][uiCounter]->uiFont = uiFont;
			}
		}
	}

	box->fUpdated = FALSE;

	return;
}

void SetBoxSecondColumnMinimumOffset( INT32 hBoxHandle, UINT32 uiWidth )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;
	box->uiSecondColumnMinimunOffset = uiWidth;
	return;
}

void SetBoxSecondColumnCurrentOffset( INT32 hBoxHandle, UINT32 uiCurrentOffset )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;
	box->uiSecondColumnCurrentOffset = uiCurrentOffset;
	return;
}


void SetBoxColumnFont( INT32 hBoxHandle, UINT32 uiFont, UINT8 column )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box || column >= MAX_POPUP_BOX_COLUMNS) return;

	for ( UINT32 uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[column][uiCounter] != NULL )
		{
			box->Text[column][uiCounter]->uiFont = uiFont;
		}
	}

	box->fUpdated = FALSE;
	return;
}



UINT32 GetBoxFont( INT32 hBoxHandle )
{
	auto* entry = GetPopupString(GetPopupBox(hBoxHandle), 0, 0);
	return entry ? entry->uiFont : 0;
}


// set the foreground color of this string in this pop up box
void SetBoxLineForeground( INT32 iBox, INT32 iStringValue, UINT8 ubColor )
{
	auto* entry = GetPopupString(GetPopupBox(iBox), iStringValue, 0);
	if (!entry) return;
	entry->ubForegroundColor = ubColor;
	return;
}

void SetBoxSecondaryShade( INT32 iBox, UINT8 ubColor )
{
	UINT32 uiCounter;

	auto* box = GetPopupBox(iBox);
	if (!box) return;

	for ( uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[0][uiCounter] != NULL )
		{
			box->Text[0][uiCounter]->ubSecondaryShade = ubColor;
		}
	}
	return;
}


// The following functions operate on the CURRENT box
void SetPopUpStringFont( INT32 hStringHandle, UINT32 uiFont )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 0);
	if (entry) entry->uiFont = uiFont;
	return;
}


void SetPopUpSecondColumnStringFont( INT32 hStringHandle, UINT32 uiFont )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 1);
	if (entry) entry->uiFont = uiFont;
	return;
}



void SetStringSecondaryShade( INT32 hStringHandle, UINT8 ubColor )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 0);
	if (entry) entry->ubSecondaryShade = ubColor;
	return;
}

void SetStringForeground( INT32 hStringHandle, UINT8 ubColor )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 0);
	if (entry) entry->ubForegroundColor = ubColor;
	return;
}

void SetStringBackground( INT32 hStringHandle, UINT8 ubColor )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 0);
	if (entry) entry->ubBackgroundColor = ubColor;
	return;
}

void SetStringHighLight( INT32 hStringHandle, UINT8 ubColor )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 0);
	if (entry) entry->ubHighLight = ubColor;
	return;
}


void SetStringShade( INT32 hStringHandle, UINT8 ubShade )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 0);
	if (entry) entry->ubShade = ubShade;
	return;
}

void SetStringSecondColumnForeground( INT32 hStringHandle, UINT8 ubColor )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 1);
	if (entry) entry->ubForegroundColor = ubColor;
	return;
}

void SetStringSecondColumnBackground( INT32 hStringHandle, UINT8 ubColor )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 1);
	if (entry) entry->ubBackgroundColor = ubColor;
	return;
}

void SetStringSecondColumnHighLight( INT32 hStringHandle, UINT8 ubColor )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 1);
	if (entry) entry->ubHighLight = ubColor;
	return;
}


void SetStringSecondColumnShade( INT32 hStringHandle, UINT8 ubShade )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 1);
	if (entry) entry->ubShade = ubShade;
	return;
}



void SetBoxForeground( INT32 hBoxHandle, UINT8 ubColor )
{
	UINT32 uiCounter;
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	for ( uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[0][uiCounter] != NULL )
		{
			box->Text[0][uiCounter]->ubForegroundColor = ubColor;
		}
	}
	return;
}

void SetBoxBackground( INT32 hBoxHandle, UINT8 ubColor )
{
	UINT32 uiCounter;
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	for ( uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[0][uiCounter] != NULL )
		{
			box->Text[0][uiCounter]->ubBackgroundColor = ubColor;
		}
	}
	return;
}

void SetBoxHighLight( INT32 hBoxHandle, UINT8 ubColor )
{
	UINT32 uiCounter;
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	for ( uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[0][uiCounter] != NULL )
		{
			box->Text[0][uiCounter]->ubHighLight = ubColor;
		}
	}
	return;
}

void SetBoxShade( INT32 hBoxHandle, UINT8 ubColor )
{
	UINT32 uiCounter;
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	for ( uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[0][uiCounter] != NULL )
		{
			box->Text[0][uiCounter]->ubShade = ubColor;
		}
	}
	return;
}




void SetBoxColumnForeground( INT32 hBoxHandle, UINT8 ubColor, UINT8 column )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box || column >= MAX_POPUP_BOX_COLUMNS) return;

	for ( UINT32 uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[column][uiCounter] != NULL )
		{
			box->Text[column][uiCounter]->ubForegroundColor = ubColor;
		}
	}
	return;
}

void SetBoxColumnBackground( INT32 hBoxHandle, UINT8 ubColor, UINT8 column )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box || column >= MAX_POPUP_BOX_COLUMNS) return;

	for ( UINT32 uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[column][uiCounter] != NULL )
		{
			box->Text[column][uiCounter]->ubBackgroundColor = ubColor;
		}
	}
	return;
}

void SetBoxColumnHighLight( INT32 hBoxHandle, UINT8 ubColor, UINT8 column )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box || column >= MAX_POPUP_BOX_COLUMNS) return;

	for ( UINT32 uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[column][uiCounter] != NULL )
		{
			box->Text[column][uiCounter]->ubHighLight = ubColor;
		}
	}
	return;
}

void SetBoxColumnShade( INT32 hBoxHandle, UINT8 ubColor, UINT8 column )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box || column >= MAX_POPUP_BOX_COLUMNS) return;

	for ( UINT32 uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
	{
		if ( box->Text[column][uiCounter] != NULL )
		{
			box->Text[column][uiCounter]->ubShade = ubColor;
		}
	}
	return;
}



void HighLightLine( INT32 hStringHandle, UINT8 column )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, column);
	if (entry) entry->fHighLightFlag = TRUE;
	return;
}


BOOLEAN GetShadeFlag( INT32 hStringHandle )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 0);
	return entry ? entry->fShadeFlag : FALSE;
}

BOOLEAN GetSecondaryShadeFlag( INT32 hStringHandle )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 0);
	return entry ? entry->fSecondaryShadeFlag : FALSE;
}


void HighLightBoxLine( INT32 hBoxHandle, INT32 iLineNumber, UINT8 column )
{
	auto* entry = GetPopupString(GetPopupBox(hBoxHandle), iLineNumber, column);
	if (entry)
	{
		// set current box
		SetCurrentBox( hBoxHandle );

		// highlight line
		HighLightLine( iLineNumber, column );
	}

	return;
}


BOOLEAN GetBoxShadeFlag( INT32 hBoxHandle, INT32 iLineNumber )
{
	auto* entry = GetPopupString(GetPopupBox(hBoxHandle), iLineNumber, 0);
	return entry ? entry->fShadeFlag : FALSE;
}

BOOLEAN GetBoxSecondaryShadeFlag( INT32 hBoxHandle, INT32 iLineNumber )
{
	auto* entry = GetPopupString(GetPopupBox(hBoxHandle), iLineNumber, 0);
	return entry ? entry->fSecondaryShadeFlag : FALSE;
}

void UnHighLightLine( INT32 hStringHandle, UINT8 column )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, column);
	if (entry) entry->fHighLightFlag = FALSE;
	return;
}

void UnHighLightBox( INT32 hBoxHandle )
{
	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;
	for ( size_t i = 0; i < MAX_POPUP_BOX_COLUMNS; i++ )
	{
		for ( INT32 iCounter = 0; iCounter < MAX_POPUP_BOX_STRING_COUNT; iCounter++ )
		{
			if ( box->Text[i][iCounter] )
				box->Text[i][iCounter]->fHighLightFlag = FALSE;
		}
	}
}

void UnHighLightSecondColumnLine( INT32 hStringHandle )
{
	auto* entry = GetPopupString(GetCurrentPopupBox(), hStringHandle, 1);
	if (entry) entry->fHighLightFlag = FALSE;
	return;
}

void UnHighLightSecondColumnBox( INT32 hBoxHandle )
{
	INT32 iCounter = 0;

	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	for ( iCounter = 0; iCounter < MAX_POPUP_BOX_STRING_COUNT; iCounter++ )
	{
		if ( box->Text[1][iCounter] )
			box->Text[1][iCounter]->fHighLightFlag = FALSE;
	}
}

void RemoveOneCurrentBoxString( INT32 hStringHandle, BOOLEAN fFillGaps )
{
	UINT32 uiCounter = 0;

	auto* box = GetCurrentPopupBox();
	if (!box || !UtilsUiStateModel::IsValidIndex(
		MAX_POPUP_BOX_STRING_COUNT, hStringHandle)) return;

	RemoveCurrentBoxPrimaryText( hStringHandle );
	RemoveCurrentBoxSecondaryText( hStringHandle );

	if ( fFillGaps )
	{
		// shuffle all strings down a slot to fill in the gap
		for ( uiCounter = hStringHandle; uiCounter < (MAX_POPUP_BOX_STRING_COUNT - 1); uiCounter++ )
		{
			box->Text[0][uiCounter] = box->Text[0][uiCounter + 1];
			box->Text[1][uiCounter] = box->Text[1][uiCounter + 1];
		}
		box->Text[0][MAX_POPUP_BOX_STRING_COUNT - 1] = nullptr;
		box->Text[1][MAX_POPUP_BOX_STRING_COUNT - 1] = nullptr;
	}

	box->fUpdated = FALSE;
}


void RemoveAllCurrentBoxStrings( void )
{
	if (!GetCurrentPopupBox()) return;

	for ( size_t i = 0; i < MAX_POPUP_BOX_COLUMNS; i++ )
	{
		for ( UINT32 uiCounter = 0; uiCounter < MAX_POPUP_BOX_STRING_COUNT; uiCounter++ )
		{
			RemoveCurrentBoxText( uiCounter, i );
		}
	}
}


void RemoveBox( INT32 hBoxHandle )
{
	INT32 hOldBoxHandle;

	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	GetCurrentBox( &hOldBoxHandle );
	SetCurrentBox( hBoxHandle );

	RemoveAllCurrentBoxStrings();

	MemFree(box);
	PopUpBoxList[hBoxHandle] = nullptr;

	if (hOldBoxHandle != hBoxHandle && GetPopupBox(hOldBoxHandle))
		SetCurrentBox( hOldBoxHandle );
	else
		guiCurrentBox = -1;

	return;
}



void ShowBox( INT32 hBoxHandle )
{
	if (auto* box = GetPopupBox(hBoxHandle))
	{
		if ( box->fShowBox == FALSE )
		{
			box->fShowBox = TRUE;
			box->fUpdated = FALSE;
		}
	}
}

void HideBox( INT32 hBoxHandle )
{
	if (auto* box = GetPopupBox(hBoxHandle))
	{
		if ( box->fShowBox == TRUE )
		{
			box->fShowBox = FALSE;
			box->fUpdated = FALSE;
		}
	}
}



void SetCurrentBox( INT32 hBoxHandle )
{
	if (GetPopupBox(hBoxHandle)) guiCurrentBox = hBoxHandle;
}


void GetCurrentBox( INT32 *hBoxHandle )
{
	if (hBoxHandle) *hBoxHandle = guiCurrentBox;
}



void DisplayBoxes( UINT32 uiBuffer )
{
	UINT32 uiCounter;

	for ( uiCounter = 0; uiCounter < MAX_POPUP_BOX_COUNT; uiCounter++ )
	{
		DisplayOnePopupBox( uiCounter, uiBuffer );
	}
	return;
}


void DisplayOnePopupBox( UINT32 uiIndex, UINT32 uiBuffer )
{
	if (auto* box = GetPopupBox(uiIndex))
	{
		if ( (box->uiBuffer == uiBuffer) && box->fShowBox )
		{
			DrawBox( uiIndex );
			DrawBoxText( uiIndex );
		}
	}
}



// force an update of this box
void ForceUpDateOfBox( UINT32 uiIndex )
{
	if (auto* box = GetPopupBox(uiIndex)) box->fUpdated = FALSE;
}



BOOLEAN DrawBox( UINT32 uiCounter )
{
	// will build pop up box in usTopX, usTopY with dimensions usWidth and usHeight
	UINT32 uiNumTilesWide;
	UINT32 uiNumTilesHigh;
	UINT32 uiCount = 0;
	HVOBJECT hBoxHandle;
	HVSURFACE hSrcVSurface;
	UINT32 uiDestPitchBYTES;
	UINT32 uiSrcPitchBYTES;
	PIXEL *pDestBuf;
	UINT8 *pSrcBuf;
	SGPRect clip;
	UINT16 usTopX, usTopY;
	UINT16 usWidth, usHeight;


	auto* box = GetPopupBox(uiCounter);
	if (!box) return FALSE;

	// only update if we need to

	if ( box->fUpdated == TRUE )
	{
		return(FALSE);
	}

	if ( box->uiFlags & POPUP_BOX_FLAG_RESIZE )
	{
		ResizeBoxToText( uiCounter );
	}

	INT32 width = box->Dimensions.iRight - box->Dimensions.iLeft;
	INT32 height = box->Dimensions.iBottom - box->Dimensions.iTop;

	// check if we have a min width, if so then update box for such
	if ( box->uiBoxMinWidth && width < static_cast<INT32>(box->uiBoxMinWidth) )
	{
		width = box->uiBoxMinWidth;
	}

	// Four two-pixel corners are required, and all arithmetic below assumes that
	// the complete box fits on screen.
	if (width < 4 || height < 4 || width > SCREEN_WIDTH || height > SCREEN_HEIGHT)
	{
		return FALSE;
	}
	const INT32 topX = std::clamp<INT32>(box->Position.iX, 0, SCREEN_WIDTH - width);
	const INT32 topY = std::clamp<INT32>(box->Position.iY, 0, SCREEN_HEIGHT - height);
	box->Position.iX = topX;
	box->Position.iY = topY;
	usTopX = static_cast<UINT16>(topX);
	usTopY = static_cast<UINT16>(topY);
	usWidth = static_cast<UINT16>(width);
	usHeight = static_cast<UINT16>(height);

	// subtract 4 because the 2 2-pixel corners are handled separately
	uiNumTilesWide = ((usWidth - 4) / BORDER_WIDTH);
	uiNumTilesHigh = ((usHeight - 4) / BORDER_HEIGHT);

	clip.iLeft = 0;
	clip.iRight = clip.iLeft + usWidth;
	clip.iTop = 0;
	clip.iBottom = clip.iTop + usHeight;

	// blit in texture first, then borders
	// blit in surface
	pDestBuf = (PIXEL *)LockVideoSurface( box->uiBuffer, &uiDestPitchBYTES );
	if (!pDestBuf) return FALSE;
	if (!GetVideoSurface( &hSrcVSurface, box->iBackGroundSurface ))
	{
		UnLockVideoSurface(box->uiBuffer);
		return FALSE;
	}
	pSrcBuf = LockVideoSurface( box->iBackGroundSurface, &uiSrcPitchBYTES );
	if (!pSrcBuf)
	{
		UnLockVideoSurface(box->uiBuffer);
		return FALSE;
	}
	Blt8BPPDataSubTo16BPPBuffer( pDestBuf, uiDestPitchBYTES, hSrcVSurface, pSrcBuf, uiSrcPitchBYTES, usTopX, usTopY, &clip );
	UnLockVideoSurface( box->iBackGroundSurface );
	UnLockVideoSurface( box->uiBuffer );
	if (!GetVideoObject( &hBoxHandle, box->iBorderObjectIndex )) return FALSE;

	// blit in 4 corners (they're 2x2 pixels)
	BltVideoObject( box->uiBuffer, hBoxHandle, TOP_LEFT_CORNER, usTopX, usTopY, VO_BLT_SRCTRANSPARENCY, NULL );
	BltVideoObject( box->uiBuffer, hBoxHandle, TOP_RIGHT_CORNER, usTopX + usWidth - 2, usTopY, VO_BLT_SRCTRANSPARENCY, NULL );
	BltVideoObject( box->uiBuffer, hBoxHandle, BOTTOM_RIGHT_CORNER, usTopX + usWidth - 2, usTopY + usHeight - 2, VO_BLT_SRCTRANSPARENCY, NULL );
	BltVideoObject( box->uiBuffer, hBoxHandle, BOTTOM_LEFT_CORNER, usTopX, usTopY + usHeight - 2, VO_BLT_SRCTRANSPARENCY, NULL );

	// blit in edges
	if ( uiNumTilesWide > 0 )
	{
		// full pieces
		for ( uiCount = 0; uiCount < uiNumTilesWide; uiCount++ )
		{
			BltVideoObject( box->uiBuffer, hBoxHandle, TOP_EDGE, usTopX + 2 + (uiCount * BORDER_WIDTH), usTopY, VO_BLT_SRCTRANSPARENCY, NULL );
			BltVideoObject( box->uiBuffer, hBoxHandle, BOTTOM_EDGE, usTopX + 2 + (uiCount * BORDER_WIDTH), usTopY + usHeight - 2, VO_BLT_SRCTRANSPARENCY, NULL );
		}

		// partial pieces
		BltVideoObject( box->uiBuffer, hBoxHandle, TOP_EDGE, usTopX + usWidth - 2 - BORDER_WIDTH, usTopY, VO_BLT_SRCTRANSPARENCY, NULL );
		BltVideoObject( box->uiBuffer, hBoxHandle, BOTTOM_EDGE, usTopX + usWidth - 2 - BORDER_WIDTH, usTopY + usHeight - 2, VO_BLT_SRCTRANSPARENCY, NULL );
	}
	if ( uiNumTilesHigh > 0 )
	{
		// full pieces
		for ( uiCount = 0; uiCount < uiNumTilesHigh; uiCount++ )
		{
			BltVideoObject( box->uiBuffer, hBoxHandle, SIDE_EDGE, usTopX, usTopY + 2 + (uiCount * BORDER_HEIGHT), VO_BLT_SRCTRANSPARENCY, NULL );
			BltVideoObject( box->uiBuffer, hBoxHandle, SIDE_EDGE, usTopX + usWidth - 2, usTopY + 2 + (uiCount * BORDER_HEIGHT), VO_BLT_SRCTRANSPARENCY, NULL );
		}

		// partial pieces
		BltVideoObject( box->uiBuffer, hBoxHandle, SIDE_EDGE, usTopX, usTopY + usHeight - 2 - BORDER_HEIGHT, VO_BLT_SRCTRANSPARENCY, NULL );
		BltVideoObject( box->uiBuffer, hBoxHandle, SIDE_EDGE, usTopX + usWidth - 2, usTopY + usHeight - 2 - BORDER_HEIGHT, VO_BLT_SRCTRANSPARENCY, NULL );
	}

	InvalidateRegion( usTopX, usTopY, usTopX + usWidth, usTopY + usHeight );
	box->fUpdated = TRUE;
	return TRUE;
}



BOOLEAN DrawBoxText( UINT32 uiCounter )
{
	UINT32 uiCount = 0;
	INT16 uX, uY;


	auto* box = GetPopupBox(uiCounter);
	if (!box) return FALSE;

	//clip text?
	if ( box->uiFlags & POPUP_BOX_FLAG_CLIP_TEXT )
	{
		const auto x1 = box->Position.iX + box->uiLeftMargin - 1;
		const auto y1 = box->Position.iY + box->uiTopMargin;
		const auto x2 = box->Position.iX + box->Dimensions.iRight - box->uiRightMargin;
		const auto y2 = box->Position.iY + box->Dimensions.iBottom - box->uiBottomMargin;
		SetFontDestBuffer( box->uiBuffer, x1, y1, x2, y2, FALSE );
	}

	for ( size_t i = 0; i < MAX_POPUP_BOX_COLUMNS; i++ )
	{
		for ( uiCount = 0; uiCount < MAX_POPUP_BOX_STRING_COUNT; uiCount++ )
		{
			auto entry = box->Text[i][uiCount];
			// there is text in this line?
			if ( entry && entry->pString )
			{
				// set font
				SetFont( entry->uiFont );

				// are we highlighting?...shading?..or neither
				if ( (entry->fHighLightFlag == FALSE) && (entry->fShadeFlag == FALSE) && (entry->fSecondaryShadeFlag == FALSE) )
				{
					// neither
					SetFontForeground( entry->ubForegroundColor );
				}
				else if ( entry->fHighLightFlag == TRUE )
				{
					// highlight
					SetFontForeground( entry->ubHighLight );
				}
				else if ( entry->fSecondaryShadeFlag == TRUE )
				{
					SetFontForeground( entry->ubSecondaryShade );
				}
				else
				{
					//shading
					SetFontForeground( entry->ubShade );
				}

				// set background
				SetFontBackground( entry->ubBackgroundColor );

				// centering?
				if ( box->uiFlags & POPUP_BOX_FLAG_CENTER_TEXT )
				{
					INT16 sLeft = box->Position.iX + box->uiLeftMargin;
					INT16 sTop = box->Position.iY + uiCount * GetFontHeight( entry->uiFont ) + box->uiTopMargin + uiCount * box->uiLineSpace;
					INT16 sWidth = box->Dimensions.iRight - (box->uiRightMargin + box->uiLeftMargin + 2);
					INT16 sHeight = GetFontHeight( entry->uiFont );
					FindFontCenterCoordinates( sLeft, sTop, sWidth, sHeight, entry->pString, ((INT32)entry->uiFont), &uX, &uY );
				}
				else
				{
					uX = box->Position.iX + box->uiLeftMargin + box->uiSecondColumnCurrentOffset * i;
					uY = ((INT16)(box->Position.iY + uiCount * GetFontHeight( entry->uiFont ) + box->uiTopMargin + uiCount * box->uiLineSpace));
				}

				// print
				mprintf( uX, uY, entry->pString );
			}
		}
	}


	if ( box->uiBuffer != guiSAVEBUFFER )
	{
		InvalidateRegion( box->Position.iX + box->uiLeftMargin - 1, box->Position.iY + box->uiTopMargin, box->Position.iX + box->Dimensions.iRight - box->uiRightMargin, box->Position.iY + box->Dimensions.iBottom - box->uiBottomMargin );
	}

	SetFontDestBuffer( FRAME_BUFFER, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FALSE );

	return TRUE;
}


void ResizeBoxToText( INT32 hBoxHandle )
{
	// run through lines of text in box and size box width to longest line plus margins
	// height is sum of getfontheight of each line+ spacing
	INT32 iWidth = 0;
	INT32 iHeight = 0;
	INT32 iCurrString = 0;
	INT32 iColumnLength = 0;


	auto* box = GetPopupBox(hBoxHandle);
	if (!box) return;

	if ( hBoxHandle == ghMoveBox )
	{
		ResizeStrategicMvtBoxForSecondStrings( hBoxHandle );
	}
	else
	{
		ResizeBoxForSecondStrings( hBoxHandle );
	}

	const UINT32 margins = box->uiLeftMargin + box->uiRightMargin;
	iHeight = box->uiTopMargin + box->uiBottomMargin;
	const auto columnOffset = box->uiSecondColumnCurrentOffset;

	for ( iCurrString = 0; iCurrString < MAX_POPUP_BOX_STRING_COUNT; iCurrString++ )
	{
		if ( box->Text[0][iCurrString] != NULL )
		{

			if ( box->Text[3][iCurrString] != NULL && wcscmp( box->Text[3][iCurrString]->pString, L"" ) != 0 )
			{
				iColumnLength = StringPixLength( box->Text[3][iCurrString]->pString, box->Text[3][iCurrString]->uiFont );
				if ( 3 * columnOffset + iColumnLength + margins > iWidth )
				{
					iWidth = 3 * columnOffset + iColumnLength + margins;
				}
			}
			else if ( box->Text[2][iCurrString] != NULL && wcscmp( box->Text[2][iCurrString]->pString, L"" ) != 0 )
			{
				iColumnLength = StringPixLength( box->Text[2][iCurrString]->pString, box->Text[2][iCurrString]->uiFont );
				if ( 2 * columnOffset + iColumnLength + margins > iWidth )
				{
					iWidth = 2 * columnOffset + iColumnLength + margins;
				}
			}
			else if ( box->Text[1][iCurrString] != NULL && wcscmp( box->Text[1][iCurrString]->pString, L"" ) != 0 )
			{
				iColumnLength = StringPixLength( box->Text[1][iCurrString]->pString, box->Text[1][iCurrString]->uiFont );
				if ( columnOffset + iColumnLength + margins > iWidth )
				{
					iWidth = columnOffset + iColumnLength + margins;
				}
			}


			INT32 iFirstColumnLength = StringPixLength( box->Text[0][iCurrString]->pString, box->Text[0][iCurrString]->uiFont ) + margins;
			if ( iFirstColumnLength > iWidth )
				iWidth = iFirstColumnLength;

			//vertical
			iHeight += GetFontHeight( box->Text[0][iCurrString]->uiFont ) + box->uiLineSpace;
		}
		else
		{
			// doesn't support gaps in text array...
			break;
		}
	}

	// Flugente we shouldn't have added more popup options than we can display anyway, but I have no idea where to stop that, so at least we can fix this
	const INT32 availableHeight = std::max<INT32>(0, SCREEN_HEIGHT - box->Position.iY);
	const INT32 availableWidth = std::max<INT32>(0, SCREEN_WIDTH - box->Position.iX);
	box->Dimensions.iBottom = min(iHeight, availableHeight);
	box->Dimensions.iRight = min(iWidth, availableWidth);

	// Constrain popup box height to background graphics max height. Otherwise we get blue graphics glitches
	const auto popupBoxHeight = box->Dimensions.iBottom - box->Dimensions.iTop;
	if ( popupBoxHeight > 480 )
	{
		box->Dimensions.iBottom = box->Dimensions.iTop + 480;
	}
}


BOOLEAN IsBoxShown( UINT32 uiHandle )
{
	auto* box = GetPopupBox(uiHandle);
	return box ? box->fShowBox : FALSE;
}


void MarkAllBoxesAsAltered( void )
{
	INT32 iCounter = 0;

	// mark all boxes as altered
	for ( iCounter = 0; iCounter < MAX_POPUP_BOX_COUNT; iCounter++ )
	{
		ForceUpDateOfBox( iCounter );
	}

	return;
}


void HideAllBoxes( void )
{
	INT32 iCounter = 0;

	// hide all the boxes that are shown
	for ( iCounter = 0; iCounter < MAX_POPUP_BOX_COUNT; iCounter++ )
	{
		HideBox( iCounter );
	}
}


void RemoveCurrentBoxPrimaryText( INT32 hStringHandle )
{
	auto* box = GetCurrentPopupBox();
	if (!box || !IsValidTextLocation(hStringHandle, 0)) return;
	DestroyPopupString(box->Text[0][hStringHandle]);
}

void RemoveCurrentBoxSecondaryText( INT32 hStringHandle )
{
	auto* box = GetCurrentPopupBox();
	if (!box || !IsValidTextLocation(hStringHandle, 1)) return;
	DestroyPopupString(box->Text[1][hStringHandle]);
}

void RemoveCurrentBoxText( INT32 hStringHandle, UINT8 column )
{
	auto* box = GetCurrentPopupBox();
	if (!box || !IsValidTextLocation(hStringHandle, column)) return;
	DestroyPopupString(box->Text[column][hStringHandle]);
}

UINT32 GetBoxSecondColumnCurrentOffset( INT32 hBoxHandle )
{
	auto* box = GetPopupBox(hBoxHandle);
	return box ? box->uiSecondColumnCurrentOffset : 0;
}
