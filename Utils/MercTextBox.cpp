	#include "MercTextBox.h"
	#include "WCheck.h"
	#include "renderworld.h"
	#include "Font Control.h"
	#include "Utilities.h"
	#include "WordWrap.h"
	#include "vobject_blitters.h"
	#include "message.h"
	#include "UtilsUiStateModel.h"

#include <array>
#include <cstring>


#define		TEXT_POPUP_WINDOW_TEXT_OFFSET_X		8
#define		TEXT_POPUP_WINDOW_TEXT_OFFSET_Y		8
#define		TEXT_POPUP_STRING_WIDTH						296
#define		TEXT_POPUP_GAP_BN_LINES						10
#define		TEXT_POPUP_FONT										FONT12ARIAL
#define		TEXT_POPUP_COLOR									FONT_MCOLOR_WHITE

#define		MERC_TEXT_FONT										FONT12ARIAL
#define		MERC_TEXT_COLOR										FONT_MCOLOR_WHITE

#define		MERC_TEXT_MIN_WIDTH								10
#define		MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_X		10
#define		MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_Y		10

#define		MERC_BACKGROUND_WIDTH										350
#define		MERC_BACKGROUND_HEIGHT									207

// the max number of pop up boxes available to user
#define MAX_NUMBER_OF_POPUP_BOXES 10

// attempt to add box to pop up box list
INT32 AddPopUpBoxToList( MercPopUpBox *pPopUpTextBox );


// grab box with this id value
MercPopUpBox * GetPopUpBoxIndex( INT32 iId );


// both of the below are index by the enum for thier types - background and border in
// MercTextBox.h

// filenames for border popup .sti's
STR8 zMercBorderPopupFilenames[ ] = {
 "INTERFACE\\TactPopUp.sti",
 "INTERFACE\\TactRedPopUp.sti",
 "INTERFACE\\TactBluePopUp.sti",
 "INTERFACE\\TactPopUpMain.sti",
 "INTERFACE\\LaptopPopup.sti",
};

// filenames for background popup .pcx's
STR8 zMercBackgroundPopupFilenames[ ] = {
	"INTERFACE\\TactPopupBackground.pcx",
	"INTERFACE\\TactPopupWhiteBackground.pcx",
	"INTERFACE\\TactPopupGreyBackground.pcx",
	"INTERFACE\\TactPopupBackgroundMain.pcx",
	"INTERFACE\\LaptopPopupBackground.pcx",
	"INTERFACE\\imp_popup_background.pcx",
};


// the pop up box structure
MercPopUpBox	gBasicPopUpTextBox;

// the current pop up box
MercPopUpBox	*gPopUpTextBox = NULL;

// the old one
MercPopUpBox	*gOldPopUpTextBox = NULL;


// the list of boxes
MercPopUpBox *gpPopUpBoxList[ MAX_NUMBER_OF_POPUP_BOXES ];

// the flags
UINT32	guiFlags = 0;
UINT32	guiBoxIcons;
UINT32	guiSkullIcons;
BOOLEAN gMercPopupSystemInitialized = FALSE;

BOOLEAN SetCurrentPopUpBox( UINT32 uiId )
{
	// given id of the box, find it in the list and set to current

	//make sure the box id is valid
	if (!UtilsUiStateModel::IsValidIndex(MAX_NUMBER_OF_POPUP_BOXES, uiId)) return FALSE;

	// see if box inited
	if( gpPopUpBoxList[ uiId ] != NULL )
	{
		gPopUpTextBox = gpPopUpBoxList[ uiId ];
		return( TRUE );
	}
	return ( FALSE );
}

BOOLEAN OverrideMercPopupBox( MercPopUpBox *pMercBox )
{
	// store old box and set current this passed one
	gOldPopUpTextBox = gPopUpTextBox;

	gPopUpTextBox = pMercBox;

	return( TRUE );
}

BOOLEAN ResetOverrideMercPopupBox( )
{
	gPopUpTextBox = gOldPopUpTextBox;

	return( TRUE );
}


BOOLEAN InitMercPopupBox( )
{
	INT32 iCounter = 0;
	VOBJECT_DESC	VObjectDesc;

	if (gMercPopupSystemInitialized) return TRUE;

	// init the pop up box list
	for( iCounter = 0; iCounter < MAX_NUMBER_OF_POPUP_BOXES; iCounter++ )
	{
		// set ptr to null
		gpPopUpBoxList[ iCounter ] = NULL;
	}

	// LOAD STOP ICON...
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("INTERFACE\\msgboxicons.sti", VObjectDesc.ImageFile);
	if( !AddVideoObject( &VObjectDesc, &guiBoxIcons ) ) return FALSE;

	// LOAD SKULL ICON...
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP("INTERFACE\\msgboxiconskull.sti", VObjectDesc.ImageFile);
	if( !AddVideoObject( &VObjectDesc, &guiSkullIcons ) )
	{
		DeleteVideoObjectFromIndex(guiBoxIcons);
		return FALSE;
	}
	gMercPopupSystemInitialized = TRUE;
	
	return( TRUE );
}


BOOLEAN ShutDownPopUpBoxes( )
{
	if (!gMercPopupSystemInitialized) return TRUE;
	INT32 iCounter = 0;
	for( iCounter = 0; iCounter < MAX_NUMBER_OF_POPUP_BOXES ; ++iCounter )
	{
		// now attempt to remove this box
		RemoveMercPopupBoxFromIndex( iCounter );
	}
	DeleteVideoObjectFromIndex(guiBoxIcons);
	DeleteVideoObjectFromIndex(guiSkullIcons);
	gPopUpTextBox = NULL;
	gOldPopUpTextBox = NULL;
	guiFlags = 0;
	gMercPopupSystemInitialized = FALSE;

	return( TRUE );
}


//Pass in the background index, and pointers to the font and shadow color
void	GetMercPopupBoxFontColor( UINT8 ubBackgroundIndex, UINT8 *pubFontColor, UINT8 *pubFontShadowColor);

// Tactical Popup
BOOLEAN LoadTextMercPopupImages( UINT8 ubBackgroundIndex, UINT8 ubBorderIndex)
{
	VSURFACE_DESC		vs_desc;
	VOBJECT_DESC	VObjectDesc;

	if (!gPopUpTextBox ||
		!UtilsUiStateModel::IsValidIndex(std::size(zMercBackgroundPopupFilenames), ubBackgroundIndex) ||
		!UtilsUiStateModel::IsValidIndex(std::size(zMercBorderPopupFilenames), ubBorderIndex))
		return FALSE;

	UINT32 background = 0;
	UINT32 border = 0;
	// this function will load the graphics associated with the background and border index values

	// the background
	vs_desc.fCreateFlags = VSURFACE_CREATE_FROMFILE | VSURFACE_SYSTEM_MEM_USAGE;
	strcpy(vs_desc.ImageFile,	zMercBackgroundPopupFilenames [ ubBackgroundIndex ]);
	if (!AddVideoSurface(&vs_desc, &background)) return FALSE;

	// border
	VObjectDesc.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	FilenameForBPP( zMercBorderPopupFilenames[ ubBorderIndex ], VObjectDesc.ImageFile );
	if (!AddVideoObject(&VObjectDesc, &border))
	{
		DeleteVideoSurfaceFromIndex(background);
		return FALSE;
	}
	RemoveTextMercPopupImages();
	gPopUpTextBox->uiMercTextPopUpBackground = background;
	gPopUpTextBox->uiMercTextPopUpBorder = border;

	gPopUpTextBox->fMercTextPopupInitialized = TRUE;

	// so far so good, return successful
	gPopUpTextBox->ubBackgroundIndex = ubBackgroundIndex;
	gPopUpTextBox->ubBorderIndex			= ubBorderIndex;

	return( TRUE );
}

void RemoveTextMercPopupImages( )
{
	//this procedure will remove the background and border video surface/object from the indecies
	if( gPopUpTextBox )
	{
		if( gPopUpTextBox->fMercTextPopupInitialized )
		{
			// the background
			DeleteVideoSurfaceFromIndex( gPopUpTextBox->uiMercTextPopUpBackground );

			// the border
			DeleteVideoObjectFromIndex( gPopUpTextBox->uiMercTextPopUpBorder );

			gPopUpTextBox->fMercTextPopupInitialized = FALSE;
		}
	}
}

BOOLEAN RenderMercPopUpBoxFromIndex( INT32 iBoxId, INT16 sDestX, INT16 sDestY, UINT32 uiBuffer )
{
	// set the current box
	if( SetCurrentPopUpBox( iBoxId ) == FALSE )
	{
		return ( FALSE );
	}

	// now attempt to render the box
	return( RenderMercPopupBox( sDestX,	sDestY,	uiBuffer ) );
}

BOOLEAN RenderMercPopupBox(INT16 sDestX, INT16 sDestY, UINT32 uiBuffer )
{
//	UINT32	uiDestPitchBYTES;
//	UINT32	uiSrcPitchBYTES;
//	PIXEL	*pDestBuf;
//	PIXEL	*pSrcBuf;
	
	// will render/transfer the image from the buffer in the data structure to the buffer specified by user
	if (!gPopUpTextBox || !gPopUpTextBox->fMercTextPopupSurfaceInitialized)
		return FALSE;
	BOOLEAN fReturnValue = TRUE;

	// grab the destination buffer
//	pDestBuf = ( UINT16* )LockVideoSurface( uiBuffer, &uiDestPitchBYTES );

	// now lock it
//	pSrcBuf = ( UINT16* )LockVideoSurface( gPopUpTextBox->uiSourceBufferIndex, &uiSrcPitchBYTES);
	
	//check to see if we are wanting to blit a transparent background
	if ( gPopUpTextBox->uiFlags & MERC_POPUP_PREPARE_FLAGS_TRANS_BACK )
		BltVideoSurface( uiBuffer, gPopUpTextBox->uiSourceBufferIndex, 0, sDestX, sDestY, VS_BLT_FAST | VS_BLT_USECOLORKEY, NULL );
	else
		BltVideoSurface( uiBuffer, gPopUpTextBox->uiSourceBufferIndex, 0, sDestX, sDestY, VS_BLT_FAST, NULL );
	
	// blt, and grab return value
//	fReturnValue = Blt16BPPTo16BPP(pDestBuf, uiDestPitchBYTES, pSrcBuf, uiSrcPitchBYTES, sDestX, sDestY, 0, 0, gPopUpTextBox->sWidth, gPopUpTextBox->sHeight);

	//Invalidate!
	if ( uiBuffer == FRAME_BUFFER )
	{
		InvalidateRegion( sDestX, sDestY, (INT16)( sDestX + gPopUpTextBox->sWidth ), (INT16)( sDestY + gPopUpTextBox->sHeight ) );
	}

	// unlock the video surfaces

	// source
//	UnLockVideoSurface( gPopUpTextBox->uiSourceBufferIndex );

	// destination
//	UnLockVideoSurface( uiBuffer );

	// return success or failure
	return fReturnValue;
}



INT32 AddPopUpBoxToList( MercPopUpBox *pPopUpTextBox )
{
	INT32 iCounter = 0;

	// make sure is a valid box
	if( pPopUpTextBox == NULL )
	{
		return ( -1 );
	}

	// attempt to add box to list
	for( iCounter = 0; iCounter < MAX_NUMBER_OF_POPUP_BOXES; ++iCounter )
	{
		if( gpPopUpBoxList[ iCounter ] == NULL )
		{
			// found a spot, inset
			gpPopUpBoxList[ iCounter ] = pPopUpTextBox;

			// set as current
			SetCurrentPopUpBox( iCounter );

			// return index value
			return( iCounter );
		}
	}

	// return failure
	return( -1 );
}

// get box with this id
MercPopUpBox * GetPopUpBoxIndex( INT32 iId )
{
	if (!UtilsUiStateModel::IsValidIndex(MAX_NUMBER_OF_POPUP_BOXES, iId)) return NULL;
	return gpPopUpBoxList[iId];
}

INT32 PrepareMercPopupBox(	INT32 iBoxId, UINT8 ubBackgroundIndex, UINT8 ubBorderIndex, STR16 pString,
							UINT16 usWidth, UINT16 usMarginX, UINT16 usMarginTopY, UINT16 usMarginBottomY,
							UINT16 *pActualWidth, UINT16 *pActualHeight, BOOLEAN bFixedWidth)
{
	UINT16 usNumberVerticalPixels;
	UINT16 usTextWidth, usHeight;
	UINT16 i;
	HVOBJECT	hImageHandle;
	UINT16 usPosY, usPosX;
	VSURFACE_DESC		vs_desc;
	UINT16 usStringPixLength;
	SGPRect DestRect;
	HVSURFACE hSrcVSurface;
	UINT32 uiDestPitchBYTES;
	UINT32 uiSrcPitchBYTES;
	PIXEL	*pDestBuf;
	UINT8	*pSrcBuf;
	UINT8		ubFontColor, ubFontShadowColor;
	PIXEL	usColorVal;
	UINT32	usLoopEnd;
	INT16		sDispTextXPos;
	MercPopUpBox *pPopUpTextBox = NULL;

	if (!gMercPopupSystemInitialized || !pString || !pActualWidth || !pActualHeight ||
		!UtilsUiStateModel::IsValidIndex(std::size(zMercBackgroundPopupFilenames), ubBackgroundIndex) ||
		!UtilsUiStateModel::IsValidIndex(std::size(zMercBorderPopupFilenames), ubBorderIndex))
		return -1;
	*pActualWidth = 0;
	*pActualHeight = 0;
	const UINT32 pendingFlags = guiFlags;
	guiFlags = 0;
	if (usMarginX >= MERC_BACKGROUND_WIDTH / 2 ||
		static_cast<UINT32>(usMarginTopY) + usMarginBottomY >= MERC_BACKGROUND_HEIGHT)
		return -1;
	if( usWidth >= SCREEN_WIDTH )
		return( -1 );

	const UINT16 minimumTextWidth = static_cast<UINT16>(
		2 * MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_X + usMarginX + 1);
	if( usWidth < minimumTextWidth ) usWidth = minimumTextWidth;
	if (usWidth >= SCREEN_WIDTH) return -1;

	// check id value, if -1, box has not been inited yet
	if( iBoxId == -1 )
	{
		// no box yet

		// create box
		pPopUpTextBox = (MercPopUpBox *) MemAlloc( sizeof( MercPopUpBox ) );
		if (!pPopUpTextBox) return -1;
		std::memset(pPopUpTextBox, 0, sizeof(*pPopUpTextBox));

		// copy over ptr
		gPopUpTextBox = pPopUpTextBox;

			// Load appropriate images
		if( LoadTextMercPopupImages( ubBackgroundIndex, ubBorderIndex ) == FALSE )
		{
			MemFree( pPopUpTextBox );
			gPopUpTextBox = NULL;
			return( -1 );
		}
	}
	else
	{
		// has been created already,
		// Check if these images are different

		// grab box
		pPopUpTextBox = GetPopUpBoxIndex( iBoxId );
		if (!pPopUpTextBox) return -1;

			// copy over ptr
		gPopUpTextBox = pPopUpTextBox;

		if ( ubBackgroundIndex != pPopUpTextBox->ubBackgroundIndex || ubBorderIndex != pPopUpTextBox->ubBorderIndex || !pPopUpTextBox->fMercTextPopupInitialized)
		{
			if( LoadTextMercPopupImages( ubBackgroundIndex, ubBorderIndex ) == FALSE )
			{
				return( -1 );
			}
		}
	}

	gPopUpTextBox->uiFlags = pendingFlags;
	auto failPreparation = [&]() -> INT32
	{
		if (pPopUpTextBox && pPopUpTextBox->fMercTextPopupSurfaceInitialized)
		{
			DeleteVideoSurfaceFromIndex(pPopUpTextBox->uiSourceBufferIndex);
			pPopUpTextBox->fMercTextPopupSurfaceInitialized = FALSE;
		}
		if (iBoxId == -1 && pPopUpTextBox)
		{
			RemoveTextMercPopupImages();
			MemFree(pPopUpTextBox);
			gPopUpTextBox = NULL;
		}
		return -1;
	};

	usStringPixLength = WFStringPixLength( pString, TEXT_POPUP_FONT);	

	// sevenfm: change messagbebox width only if bFixedWidth = FALSE
	if( !bFixedWidth && ( usStringPixLength < ( usWidth - ( MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_X ) * 2 ) ) )
	{
		usWidth = usStringPixLength + MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_X * 2;
		usTextWidth = usWidth - ( MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_X	) * 2 + 1;
	}
	else
	{
		usTextWidth = usWidth - ( MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_X	) * 2 + 1 - usMarginX;
	}

	usNumberVerticalPixels = IanWrappedStringHeight(0,0, usTextWidth, 2, TEXT_POPUP_FONT, MERC_TEXT_COLOR,	pString, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

	usHeight = usNumberVerticalPixels + MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_X * 2;

	// Add height for margins
	usHeight += usMarginTopY + usMarginBottomY;

	// Add width for margins
	usWidth += (usMarginX*2);

	// Add width for iconic...
	if ( ( pPopUpTextBox->uiFlags & ( MERC_POPUP_PREPARE_FLAGS_STOPICON | MERC_POPUP_PREPARE_FLAGS_SKULLICON ) ) )
	{
		// Make minimun height for box...
		if ( usHeight < 45 )
		{
			usHeight = 45;
		}
		usWidth += 35;
	}

	if( usWidth >= MERC_BACKGROUND_WIDTH )
		usWidth = MERC_BACKGROUND_WIDTH-1;

	if ( usHeight >= MERC_BACKGROUND_HEIGHT )
		usHeight = MERC_BACKGROUND_HEIGHT - 1;

	//make sure the area isnt bigger then the background texture
	if( ( usWidth >= MERC_BACKGROUND_WIDTH ) || usHeight >= MERC_BACKGROUND_HEIGHT)
	{
		return failPreparation();
	}

	// Create a background video surface to blt the face onto
	memset( &vs_desc, 0, sizeof( VSURFACE_DESC ) );
	vs_desc.fCreateFlags = VSURFACE_CREATE_DEFAULT | VSURFACE_SYSTEM_MEM_USAGE;
	vs_desc.usWidth = usWidth;
	vs_desc.usHeight = usHeight;
	vs_desc.ubBitDepth = 16;
	if (pPopUpTextBox->fMercTextPopupSurfaceInitialized)
	{
		DeleteVideoSurfaceFromIndex(pPopUpTextBox->uiSourceBufferIndex);
		pPopUpTextBox->fMercTextPopupSurfaceInitialized = FALSE;
	}
	if (!AddVideoSurface(&vs_desc, &pPopUpTextBox->uiSourceBufferIndex))
	{
		return failPreparation();
	}
	pPopUpTextBox->fMercTextPopupSurfaceInitialized = TRUE;

	pPopUpTextBox->sWidth = usWidth;
	pPopUpTextBox->sHeight = usHeight;

	*pActualWidth = usWidth;
	*pActualHeight = usHeight;

	DestRect.iLeft = 0;
	DestRect.iTop = 0;
	DestRect.iRight = DestRect.iLeft + usWidth;
	DestRect.iBottom = DestRect.iTop + usHeight;

	if ( pPopUpTextBox->uiFlags & MERC_POPUP_PREPARE_FLAGS_TRANS_BACK )
	{
		// Fill the interior with an unused colour-key sentinel (yellow) and mark
		// that colour transparent, so the box background blits out leaving only the
		// border + (white) text. Two constraints:
		//  1) the fill must equal what the colour-key blitter derives from the
		//     surface's TransparentColor -- PixFromColor16 of its low 16 bits, NOT
		//     Get16BPPColor's true ARGB8888. At 32bpp those disagreed, so the key
		//     never matched and the box rendered as an opaque yellow rectangle.
		//  2) the sentinel must be a colour the text/border never use. It must stay
		//     YELLOW: the text is FONT_MCOLOR_WHITE, so a white key would key the
		//     text out too (box went see-through but text vanished).
		// Feeding the blitter the RGB565 yellow *token* (low 16 bits) and filling
		// with its expansion satisfies both: key == fill == opaque yellow, != white.
		const UINT16 transToken = Get16BPPColorToken( 255, 255, 0 );	// RGB565 yellow
		SetVideoSurfaceTransparency( pPopUpTextBox->uiSourceBufferIndex, transToken );

		pDestBuf = (PIXEL *)LockVideoSurface( pPopUpTextBox->uiSourceBufferIndex, &uiDestPitchBYTES);
		if (!pDestBuf)
		{
			DeleteVideoSurfaceFromIndex(pPopUpTextBox->uiSourceBufferIndex);
			pPopUpTextBox->fMercTextPopupSurfaceInitialized = FALSE;
			return failPreparation();
		}

		usColorVal = PixFromColor16( transToken );
		usLoopEnd	= ( usWidth * usHeight );

		for ( UINT32 fillIndex = 0; fillIndex < usLoopEnd; ++fillIndex )
		{
			pDestBuf[fillIndex] = usColorVal;
		}

		UnLockVideoSurface(pPopUpTextBox->uiSourceBufferIndex);
	}
	else
	{
		if( !GetVideoSurface( &hSrcVSurface, pPopUpTextBox->uiMercTextPopUpBackground) )
			return failPreparation();

		pDestBuf = (PIXEL *)LockVideoSurface( pPopUpTextBox->uiSourceBufferIndex, &uiDestPitchBYTES);
		pSrcBuf = LockVideoSurface( pPopUpTextBox->uiMercTextPopUpBackground, &uiSrcPitchBYTES);
		if (!pDestBuf || !pSrcBuf)
		{
			if (pSrcBuf) UnLockVideoSurface(pPopUpTextBox->uiMercTextPopUpBackground);
			if (pDestBuf) UnLockVideoSurface(pPopUpTextBox->uiSourceBufferIndex);
			return failPreparation();
		}

		Blt8BPPDataSubTo16BPPBuffer( pDestBuf,	uiDestPitchBYTES, hSrcVSurface, pSrcBuf,uiSrcPitchBYTES,0,0, &DestRect);

		UnLockVideoSurface( pPopUpTextBox->uiMercTextPopUpBackground);
		UnLockVideoSurface(pPopUpTextBox->uiSourceBufferIndex);
	}

	if (!GetVideoObject(&hImageHandle, pPopUpTextBox->uiMercTextPopUpBorder))
		return failPreparation();

	usPosY = 0;
	//blit top row of images
	for(i=TEXT_POPUP_GAP_BN_LINES; i< usWidth-TEXT_POPUP_GAP_BN_LINES; i+=TEXT_POPUP_GAP_BN_LINES)
	{
		//TOP ROW
		BltVideoObject(pPopUpTextBox->uiSourceBufferIndex, hImageHandle, 1,i, usPosY, VO_BLT_SRCTRANSPARENCY,NULL);
		//BOTTOM ROW
		BltVideoObject(pPopUpTextBox->uiSourceBufferIndex, hImageHandle, 6,i, usHeight - TEXT_POPUP_GAP_BN_LINES+6, VO_BLT_SRCTRANSPARENCY,NULL);
	}

	//blit the left and right row of images
	usPosX = 0;
	for(i=TEXT_POPUP_GAP_BN_LINES; i< usHeight-TEXT_POPUP_GAP_BN_LINES; i+=TEXT_POPUP_GAP_BN_LINES)
	{
		BltVideoObject(pPopUpTextBox->uiSourceBufferIndex, hImageHandle, 3,usPosX, i, VO_BLT_SRCTRANSPARENCY,NULL);
		BltVideoObject(pPopUpTextBox->uiSourceBufferIndex, hImageHandle, 4,usPosX+usWidth-4, i, VO_BLT_SRCTRANSPARENCY,NULL);
	}

	//blt the corner images for the row
	//top left
	BltVideoObject(pPopUpTextBox->uiSourceBufferIndex, hImageHandle, 0, 0, usPosY, VO_BLT_SRCTRANSPARENCY,NULL);
	//top right
	BltVideoObject(pPopUpTextBox->uiSourceBufferIndex, hImageHandle, 2, usWidth-TEXT_POPUP_GAP_BN_LINES, usPosY, VO_BLT_SRCTRANSPARENCY,NULL);
	//bottom left
	BltVideoObject(pPopUpTextBox->uiSourceBufferIndex, hImageHandle, 5, 0, usHeight-TEXT_POPUP_GAP_BN_LINES, VO_BLT_SRCTRANSPARENCY,NULL);
	//bottom right
	BltVideoObject(pPopUpTextBox->uiSourceBufferIndex, hImageHandle, 7, usWidth-TEXT_POPUP_GAP_BN_LINES, usHeight-TEXT_POPUP_GAP_BN_LINES, VO_BLT_SRCTRANSPARENCY,NULL);

	// Icon if ness....
	if ( pPopUpTextBox->uiFlags & MERC_POPUP_PREPARE_FLAGS_STOPICON )
	{
		BltVideoObjectFromIndex( pPopUpTextBox->uiSourceBufferIndex, guiBoxIcons, 0, 5, 4, VO_BLT_SRCTRANSPARENCY,NULL);
	}
	if ( pPopUpTextBox->uiFlags & MERC_POPUP_PREPARE_FLAGS_SKULLICON )
	{
		BltVideoObjectFromIndex( pPopUpTextBox->uiSourceBufferIndex, guiSkullIcons, 0, 9, 4, VO_BLT_SRCTRANSPARENCY,NULL);
	}

	//Get the font and shadow colors
	GetMercPopupBoxFontColor( ubBackgroundIndex, &ubFontColor, &ubFontShadowColor );

	SetFontShadow( ubFontShadowColor );
	SetFontDestBuffer( pPopUpTextBox->uiSourceBufferIndex, 0, 0, usWidth, usHeight, FALSE );

	//Display the text
	sDispTextXPos = (INT16)(( MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_X + usMarginX ));

	if ( pPopUpTextBox->uiFlags & ( MERC_POPUP_PREPARE_FLAGS_STOPICON | MERC_POPUP_PREPARE_FLAGS_SKULLICON ) )
	{
		sDispTextXPos += 30;
	}
	
//if language represents words with a single char
#ifdef SINGLE_CHAR_WORDS
	{
		//Enable the use of single word wordwrap
		if( gfUseWinFonts )
		{
			UseSingleCharWordsForWordWrap( TRUE );
		}

		//Display the text
		DisplayWrappedString( sDispTextXPos, (INT16)(( MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_Y + usMarginTopY ) ), usTextWidth, 2, MERC_TEXT_FONT, ubFontColor,	pString, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);

		//Disable the use of single word wordwrap
		UseSingleCharWordsForWordWrap( FALSE );
	}
#else
	{
		//Display the text
		DisplayWrappedString( sDispTextXPos, (INT16)(( MERC_TEXT_POPUP_WINDOW_TEXT_OFFSET_Y + usMarginTopY ) ), usTextWidth, 2, MERC_TEXT_FONT, ubFontColor,	pString, FONT_MCOLOR_BLACK, FALSE, LEFT_JUSTIFIED);
	}
#endif
	
	SetFontDestBuffer( FRAME_BUFFER, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FALSE );
	SetFontShadow(DEFAULT_SHADOW);

	if( iBoxId == -1 )
	{
		// now return attemp to add to pop up box list, if successful will return index
		const INT32 newBoxId = AddPopUpBoxToList(pPopUpTextBox);
		if (newBoxId < 0)
		{
			RemoveMercPopupBox();
			return -1;
		}
		return newBoxId;
	}


	// set as current box
	SetCurrentPopUpBox( iBoxId );

	return( iBoxId );
}

//Deletes the surface thats contains the border, background and the text.
BOOLEAN RemoveMercPopupBox()
{
	INT32 iCounter = 0;

	// make sure the current box does in fact exist
	if( gPopUpTextBox == NULL )
	{
		// failed..
		return( FALSE );
	}

	MercPopUpBox* box = gPopUpTextBox;
	// now find this box in the list
	for( iCounter = 0; iCounter < MAX_NUMBER_OF_POPUP_BOXES; ++iCounter )
	{
		if( gpPopUpBoxList[iCounter] == box )
		{
			gpPopUpBoxList[iCounter] = NULL;
			break;
		}
	}

	if (box->fMercTextPopupSurfaceInitialized)
		DeleteVideoSurfaceFromIndex(box->uiSourceBufferIndex);

	RemoveTextMercPopupImages();
	if (gOldPopUpTextBox == box) gOldPopUpTextBox = NULL;
	MemFree(box);
	gPopUpTextBox = NULL;

	return(TRUE);
}


BOOLEAN RemoveMercPopupBoxFromIndex( UINT32 uiId )
{
	// find this box, set it to current, and delete it
	if( SetCurrentPopUpBox( uiId ) == FALSE )
	{
		// failed
		return( FALSE );
	}

	// now try to remove it
	return( RemoveMercPopupBox( ) );
}


//Pass in the background index, and pointers to the font and shadow color
void	GetMercPopupBoxFontColor( UINT8 ubBackgroundIndex, UINT8 *pubFontColor, UINT8 *pubFontShadowColor)
{
	switch( ubBackgroundIndex )
	{
		case BASIC_MERC_POPUP_BACKGROUND:
			*pubFontColor = TEXT_POPUP_COLOR;
			*pubFontShadowColor = DEFAULT_SHADOW;
			break;

		case WHITE_MERC_POPUP_BACKGROUND:
			*pubFontColor = 2;
			*pubFontShadowColor = FONT_MCOLOR_WHITE;
			break;

		case GREY_MERC_POPUP_BACKGROUND:
			*pubFontColor = 2;
			*pubFontShadowColor = NO_SHADOW;
			break;

		case LAPTOP_POPUP_BACKGROUND:
			*pubFontColor = TEXT_POPUP_COLOR;
			*pubFontShadowColor = DEFAULT_SHADOW;
			break;

		default:
			*pubFontColor = TEXT_POPUP_COLOR;
			*pubFontShadowColor = DEFAULT_SHADOW;
			break;
	}
}

BOOLEAN	SetPrepareMercPopupFlags( UINT32 uiFlags )
{
	guiFlags |= uiFlags;
	return( TRUE );
}

BOOLEAN SetPrepareMercPopUpFlagsFromIndex( UINT32 uiFlags, UINT32 uiId )
{
	// find this box, set it to current, and delete it
	if( SetCurrentPopUpBox( uiId ) == FALSE )
	{
		// failed
		return( FALSE );
	}

	// now try to remove it
	return( SetPrepareMercPopupFlags( uiFlags ) );
}
