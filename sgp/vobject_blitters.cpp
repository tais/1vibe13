	#include "DirectDraw Calls.h"
	#include <stdio.h>
	#include <cstring>
	#include "DEBUG.H"
		#include "video.h"													// JA2
	#include "himage.h"
	#include "vobject.h"
	#include "WCheck.h"
	#include "vobject.h"
	#include "vobject_blitters.h"
	#include "shading.h"
	#include "pixfmt.h"   // PIXEL + PixShade/PixIntensity (Phase 6b)
	#include "sgp_logger.h"

#include <map>
std::map<UINT32,ClipRectangle> g_SurfaceRectangle;

static UINT8 g_AlphaTimesValueCache[256][256];

static const unsigned short maxChar = 0xff;
PIXEL blendWithAlpha(unsigned int colNew, unsigned int colOld, unsigned int alpha)
{
	const unsigned int a = alpha & 0xFF;
	const unsigned int ia = 255 - a;
#if SGP_PIXEL_DEPTH == 32
	// Both colours are ARGB8888 (palette LUT + dest are 32bpp). Blend per
	// 8-bit channel; result is opaque.
	const unsigned int oldR = (colOld >> 16) & 0xFF;
	const unsigned int oldG = (colOld >>  8) & 0xFF;
	const unsigned int oldB =  colOld        & 0xFF;
	const unsigned int newR = (colNew >> 16) & 0xFF;
	const unsigned int newG = (colNew >>  8) & 0xFF;
	const unsigned int newB =  colNew        & 0xFF;
	const unsigned int outR = (oldR * ia + newR * a + 128) / 255;
	const unsigned int outG = (oldG * ia + newG * a + 128) / 255;
	const unsigned int outB = (oldB * ia + newB * a + 128) / 255;
	return 0xFF000000u | (outR << 16) | (outG << 8) | outB;
#else
	const unsigned int oldR = (colOld >> 11) & 0x1F;
	const unsigned int oldG = (colOld >> 5)  & 0x3F;
	const unsigned int oldB = (colOld)       & 0x1F;
	const unsigned int newR = (colNew >> 11) & 0x1F;
	const unsigned int newG = (colNew >> 5)  & 0x3F;
	const unsigned int newB = (colNew)       & 0x1F;
	const unsigned int outR = (oldR * ia + newR * a + 128) / 255;
	const unsigned int outG = (oldG * ia + newG * a + 128) / 255;
	const unsigned int outB = (oldB * ia + newB * a + 128) / 255;
	return (unsigned short)((outR << 11) | (outG << 5) | outB);
#endif
}

class InitAlphaTimesValueCache
{
public:
	InitAlphaTimesValueCache()
	{
		for(unsigned int alpha = 0; alpha < 256; alpha++)
		{
			for(unsigned int val = 0; val < 256; val++)
			{
				// it can't get any simpler than that
				g_AlphaTimesValueCache[alpha][val] = (UINT8)( ((double)alpha/255.0) * val );
			}
		}
	}
};

static InitAlphaTimesValueCache s_InitAlphaCache;

BOOLEAN Blt32BPPTo16BPPTrans(PIXEL *pDest, UINT32 uiDestPitch, UINT32 *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight)
{
	UINT32 *pSrcPtr;
	PIXEL *pDestPtr;
	UINT32 uiLineSkipDest, uiLineSkipSrc;

	Assert(pDest!=NULL);
	Assert(pSrc!=NULL);

	pSrcPtr			= (UINT32 *)((UINT8 *)pSrc+(iSrcYPos*uiSrcPitch)+(iSrcXPos*4));
	uiLineSkipSrc	= uiSrcPitch-(uiWidth*4);

	pDestPtr		= (PIXEL *)((UINT8 *)pDest+(iDestYPos*uiDestPitch)+(iDestXPos*sizeof(PIXEL)));
	uiLineSkipDest	= uiDestPitch-(uiWidth*sizeof(PIXEL));

	UINT8 alpha, dst_channel, src_channel;
	UINT8 red, green, blue;
	do
	{
		UINT32 w = uiWidth;
		do
		{
			// a
			alpha = (UINT8)((0xFF000000 & *pSrcPtr) >> 24);
			//alpha = 255;
			if(alpha > 0)
			{
#if SGP_PIXEL_DEPTH == 32
				// dest is ARGB8888; source is alpha + R(low),G,B (FROMRGB layout)
				const UINT32 d = *pDestPtr;
				dst_channel = (UINT8)((d >> 16) & 0xFF);
				src_channel = (UINT8)( *pSrcPtr        & 0xFF);
				red   = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );

				dst_channel = (UINT8)((d >>  8) & 0xFF);
				src_channel = (UINT8)((*pSrcPtr >>  8) & 0xFF);
				green = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );

				dst_channel = (UINT8)( d         & 0xFF);
				src_channel = (UINT8)((*pSrcPtr >> 16) & 0xFF);
				blue  = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );

				*pDestPtr = 0xFF000000u | ((UINT32)red << 16) | ((UINT32)green << 8) | (UINT32)blue;
#else
				// r
				dst_channel = (UINT8)((0x1F & *pDestPtr) << 3);
				src_channel = (UINT8)((0xFF & *pSrcPtr) );
				red = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );
				//red = (UINT8)( g_AlphaTimesValueCache[alpha][src_channel] );

				// g
				dst_channel = (UINT8)((0x7E0 & *pDestPtr) >> 3);
				src_channel = (UINT8)((0xFF00 & *pSrcPtr) >> 8);
				green = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );
				//green = (UINT8)( g_AlphaTimesValueCache[alpha][src_channel] );

				// b
				dst_channel = (UINT8)((0xF800 & *pDestPtr) >> 8);
				src_channel = (UINT8)((0xFF0000 & *pSrcPtr) >> 16);
				blue = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );
				//blue = (UINT8)( g_AlphaTimesValueCache[alpha][src_channel] );


				UINT32 newcolor = FROMRGB(red,green,blue);
				*pDestPtr = Get16BPPColor(newcolor);
#endif
			}
			pSrcPtr++;
			pDestPtr++;
		}
		while (--w != 0);
		pSrcPtr  = (UINT32*)((UINT8*)pSrcPtr  + uiLineSkipSrc);
		pDestPtr = (PIXEL *)((UINT8 *)pDestPtr + uiLineSkipDest);
	}
	while (--uiHeight != 0);

	return ( TRUE );
}

BOOLEAN Blt32BPPTo16BPPTransShadow(PIXEL *pDst, UINT32 uiDstPitch, UINT32 *pSrc, UINT32 uiSrcPitch, INT32 iDstXPos, INT32 iDstYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight)
{
	UINT32 *pSrcPtr;
	PIXEL *pDstPtr;
	UINT32 uiLineSkipDst, uiLineSkipSrc;

	Assert(pDst!=NULL);
	Assert(pSrc!=NULL);

	pSrcPtr			= (UINT32*)((UINT8*)pSrc + (iSrcYPos * uiSrcPitch) + (iSrcXPos * 4));
	uiLineSkipSrc	= uiSrcPitch-(uiWidth*4);

	pDstPtr			= (PIXEL*)((UINT8*)pDst + (iDstYPos * uiDstPitch) + (iDstXPos * sizeof(PIXEL)));
	uiLineSkipDst	= uiDstPitch-(uiWidth*sizeof(PIXEL));

	UINT8 alpha, dst_channel, src_channel;
	UINT8 red, green, blue;
	PIXEL tmpVal;
	UINT32 newcolor;
	do
	{
		UINT32 w = uiWidth;
		do
		{
			// a
			alpha = (UINT8)((0xFF000000 & *pSrcPtr) >> 24);
			//alpha = 255;
			if(alpha > 0)
			{
				// the darker shade
				tmpVal = PixShade(*pDstPtr);

#if SGP_PIXEL_DEPTH == 32
				// Blend the dest with a darkened copy of itself (shadow),
				// weighted by source alpha. Source colour is ignored.
				const UINT32 d = *pDstPtr;
				dst_channel = (UINT8)((d      >> 16) & 0xFF);
				src_channel = (UINT8)((tmpVal >> 16) & 0xFF);
				red   = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );

				dst_channel = (UINT8)((d      >>  8) & 0xFF);
				src_channel = (UINT8)((tmpVal >>  8) & 0xFF);
				green = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );

				dst_channel = (UINT8)( d        & 0xFF);
				src_channel = (UINT8)( tmpVal   & 0xFF);
				blue  = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );

				*pDstPtr = 0xFF000000u | ((UINT32)red << 16) | ((UINT32)green << 8) | (UINT32)blue;
#else
				// r
				dst_channel = (UINT8)((0x1F & *pDstPtr) << 3);
				//src_channel = (UINT8)((0xFF & tmpVal) );
				src_channel = (UINT8)((0x1F & tmpVal) << 3);
				red = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );

				// g
				dst_channel = (UINT8)((0x7E0  & *pDstPtr) >> 3);
				//src_channel = (UINT8)((0xFF00 & tmpVal) >> 8);
				src_channel = (UINT8)((0x7E0 & tmpVal) >> 3);
				green = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );

				// b
				dst_channel = (UINT8)((0xF800   & *pDstPtr) >> 8);
				//src_channel = (UINT8)((0xFF0000 & tmpVal) >> 16);
				src_channel = (UINT8)((0xF800 & tmpVal) >> 8);
				blue = (UINT8)( g_AlphaTimesValueCache[255-alpha][dst_channel] + g_AlphaTimesValueCache[alpha][src_channel] );


				newcolor = FROMRGB(red,green,blue);
				*pDstPtr = Get16BPPColor(newcolor);
#endif
			}
			pSrcPtr++;
			pDstPtr++;
		}
		while (--w != 0);
		pSrcPtr = (UINT32*)((UINT8*)pSrcPtr + uiLineSkipSrc);
		pDstPtr = (PIXEL *)((UINT8 *)pDstPtr + uiLineSkipDst);
	}
	while (--uiHeight != 0);

	return ( TRUE );
}


/*	Here are bliting functions. If you dont know what kind of functions they are so :
 *	correct me if im wrong they copy array of bits from src image to dest image
 *	maby we can get ride this includes above? we dont need theme here i thinks so
 *	any questions? joker
 */

/*	ClipingRect was declared here with static initializer
 *	We need to change that and initialize it in run-time
 *	any questions? joker
 */

SGPRect	ClippingRect;

UINT32	guiTranslucentMask=0x3def; //0x7bef;		// mask for halving 5,6,5

// GLOBALS for pre-calculating skip values
INT32		gLeftSkip, gRightSkip, gTopSkip, gBottomSkip;
BOOLEAN	gfUsePreCalcSkips = FALSE;


//*Experimental**********************************************************************

/**********************************************************************************************
 Blt16BPPDataTo16BPPBufferTransZClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt16BPPDataTo16BPPBufferTransZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
PIXEL		*p16BPPPalette;
UINT32		uiOffset;
UINT32		usHeight, usWidth, Unblitted;
UINT8		*SrcPtr, *DestPtr, *ZPtr;
UINT32		LineSkip;
ETRLEObject *pTrav;
INT32		iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
INT32		ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Despite the function name saying "16BPPData" the source is still
	// 8bpp ETRLE (uses pETRLEObject + pShadeCurrent palette LUT) --
	// identical body to TransZClip.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (usZValue >= rowZ[dx]) {
							rowZ[dx]    = usZValue;
							rowDest[dx] = p16BPPPalette[*src];
						}
					}
					++src;
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt16BPPDataTo16BPPBufferTransparentClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt16BPPDataTo16BPPBufferTransparentClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
//PIXEL		*p16BPPPalette;
//UINT32		uiOffset;
UINT32		usHeight, usWidth, Unblitted;
UINT8		*SrcPtr, *DestPtr;
UINT32		LineSkip;
//ETRLEObject *pTrav;
INT32		iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
INT32		ClipX1, ClipY1, ClipX2, ClipY2;
SixteenBPPObjectInfo *	p16BPPObject;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	p16BPPObject=&(hSrcVObject->p16BPPObject[usIndex]);

	// Get Offsets from Index into structure
	usHeight				= p16BPPObject->usHeight;
	usWidth					= p16BPPObject->usWidth;

	// Add to start position of dest buffer
	iTempX = iX + p16BPPObject->sOffsetX;
	iTempY = iY + p16BPPObject->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	if ( gfUsePreCalcSkips )
	{
		LeftSkip= gLeftSkip;
		RightSkip= gRightSkip;
		TopSkip= gTopSkip;
		BottomSkip= gBottomSkip;
		gfUsePreCalcSkips = FALSE;

	}
	else
	{
		// Calculate rows hanging off each side of the screen
		LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
		RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
		TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
		BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);
	}

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)p16BPPObject->p16BPPData;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable 16bpp-ETRLE clipped blit. Same ETRLE command protocol
	// as the 8bpp variants (0 = end-of-row, 0x80+n = skip n
	// transparent pixels, otherwise n opaque pixels follow), but each
	// opaque pixel is 2 source bytes (raw RGB565) rather than a 1-byte
	// palette index.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += (UINT32)cmd * 2;  // 16bpp payload
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT16 pix = *(const UINT16*)src;
					src += 2;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						rowDest[srcX - LeftSkip] = pix;
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/***********************************************************************************/

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZNBClipTranslucent

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	NOT updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("Translucents").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipTranslucent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
PIXEL *p16BPPPalette;
UINT32 uiOffset, uiLineFlag;
UINT32 usHeight, usWidth, Unblitted;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 LineSkip;
ETRLEObject *pTrav;
INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZNBClipTranslucent: clipped 50% blend, Z-test, no Z write.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (rowZ[dx] < usZValue) {
							rowDest[dx] = PixBlend50(p16BPPPalette[v], rowDest[dx]);
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZTranslucent

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("Translucents").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZTranslucent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
UINT32 usHeight, usWidth, uiOffset, LineSkip;
INT32	iTempX, iTempY;
PIXEL *p16BPPPalette;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 uiLineFlag;
ETRLEObject *pTrav;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZTranslucent: 50% blend via lo-bit-strip:
	// out = ((src>>1) & mask) + ((dest>>1) & mask).
	// Z-test (>) with Z update on accept.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (*rowZ < usZValue) {
						*rowZ = usZValue;
						*rowDest = PixBlend50(p16BPPPalette[*src], *rowDest);
					}
					++src;
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZClipTranslucent

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("Translucents").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipTranslucent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
PIXEL *p16BPPPalette;
UINT32 uiOffset, uiLineFlag;
UINT32 usHeight, usWidth, Unblitted;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 LineSkip;
ETRLEObject *pTrav;
INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZClipTranslucent: clipped 50% blend, Z-test + Z update.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (rowZ[dx] < usZValue) {
							rowZ[dx] = usZValue;
							rowDest[dx] = PixBlend50(p16BPPPalette[v], rowDest[dx]);
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZNBTranslucent

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	NOT updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("Translucents").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBTranslucent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
UINT32 usHeight, usWidth, uiOffset, LineSkip;
INT32	iTempX, iTempY;
PIXEL *p16BPPPalette;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 uiLineFlag;
ETRLEObject *pTrav;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZNBTranslucent: 50% blend, Z-test, no Z update.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (*rowZ < usZValue) {
						*rowDest = PixBlend50(p16BPPPalette[*src], *rowDest);
					}
					++src;
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}





/**********************************************************************************************
 InitZBuffer

	Allocates and initializes a Z buffer for use with the Z buffer blitters. Doesn't really do
	much except allocate a chunk of memory, and zero it.

**********************************************************************************************/
UINT16 *InitZBuffer(UINT32 uiPitch, UINT32 uiHeight)
{
UINT16 *pBuffer;
	/*
	*	ClippingRect was declared first with SCREEN_WIDTH and HEIGHT but now they are not
	*	constant so i will initialize it here
	*	any questions? joker
	*/


	ClippingRect.iLeft		= 0;
	ClippingRect.iTop		= 0;
	ClippingRect.iRight		= SCREEN_WIDTH;
	ClippingRect.iBottom	= SCREEN_HEIGHT;

	if((pBuffer = (UINT16 *) MemAlloc(uiPitch*uiHeight))==NULL)
		return(NULL);
	BYTE* data = (BYTE*)pBuffer;
	SurfaceData::SetApplicationData(data);
	g_SurfaceRectangle[SurfaceData::GetSurfaceID(data)].SetRect(ClippingRect);

	memset(pBuffer, 0, (uiPitch*uiHeight));
	return(pBuffer);
}

/**********************************************************************************************
 ShutdownZBuffer

	Frees up the memory allocated for the Z buffer.

**********************************************************************************************/
BOOLEAN ShutdownZBuffer(UINT16 *pBuffer)
{
	SurfaceData::ReleaseApplicationData((BYTE*)pBuffer);
	MemFree(pBuffer);
	return(TRUE);
}


BOOLEAN BlitZRect(UINT16 *pZBuffer, UINT32 uiPitch, INT16 sLeft, INT16 sTop, INT16 sRight, INT16 sBottom, UINT16 usZValue)
{
INT16 sLeftClip, sTopClip, sRightClip, sBottomClip;
UINT8 *pZPtr;
UINT32 uiLineSkip, usWidth, usHeight;

	sLeftClip=__max(ClippingRect.iLeft, sLeft);
	sRightClip=__min(ClippingRect.iRight, sRight);
	sTopClip=__max(ClippingRect.iTop, sTop);
	sBottomClip=__min(ClippingRect.iBottom, sBottom);

	usHeight=sBottomClip-sTopClip;
	usWidth=sRightClip-sLeftClip;
	pZPtr=(UINT8 *)pZBuffer+(sTopClip*uiPitch)+(sLeftClip*2);
	uiLineSkip=(uiPitch-(usWidth*2));

	if((usHeight==0) || (usWidth==0))
		return(FALSE);

	// Portable BlitZRect: fill rect of Z buffer with usZValue.
	{
		UINT16* row = (UINT16*)pZPtr;   // Z-buffer row (depth values stay UINT16)
		for (UINT32 y = 0; y < usHeight; ++y) {
			for (UINT32 x = 0; x < usWidth; ++x) row[x] = usZValue;
			row = (UINT16*)((UINT8*)row + uiPitch);
		}
	}

	return(TRUE);
}




//*****************************************************************************
//** 16 Bit Blitters
//**
//*****************************************************************************

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferMonoShadowClip

	Uses a bitmap an 8BPP template for blitting. Anywhere a 1 appears in the bitmap, a shadow
	is blitted to the destination (a black pixel). Any other value above zero is considered a
	forground color, and zero is background. If the parameter for the background color is zero,
	transparency is used for the background.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferMonoShadowClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL usForeground, PIXEL usBackground, PIXEL usShadow )
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable ETRLE row decoder with clipping + mono palette mapping.
	// Per-pixel rule (matching the legacy asm):
	//   src byte 0 -> background (only written if usBackground != 0)
	//   src byte 1 -> shadow     (only written if usShadow     != 0)
	//   src byte >1 -> foreground
	// Transparent runs (high-bit-set commands) write usBackground
	// when non-zero, otherwise just skip. Used by WinFont when GDI
	// rasterizes glyphs and needs to flatten them into the surface.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}

		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowBase = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					if (usBackground != 0) {
						for (UINT8 i = 0; i < n; ++i, ++srcX) {
							if (srcX >= LeftSkip && srcX < rightEdge) {
								rowBase[srcX - LeftSkip] = PixFromColor16(usBackground);
							}
						}
					} else {
						srcX += n;
					}
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const UINT8 v = *src;
						PIXEL colour = 0; bool write = false;
						if (v == 0) {
							if (usBackground != 0) { colour = usBackground; write = true; }
						} else if (v == 1) {
							if (usShadow != 0)     { colour = usShadow;     write = true; }
						} else {
							colour = usForeground; write = true;
						}
						if (write) rowBase[srcX - LeftSkip] = PixFromColor16(colour);
					}
					++src;
				}
			}
			rowBase = (PIXEL *)((UINT8 *)rowBase + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}



extern void WriteMessageToFile( const STR16 pString );

/**********************************************************************************************
	Blt16BPPTo16BPP

	Copies a rect of 16 bit data from a video buffer to a buffer position of the brush
	in the data area, for later blitting. Used to copy background information for mercs
	etc. to their unblit buffer, for later reblitting. Does NOT clip.

**********************************************************************************************/
BOOLEAN Blt16BPPTo16BPP(PIXEL *pDest, UINT32 uiDestPitch, PIXEL *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight)
{
PIXEL *pSrcPtr, *pDestPtr;
UINT32 uiLineSkipDest, uiLineSkipSrc;

	Assert(pDest!=NULL);
	Assert(pSrc!=NULL);

	try
	{
		UINT32 surfID = SurfaceData::GetSurfaceID((BYTE*)pDest);
		ClipRectangle::ClipType ct;
		if( (ct=g_SurfaceRectangle[surfID].Clip(iDestXPos, iDestYPos,uiWidth, uiHeight)) != ClipRectangle::NoClip )
		{
#if _DEBUG
			WriteMessageToFile(L"Trying to render to outside of destination surface");
#endif
			if(ct == ClipRectangle::FullClip)
			{
				return false;
			}
		}
		surfID = SurfaceData::GetSurfaceID((BYTE*)pSrc);
		if( surfID && (ct=g_SurfaceRectangle[surfID].Clip(iSrcXPos, iSrcYPos,uiWidth, uiHeight)) != ClipRectangle::NoClip )
		{
#if _DEBUG
			WriteMessageToFile(L"Trying to render from outside of surface surface");
#endif
			if(ct == ClipRectangle::FullClip)
			{
				return false;
			}
		}
	}
	catch(std::exception& ex)
	{
		SGP_ERROR(ex.what());
		return false;
	}

	pSrcPtr=(PIXEL *)((UINT8 *)pSrc+(iSrcYPos*uiSrcPitch)+(iSrcXPos*sizeof(PIXEL)));
	pDestPtr=(PIXEL *)((UINT8 *)pDest+(iDestYPos*uiDestPitch)+(iDestXPos*sizeof(PIXEL)));
	uiLineSkipDest=uiDestPitch-(uiWidth*sizeof(PIXEL));
	uiLineSkipSrc=uiSrcPitch-(uiWidth*sizeof(PIXEL));

	// Portable C path: row-by-row memcpy. Modern memcpy() is
	// vectorized; the original asm unrolled movsw / movsd by hand.
	for (UINT32 y = 0; y < uiHeight; ++y)
	{
		std::memcpy(pDestPtr, pSrcPtr, (size_t)uiWidth * sizeof(PIXEL));
		pDestPtr = (PIXEL *)((UINT8 *)pDestPtr + uiDestPitch);
		pSrcPtr  = (PIXEL *)((UINT8 *)pSrcPtr  + uiSrcPitch);
	}

	return(TRUE);
}

/**********************************************************************************************
	Blt16BPPTo16BPPTrans

	Copies a rect of 16 bit data from a video buffer to a buffer position of the brush
	in the data area, for later blitting. Used to copy background information for mercs
	etc. to their unblit buffer, for later reblitting. Does NOT clip. Transparent color is
	not copied.

**********************************************************************************************/
BOOLEAN Blt16BPPTo16BPPTrans(PIXEL *pDest, UINT32 uiDestPitch, PIXEL *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight, UINT16 usTrans)
{
	PIXEL *pSrcPtr, *pDestPtr;
	UINT32 uiLineSkipDest, uiLineSkipSrc;

	Assert(pDest!=NULL);
	Assert(pSrc!=NULL);

	pSrcPtr			= (PIXEL *)((UINT8 *)pSrc+(iSrcYPos*uiSrcPitch)+(iSrcXPos*sizeof(PIXEL)));
	pDestPtr		= (PIXEL *)((UINT8 *)pDest+(iDestYPos*uiDestPitch)+(iDestXPos*sizeof(PIXEL)));
	uiLineSkipDest	= uiDestPitch - (uiWidth*sizeof(PIXEL));
	uiLineSkipSrc	= uiSrcPitch - (uiWidth*sizeof(PIXEL));
	do
	{
		UINT32 w = uiWidth;
		do
		{
			if (*pSrcPtr != usTrans) *pDestPtr = *pSrcPtr;
			pSrcPtr++;
			pDestPtr++;
		}
		while (--w != 0);
		pSrcPtr  = (PIXEL *)((UINT8 *)pSrcPtr  + uiLineSkipSrc);
		pDestPtr = (PIXEL *)((UINT8 *)pDestPtr + uiLineSkipDest);
	}
	while (--uiHeight != 0);
	return(TRUE);
}

BOOLEAN Blt16BPPTo16BPPTransShadow(PIXEL *pDest, UINT32 uiDestPitch, PIXEL *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight, UINT16 usTrans)
{
	PIXEL *pSrcPtr, *pDestPtr;
	UINT32 uiLineSkipDest, uiLineSkipSrc;

	Assert(pDest != NULL);
	Assert(pSrc  != NULL);

	pSrcPtr			= (PIXEL *)((UINT8 *)pSrc  + (iSrcYPos  * uiSrcPitch)  + (iSrcXPos  * sizeof(PIXEL)));
	pDestPtr		= (PIXEL *)((UINT8 *)pDest + (iDestYPos * uiDestPitch) + (iDestXPos * sizeof(PIXEL)));
	uiLineSkipDest	= uiDestPitch - (uiWidth * sizeof(PIXEL));
	uiLineSkipSrc	= uiSrcPitch  - (uiWidth * sizeof(PIXEL));
	do
	{
		UINT32 w = uiWidth;
		do
		{
			if (*pSrcPtr != usTrans)
			{
				*pDestPtr = PixShade(*pDestPtr);
			}
			pSrcPtr++;
			pDestPtr++;
		}
		while (--w != 0);
		pSrcPtr  = (PIXEL *)((UINT8 *)pSrcPtr  + uiLineSkipSrc);
		pDestPtr = (PIXEL *)((UINT8 *)pDestPtr + uiLineSkipDest);
	}
	while (--uiHeight != 0);
	return TRUE;
}

/**********************************************************************************************
	Blt16BPPTo16BPPMirror

	Copies a rect of 16 bit data from a video buffer to a buffer position of the brush
	in the data area, for later blitting. Used to copy background information for mercs
	etc. to their unblit buffer, for later reblitting. Does NOT clip.

**********************************************************************************************/
BOOLEAN Blt16BPPTo16BPPMirror(PIXEL *pDest, UINT32 uiDestPitch, PIXEL *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight)
{
PIXEL *pSrcPtr, *pDestPtr;
UINT32 uiLineSkipDest, uiLineSkipSrc;
INT32	RightSkip, LeftSkip, TopSkip, BottomSkip, BlitLength, BlitHeight;
INT32 iTempX, iTempY, ClipX1, ClipY1, ClipX2, ClipY2;
SGPRect *clipregion=NULL;

	Assert(pDest!=NULL);
	Assert(pSrc!=NULL);

	// Add to start position of dest buffer
	iTempX = iDestXPos;
	iTempY = iDestYPos;

	if(clipregion==NULL)
	{
		ClipX1= ClippingRect.iLeft; 	//0; it was changed, why??? joker//;
		ClipY1= ClippingRect.iTop;		//0; //;
		ClipX2= ClippingRect.iRight;	//SCREEN_WIDTH; //;
		ClipY2= ClippingRect.iBottom;	//SCREEN_HEIGHT;	//;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - __min(ClipX1, iTempX), (INT32)uiWidth);
	RightSkip=__min(__max(ClipX2, (iTempX+(INT32)uiWidth)) - ClipX2, (INT32)uiWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)uiHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)uiHeight)) - ClipY2, (INT32)uiHeight);

	iTempX=__max(ClipX1, iDestXPos);
	iTempY=__max(ClipY1, iDestYPos);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)uiWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)uiHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)uiWidth) || (RightSkip >=(INT32)uiWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)uiHeight) || (BottomSkip >=(INT32)uiHeight))
		return(TRUE);

	pSrcPtr=(PIXEL *)((UINT8 *)pSrc+(TopSkip*uiSrcPitch)+(RightSkip*2));
	pDestPtr=(PIXEL *)((UINT8 *)pDest+(iTempY*uiDestPitch)+(iTempX*2)+((BlitLength-1)*2));
	uiLineSkipDest=uiDestPitch;//+((BlitLength-1)*2);
	uiLineSkipSrc=uiSrcPitch-(BlitLength*2);

	// Portable 16bpp mirrored copy. pDestPtr starts at the rightmost
	// pixel of the visible row; pSrcPtr walks forward; dest walks
	// backward.
	for (INT32 row = 0; row < BlitHeight; ++row) {
		PIXEL* rowSrc  = pSrcPtr;
		PIXEL* rowDest = pDestPtr;
		for (INT32 i = 0; i < BlitLength; ++i) {
			*rowDest-- = *rowSrc++;
		}
		pSrcPtr  = (PIXEL *)((UINT8 *)pSrcPtr  + uiSrcPitch);
		pDestPtr = (PIXEL *)((UINT8 *)pDestPtr + uiDestPitch);
	}

	return(TRUE);
}

/***********************************************************************************************
	Blt8BPPTo8BPP

	Copies a rect of an 8 bit data from a video buffer to a buffer position of the brush
	in the data area, for later blitting. Used to copy background information for mercs
	etc. to their unblit buffer, for later reblitting. Does NOT clip.

**********************************************************************************************/
BOOLEAN Blt8BPPTo8BPP(UINT8 *pDest, UINT32 uiDestPitch, UINT8 *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight)
{
UINT8 *pSrcPtr, *pDestPtr;
UINT32 uiLineSkipDest, uiLineSkipSrc;

	Assert(pDest!=NULL);
	Assert(pSrc!=NULL);

	pSrcPtr=pSrc+(iSrcYPos*uiSrcPitch)+(iSrcXPos);
	pDestPtr=pDest+(iDestYPos*uiDestPitch)+(iDestXPos);
	uiLineSkipDest=uiDestPitch-(uiWidth);
	uiLineSkipSrc=uiSrcPitch-(uiWidth);

	// Portable 8bpp row-by-row memcpy.
	for (UINT32 y = 0; y < uiHeight; ++y) {
		std::memcpy(pDestPtr, pSrcPtr, uiWidth);
		pDestPtr += uiDestPitch;
		pSrcPtr  += uiSrcPitch;
	}

	return(TRUE);
}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZPixelate

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("pixelates").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZPixelate( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
UINT32 usHeight, usWidth, uiOffset, LineSkip;
INT32	iTempX, iTempY;
PIXEL *p16BPPPalette;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 uiLineFlag;
ETRLEObject *pTrav;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZPixelate: checkerboard pattern keyed on absolute
	// screen coords. Blit a pixel iff (absY + absX) is even; otherwise
	// skip. Z-tested with Z update on accept.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 lineFlag = uiLineFlag;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			INT32 xparity = iTempX & 1;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					xparity ^= (n & 1);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (lineFlag == (UINT32)xparity) {
						if (*rowZ <= usZValue) {
							*rowZ = usZValue;
							*rowDest = p16BPPPalette[*src];
						}
					}
					++src;
					++rowDest;
					++rowZ;
					xparity ^= 1;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZPixelateObscured

	// OK LIKE NORMAL PIXELATE BUT ONLY PIXELATES STUFF BELOW Z level

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("pixelates").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZPixelateObscured( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
UINT32 usHeight, usWidth, uiOffset, LineSkip;
INT32	iTempX, iTempY;
PIXEL *p16BPPPalette;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 uiLineFlag;
ETRLEObject *pTrav;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZPixelateObscured: like TransZPixelate but the
	// checkerboard only kicks in when the sprite is occluded (Z >=
	// existing). When in front, it's a full blit. Z always updated.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 lineFlag = uiLineFlag;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			INT32 xparity = iTempX & 1;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					xparity ^= (n & 1);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const bool inFront = (*rowZ < usZValue);
					if (inFront || lineFlag == (UINT32)xparity) {
						*rowZ    = usZValue;
						*rowDest = p16BPPPalette[*src];
					}
					++src;
					++rowDest;
					++rowZ;
					xparity ^= 1;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZClipPixelate

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("pixelates").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipPixelate( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
PIXEL *p16BPPPalette;
UINT32 uiOffset, uiLineFlag;
UINT32 usHeight, usWidth, Unblitted;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 LineSkip;
ETRLEObject *pTrav;
INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZClipPixelate: clipped checkerboard, Z-test + Z update.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		// lineFlag at first visible row = (iTempY + TopSkip) & 1
		UINT32 lineFlag = (UINT32)((iTempY + TopSkip) & 1);
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						const INT32 absX = iTempX + LeftSkip + dx;
						if (lineFlag == (UINT32)(absX & 1)) {
							if (rowZ[dx] <= usZValue) {
								rowZ[dx]    = usZValue;
								rowDest[dx] = p16BPPPalette[v];
							}
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZNBPixelate

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	NOT updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("pixelates").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBPixelate( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
UINT32 usHeight, usWidth, uiOffset, LineSkip;
INT32	iTempX, iTempY;
PIXEL *p16BPPPalette;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 uiLineFlag;
ETRLEObject *pTrav;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZNBPixelate: checkerboard, Z-test, no Z update.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 lineFlag = uiLineFlag;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			INT32 xparity = iTempX & 1;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					xparity ^= (n & 1);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (lineFlag == (UINT32)xparity) {
						if (*rowZ <= usZValue) {
							*rowDest = p16BPPPalette[*src];
						}
					}
					++src;
					++rowDest;
					++rowZ;
					xparity ^= 1;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZNBClipPixelate

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	NOT updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("pixelates").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipPixelate( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
PIXEL *p16BPPPalette;
UINT32 uiOffset, uiLineFlag;
UINT32 usHeight, usWidth, Unblitted;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 LineSkip;
ETRLEObject *pTrav;
INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));
	uiLineFlag=(iTempY&1);

	// Portable TransZNBClipPixelate: clipped checkerboard, Z-test, no Z write.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		UINT32 lineFlag = (UINT32)((iTempY + TopSkip) & 1);
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						const INT32 absX = iTempX + LeftSkip + dx;
						if (lineFlag == (UINT32)(absX & 1)) {
							if (rowZ[dx] <= usZValue) {
								rowDest[dx] = p16BPPPalette[v];
							}
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZ

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZ( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// ETRLE row decoder, Z-tested with Z update.
	// Command byte protocol: 0 = end of row; high bit set = skip
	// (cmd & 0x7F) transparent pixels; otherwise the byte is the
	// run-length and that many 8bpp palette indices follow, each
	// promoted via p16BPPPalette[] into RGB565.
	{
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		const UINT8* src = SrcPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (usZValue >= *rowZ) {
						*rowZ    = usZValue;
						*rowDest = p16BPPPalette[*src];
					}
					++src;
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZNB

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is
	NOT updated by this version. The Z-buffer is 16 bit, and	must be the same dimensions
	(including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNB( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// ETRLE row decoder, Z-tested but no Z update.
	{
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		const UINT8* src = SrcPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (*rowZ <= usZValue) {
						*rowDest = p16BPPPalette[*src];
					}
					++src;
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZNBColor

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is
	NOT updated by this version. The Z-buffer is 16 bit, and	must be the same dimensions
	(including Pitch) as the destination. Any pixels that fail the Z test, are written
	to with the specified color value.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBColor( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL usColor)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Portable TransZNBColor: when Z passes (sprite in front) draw
	// the normal palette colour; when Z fails (obscured) draw the
	// silhouette in usColor. Z buffer is never updated (NB).
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (*rowZ <= usZValue) {
						*rowDest = p16BPPPalette[*src];
					} else {
						*rowDest = PixFromColor16(usColor);
					}
					++src;
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadow

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. If the source pixel is 254,
	it is considered a shadow, and the destination buffer is darkened rather than blitted on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadow(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows) 
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Portable ETRLE TransShadow: src byte 254 = shadow (darken dest
	// via PixShade() unless fIgnoreShadows); anything else =
	// regular palette write. No Z buffer in this variant.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (v == 254) {
						if (!fIgnoreShadows) *rowDest = PixShade(*rowDest);
					} else {
						*rowDest = p16BPPPalette[v];
					}
					++rowDest;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadow

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. If the source pixel is 254,
	it is considered a shadow, and the destination buffer is darkened rather than blitted on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *AlphaPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(hAlphaVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF(iTempX >= 0);
	CHECKF(iTempY >= 0);


	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	AlphaPtr = (UINT8 *)hAlphaVObject->pPixData + (hAlphaVObject->pETRLEObject[usIndex]).uiDataOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * sizeof(PIXEL));
	LineSkip = (uiDestPitchBYTES - (usWidth * 2));

	// Portable TransShadowAlpha: alpha mask vobject is a byte-for-byte
	// parallel stream to the source ETRLE -- the legacy asm advances
	// AlphaPtr once per command byte and once per opaque payload byte,
	// matching the source stride exactly. Per opaque pixel: src 254 =
	// shadow darken (ignore alpha); else blendWithAlpha(palette[src],
	// existing dest, alpha-byte).
	{
		const UINT8* src   = SrcPtr;
		const UINT8* alpha = AlphaPtr;
		PIXEL*      dest  = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha; // consume one alpha byte per command byte
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					const UINT8 a = *alpha++;
					if (v == 254) {
						if (!fIgnoreShadows) *rowDest = PixShade(*rowDest);
					} else {
						*rowDest = blendWithAlpha(p16BPPPalette[v], *rowDest, a);
					}
					++rowDest;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZ

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. If the source pixel is 254,
	it is considered a shadow, and the destination buffer is darkened rather than blitted on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr, *AlphaPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(hAlphaVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF(iTempX >= 0);
	CHECKF(iTempY >= 0);


	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	AlphaPtr = (UINT8 *)hAlphaVObject->pPixData + (hAlphaVObject->pETRLEObject[usIndex]).uiDataOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * 2);
	LineSkip = (uiDestPitchBYTES - (usWidth * 2));

	// Portable TransShadowZAlpha: alpha-blended sprite with Z test +
	// Z update. Alpha mask stream is byte-for-byte parallel to src.
	{
		const UINT8* src   = SrcPtr;
		const UINT8* alpha = AlphaPtr;
		PIXEL*      dest  = (PIXEL *)DestPtr;
		UINT16*      zbuf  = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					const UINT8 a = *alpha++;
					if (usZValue >= *rowZ) {
						if (v == 254) {
							if (!fIgnoreShadows) *rowDest = PixShade(*rowDest);
						} else {
							*rowDest = blendWithAlpha(p16BPPPalette[v], *rowDest, a);
						}
						*rowZ = usZValue;
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZ

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. If the source pixel is 254,
	it is considered a shadow, and the destination buffer is darkened rather than blitted on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZ(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Portable TransShadowZ: src 254 = shadow darken; else palette
	// write. Z-test (>=), Z-update on accept.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (usZValue >= *rowZ) {
						if (v == 254) {
							if (!fIgnoreShadows) *rowDest = PixShade(*rowDest);
						} else {
							*rowDest = p16BPPPalette[v];
						}
						*rowZ = usZValue;
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNB

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is NOT
	updated. If the source pixel is 254, it is considered a shadow, and the destination
	buffer is darkened rather than blitted on. The Z-buffer is 16 bit, and must be the same
	dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNB(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows) 
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Portable TransShadowZNB: like TransShadowZ but no Z update.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (usZValue >= *rowZ) {
						if (v == 254) {
							if (!fIgnoreShadows) *rowDest = PixShade(*rowDest);
						} else {
							*rowDest = p16BPPPalette[v];
						}
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNB

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is NOT
	updated. If the source pixel is 254, it is considered a shadow, and the destination
	buffer is darkened rather than blitted on. The Z-buffer is 16 bit, and must be the same
	dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr, *AlphaPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(hAlphaVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF(iTempX >= 0);
	CHECKF(iTempY >= 0);


	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	AlphaPtr = (UINT8 *)hAlphaVObject->pPixData + (hAlphaVObject->pETRLEObject[usIndex]).uiDataOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * 2);
	LineSkip = (uiDestPitchBYTES - (usWidth * 2));

	// Portable TransShadowZNBAlpha: Z-tested alpha-blended sprite,
	// no Z write.
	{
		const UINT8* src   = SrcPtr;
		const UINT8* alpha = AlphaPtr;
		PIXEL*      dest  = (PIXEL *)DestPtr;
		UINT16*      zbuf  = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					const UINT8 a = *alpha++;
					if (usZValue >= *rowZ) {
						if (v == 254) {
							if (!fIgnoreShadows) *rowDest = PixShade(*rowDest);
						} else {
							*rowDest = blendWithAlpha(p16BPPPalette[v], *rowDest, a);
						}
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}




/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNBObscured

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is NOT
	updated. If the source pixel is 254, it is considered a shadow, and the destination
	buffer is darkened rather than blitted on. The Z-buffer is 16 bit, and must be the same
	dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscured(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows) 
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	UINT32 uiLineFlag;



	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	uiLineFlag=(iTempY&1);

	// Portable TransShadowZNBObscured: in-front pixels render
	// normally (with shadow); obscured pixels render as a
	// checkerboard silhouette (no shadow when obscured). Z buffer
	// not updated.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 lineFlag = uiLineFlag;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			INT32 xparity = iTempX & 1;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					xparity ^= (n & 1);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (*rowZ <= usZValue) {
						// In front
						if (v == 254) {
							if (*rowZ < usZValue && !fIgnoreShadows) {
								*rowDest = PixShade(*rowDest);
							}
						} else {
							*rowDest = p16BPPPalette[v];
						}
					} else {
						// Obscured: checkerboard silhouette
						if (v != 254 && lineFlag == (UINT32)xparity) {
							*rowDest = p16BPPPalette[v];
						}
					}
					++rowDest;
					++rowZ;
					xparity ^= 1;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNBObscured

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is NOT
	updated. If the source pixel is 254, it is considered a shadow, and the destination
	buffer is darkened rather than blitted on. The Z-buffer is 16 bit, and must be the same
	dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredTest(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	UINT32 uiLineFlag;



	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF(iTempX >= 0);
	CHECKF(iTempY >= 0);


	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * 2);
	LineSkip = (uiDestPitchBYTES - (usWidth * 2));
	uiLineFlag = (iTempY & 1);

	// Portable TransShadowZNBObscuredTest: like TransShadowZNBObscured
	// but the obscured-checkerboard pixel write blends at constant
	// alpha 0x7F (50%) instead of an opaque write.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 lineFlag = uiLineFlag;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			INT32 xparity = iTempX & 1;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					xparity ^= (n & 1);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (*rowZ <= usZValue) {
						if (v == 254) {
							if (*rowZ < usZValue && !fIgnoreShadows) {
								*rowDest = PixShade(*rowDest);
							}
						} else {
							*rowDest = p16BPPPalette[v];
						}
					} else {
						if (v != 254 && lineFlag == (UINT32)xparity) {
							*rowDest = blendWithAlpha(p16BPPPalette[v], *rowDest, 0x7F);
						}
					}
					++rowDest;
					++rowZ;
					xparity ^= 1;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNBObscured

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is NOT
	updated. If the source pixel is 254, it is considered a shadow, and the destination
	buffer is darkened rather than blitted on. The Z-buffer is 16 bit, and must be the same
	dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr, *AlphaPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	UINT32 uiLineFlag;

	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(hAlphaVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF(iTempX >= 0);
	CHECKF(iTempY >= 0);

	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	AlphaPtr = (UINT8 *)hAlphaVObject->pPixData + (hAlphaVObject->pETRLEObject[usIndex]).uiDataOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX * 2);
	LineSkip = (uiDestPitchBYTES - (usWidth * 2));
	uiLineFlag = (iTempY & 1);

	// Portable TransShadowZNBObscuredAlpha: in front -> alpha-blended
	// sprite (with shadow); obscured + checkerboard hit -> alpha-
	// blended pixel from the same source. Alpha stream is parallel.
	{
		const UINT8* src   = SrcPtr;
		const UINT8* alpha = AlphaPtr;
		PIXEL*      dest  = (PIXEL *)DestPtr;
		UINT16*      zbuf  = (UINT16*)ZPtr;
		UINT32 lineFlag = uiLineFlag;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			INT32 xparity = iTempX & 1;
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					xparity ^= (n & 1);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					const UINT8 a = *alpha++;
					if (*rowZ <= usZValue) {
						if (v == 254) {
							if (*rowZ < usZValue && !fIgnoreShadows) {
								*rowDest = PixShade(*rowDest);
							}
						} else {
							*rowDest = blendWithAlpha(p16BPPPalette[v], *rowDest, a);
						}
					} else {
						if (v != 254 && lineFlag == (UINT32)xparity) {
							*rowDest = blendWithAlpha(p16BPPPalette[v], *rowDest, a);
						}
					}
					++rowDest;
					++rowZ;
					xparity ^= 1;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination. Pixels with a value of
	254 are shaded instead of blitted.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZClip(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows) 
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable TransShadowZClip: clipped, Z-tested, Z update.
	// Src 254 = shadow darken; else palette write.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (usZValue >= rowZ[dx]) {
							if (v == 254) {
								if (!fIgnoreShadows) rowDest[dx] = PixShade(rowDest[dx]);
							} else {
								rowDest[dx] = p16BPPPalette[v];
							}
							rowZ[dx] = usZValue;
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination. Pixels with a value of
	254 are shaded instead of blitted.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZClipAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr, *AlphaPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(hAlphaVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if (clipregion == NULL)
	{
		ClipX1 = ClippingRect.iLeft;
		ClipY1 = ClippingRect.iTop;
		ClipX2 = ClippingRect.iRight;
		ClipY2 = ClippingRect.iBottom;
	}
	else
	{
		ClipX1 = clipregion->iLeft;
		ClipY1 = clipregion->iTop;
		ClipX2 = clipregion->iRight;
		ClipY2 = clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip = __min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip = __min(max(ClipX2, (iTempX + (INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip = __min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip = __min(__max(ClipY2, (iTempY + (INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength = ((INT32)usWidth - LeftSkip - RightSkip);
	BlitHeight = ((INT32)usHeight - TopSkip - BottomSkip);

	// check if whole thing is clipped
	if ((LeftSkip >= (INT32)usWidth) || (RightSkip >= (INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if ((TopSkip >= (INT32)usHeight) || (BottomSkip >= (INT32)usHeight))
		return(TRUE);

	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	AlphaPtr = (UINT8 *)hAlphaVObject->pPixData + (hAlphaVObject->pETRLEObject[usIndex]).uiDataOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY + TopSkip)) + ((iTempX + LeftSkip) * sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY + TopSkip)) + ((iTempX + LeftSkip) * 2);
	LineSkip = (uiDestPitchBYTES - (BlitLength * 2));

	// Portable TransShadowZClipAlpha: clipped alpha-blended shadow blit
	// with Z test + Z update.
	{
		const UINT8* src   = SrcPtr;
		const UINT8* alpha = AlphaPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) { src += cmd; alpha += cmd; }
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					const UINT8 a = *alpha++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (usZValue >= rowZ[dx]) {
							if (v == 254) {
								if (!fIgnoreShadows) rowDest[dx] = PixShade(rowDest[dx]);
							} else {
								rowDest[dx] = blendWithAlpha(p16BPPPalette[v], rowDest[dx], a);
							}
							rowZ[dx] = usZValue;
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination. Pixels with a value of
	254 are shaded instead of blitted.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowClip(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows) 
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable TransShadowClip: clipped, no Z buffer. Src 254 = shadow.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (v == 254) {
							if (!fIgnoreShadows) rowDest[dx] = PixShade(rowDest[dx]);
						} else {
							rowDest[dx] = p16BPPPalette[v];
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination. Pixels with a value of
	254 are shaded instead of blitted.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowClipAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *AlphaPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(hAlphaVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if (clipregion == NULL)
	{
		ClipX1 = ClippingRect.iLeft;
		ClipY1 = ClippingRect.iTop;
		ClipX2 = ClippingRect.iRight;
		ClipY2 = ClippingRect.iBottom;
	}
	else
	{
		ClipX1 = clipregion->iLeft;
		ClipY1 = clipregion->iTop;
		ClipX2 = clipregion->iRight;
		ClipY2 = clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip = __min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip = __min(max(ClipX2, (iTempX + (INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip = __min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip = __min(__max(ClipY2, (iTempY + (INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength = ((INT32)usWidth - LeftSkip - RightSkip);
	BlitHeight = ((INT32)usHeight - TopSkip - BottomSkip);

	// check if whole thing is clipped
	if ((LeftSkip >= (INT32)usWidth) || (RightSkip >= (INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if ((TopSkip >= (INT32)usHeight) || (BottomSkip >= (INT32)usHeight))
		return(TRUE);

	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	AlphaPtr = (UINT8 *)hAlphaVObject->pPixData + (hAlphaVObject->pETRLEObject[usIndex]).uiDataOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY + TopSkip)) + ((iTempX + LeftSkip) * sizeof(PIXEL));
	LineSkip = (uiDestPitchBYTES - (BlitLength * 2));

	// Portable TransShadowClipAlpha: clipped alpha-blended shadow blit.
	// Alpha stream is byte-for-byte parallel with source ETRLE stream.
	{
		const UINT8* src   = SrcPtr;
		const UINT8* alpha = AlphaPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) {
					src   += cmd;
					alpha += cmd;
				}
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					const UINT8 a = *alpha++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (v == 254) {
							if (!fIgnoreShadows) rowDest[dx] = PixShade(rowDest[dx]);
						} else {
							rowDest[dx] = blendWithAlpha(p16BPPPalette[v], rowDest[dx], a);
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNBClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the
	destination. Pixels with a value of	254 are shaded instead of blitted. The Z buffer is
	NOT updated.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBClip(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows) 
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable TransShadowZNBClip: clipped, Z-tested, no Z write.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (usZValue >= rowZ[dx]) {
							if (v == 254) {
								if (!fIgnoreShadows) rowDest[dx] = PixShade(rowDest[dx]);
							} else {
								rowDest[dx] = p16BPPPalette[v];
							}
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNBClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the
	destination. Pixels with a value of	254 are shaded instead of blitted. The Z buffer is
	NOT updated.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBClipAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr, *AlphaPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(hAlphaVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if (clipregion == NULL)
	{
		ClipX1 = ClippingRect.iLeft;
		ClipY1 = ClippingRect.iTop;
		ClipX2 = ClippingRect.iRight;
		ClipY2 = ClippingRect.iBottom;
	}
	else
	{
		ClipX1 = clipregion->iLeft;
		ClipY1 = clipregion->iTop;
		ClipX2 = clipregion->iRight;
		ClipY2 = clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip = __min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip = __min(max(ClipX2, (iTempX + (INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip = __min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip = __min(__max(ClipY2, (iTempY + (INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength = ((INT32)usWidth - LeftSkip - RightSkip);
	BlitHeight = ((INT32)usHeight - TopSkip - BottomSkip);

	// check if whole thing is clipped
	if ((LeftSkip >= (INT32)usWidth) || (RightSkip >= (INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if ((TopSkip >= (INT32)usHeight) || (BottomSkip >= (INT32)usHeight))
		return(TRUE);

	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	AlphaPtr = (UINT8 *)hAlphaVObject->pPixData + (hAlphaVObject->pETRLEObject[usIndex]).uiDataOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY + TopSkip)) + ((iTempX + LeftSkip) * sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY + TopSkip)) + ((iTempX + LeftSkip) * 2);
	LineSkip = (uiDestPitchBYTES - (BlitLength * 2));

	// Portable TransShadowZNBClipAlpha: clipped alpha-blended Z-test
	// (no Z write).
	{
		const UINT8* src   = SrcPtr;
		const UINT8* alpha = AlphaPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) { src += cmd; alpha += cmd; }
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					const UINT8 a = *alpha++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (usZValue >= rowZ[dx]) {
							if (v == 254) {
								if (!fIgnoreShadows) rowDest[dx] = PixShade(rowDest[dx]);
							} else {
								rowDest[dx] = blendWithAlpha(p16BPPPalette[v], rowDest[dx], a);
							}
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNBClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the
	destination. Pixels with a value of	254 are shaded instead of blitted. The Z buffer is
	NOT updated.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredClip(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted, uiLineFlag;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable TransShadowZNBObscuredClip: clipped variant of
	// TransShadowZNBObscured (in-front normal + obscured checkerboard).
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		UINT32 lineFlag = (UINT32)((iTempY + TopSkip) & 1);
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						const INT32 absX = iTempX + LeftSkip + dx;
						if (rowZ[dx] <= usZValue) {
							if (v == 254) {
								if (rowZ[dx] < usZValue && !fIgnoreShadows) {
									rowDest[dx] = PixShade(rowDest[dx]);
								}
							} else {
								rowDest[dx] = p16BPPPalette[v];
							}
						} else {
							if (v != 254 && lineFlag == (UINT32)(absX & 1)) {
								rowDest[dx] = p16BPPPalette[v];
							}
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNBClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the
	destination. Pixels with a value of	254 are shaded instead of blitted. The Z buffer is
	NOT updated.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredClipAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows)
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted, uiLineFlag;
	UINT8	*SrcPtr, *DestPtr, *ZPtr, *AlphaPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert(hSrcVObject != NULL);
	Assert(hAlphaVObject != NULL);
	Assert(pBuffer != NULL);

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight = (UINT32)pTrav->usHeight;
	usWidth = (UINT32)pTrav->usWidth;
	uiOffset = pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if (clipregion == NULL)
	{
		ClipX1 = ClippingRect.iLeft;
		ClipY1 = ClippingRect.iTop;
		ClipX2 = ClippingRect.iRight;
		ClipY2 = ClippingRect.iBottom;
	}
	else
	{
		ClipX1 = clipregion->iLeft;
		ClipY1 = clipregion->iTop;
		ClipX2 = clipregion->iRight;
		ClipY2 = clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip = __min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip = __min(max(ClipX2, (iTempX + (INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip = __min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip = __min(__max(ClipY2, (iTempY + (INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength = ((INT32)usWidth - LeftSkip - RightSkip);
	BlitHeight = ((INT32)usHeight - TopSkip - BottomSkip);

	// check if whole thing is clipped
	if ((LeftSkip >= (INT32)usWidth) || (RightSkip >= (INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if ((TopSkip >= (INT32)usHeight) || (BottomSkip >= (INT32)usHeight))
		return(TRUE);

	SrcPtr = (UINT8 *)hSrcVObject->pPixData + uiOffset;
	AlphaPtr = (UINT8 *)hAlphaVObject->pPixData + (hAlphaVObject->pETRLEObject[usIndex]).uiDataOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY + TopSkip)) + ((iTempX + LeftSkip) * sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY + TopSkip)) + ((iTempX + LeftSkip) * 2);
	LineSkip = (uiDestPitchBYTES - (BlitLength * 2));

	// Portable TransShadowZNBObscuredClipAlpha: clipped, alpha-blended,
	// Obscured (in-front alpha sprite / obscured checkerboard alpha
	// silhouette).
	{
		const UINT8* src   = SrcPtr;
		const UINT8* alpha = AlphaPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) { src += cmd; alpha += cmd; }
			}
		}
		UINT32 lineFlag = (UINT32)((iTempY + TopSkip) & 1);
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				++alpha;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					const UINT8 a = *alpha++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						const INT32 absX = iTempX + LeftSkip + dx;
						if (rowZ[dx] <= usZValue) {
							if (v == 254) {
								if (rowZ[dx] < usZValue && !fIgnoreShadows) {
									rowDest[dx] = PixShade(rowDest[dx]);
								}
							} else {
								rowDest[dx] = blendWithAlpha(p16BPPPalette[v], rowDest[dx], a);
							}
						} else {
							if (v != 254 && lineFlag == (UINT32)(absX & 1)) {
								rowDest[dx] = blendWithAlpha(p16BPPPalette[v], rowDest[dx], a);
							}
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransShadowZNBClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below OR EQUAL! that of the current pixel, it is written on.
	The Z-buffer is 16 bit, and	must be the same dimensions (including Pitch) as the
	destination. Pixels with a value of	254 are shaded instead of blitted. The Z buffer is
	NOT updated.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowBelowOrEqualZNBClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette )
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Clipped ETRLE: Z-test pass if zbuf <= usZValue (no Z update).
	// src==254 is the shadow marker -- darken dest via PixShade(),
	// but only when zbuf < usZValue (strictly less; equal skips).
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (rowZ[dx] <= usZValue) {
							if (v == 254) {
								if (rowZ[dx] < usZValue) {
									rowDest[dx] = PixShade(rowDest[dx]);
								}
							} else {
								rowDest[dx] = p16BPPPalette[v];
							}
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
		(void)LineSkip;
		(void)Unblitted;
		(void)LSCount;
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferShadowZ

	Creates a shadow using a brush, but modifies the destination buffer only if the current
	Z level is equal to higher than what's in the Z buffer at that pixel location. It
	updates the Z buffer with the new Z level.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZ( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Portable ETRLE shadow blit: instead of writing the source
	// palette colour, darken the existing destination pixel via
	// PixShade(oldDest). Z-buffer test, then update Z.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (*rowZ < usZValue) {
						*rowDest = PixShade(*rowDest);
						*rowZ    = usZValue;
					}
					++src;
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferShadowZClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable ETRLE shadow blit: clipped, Z-tested darken, Z update.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}

		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (rowZ[dx] < usZValue) {
							rowDest[dx] = PixShade(rowDest[dx]);
							rowZ[dx]    = usZValue;
						}
					}
					++src;
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferShadowZNB

	Creates a shadow using a brush, but modifies the destination buffer only if the current
	Z level is equal to higher than what's in the Z buffer at that pixel location. It does
	NOT update the Z buffer with the new Z value.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZNB( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Portable ETRLE shadow blit: Z-tested darken, no Z update.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					if (*rowZ < usZValue) {
						*rowDest = PixShade(*rowDest);
					}
					++src;
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferShadowZNBClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, the Z value is
	not updated,	for any non-transparent pixels. The Z-buffer is 16 bit, and	must be the
	same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZNBClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable ETRLE shadow blit: clipped, Z-tested darken, no Z write.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}

		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (rowZ[dx] < usZValue) {
							rowDest[dx] = PixShade(rowDest[dx]);
						}
					}
					++src;
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable ETRLE row decoder with clipping + Z-buffer test/update.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}

		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (usZValue >= rowZ[dx]) {
							rowZ[dx]    = usZValue;
							rowDest[dx] = p16BPPPalette[*src];
						}
					}
					++src;
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZNBClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is NOT
	updated in this version. The Z-buffer is 16 bit, and must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable ETRLE row decoder with clipping + Z test (no Z write).
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}

		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (rowZ[dx] <= usZValue) {
							rowDest[dx] = p16BPPPalette[*src];
						}
					}
					++src;
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZNBClipColor

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on. The Z value is NOT
	updated in this version. The Z-buffer is 16 bit, and must be the same dimensions (including
	Pitch) as the destination. Any pixels that fail the Z test are written to with the
	specified pixel value.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipColor( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL usColor)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable TransZNBClipColor: clipped silhouette tint (Z fail
	// draws in usColor; Z pass draws palette colour). No Z update.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						rowDest[dx] = (rowZ[dx] <= usZValue) ? p16BPPPalette[v] : PixFromColor16(usColor);
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataSubTo16BPPBuffer

	Blits a subrect from a flat 8 bit surface to a 16-bit buffer.

**********************************************************************************************/
BOOLEAN Blt8BPPDataSubTo16BPPBuffer( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVSURFACE hSrcVSurface, UINT8 *pSrcBuffer, UINT32 uiSrcPitch, INT32 iX, INT32 iY, SGPRect *pRect)
{
	PIXEL *p16BPPPalette;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip, LeftSkip, RightSkip, TopSkip, BlitLength, SrcSkip, BlitHeight;
	INT32	iTempX, iTempY;

	// Assertions
	Assert( hSrcVSurface != NULL );
	Assert( pSrcBuffer != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	usHeight				= (UINT32)hSrcVSurface->usHeight;
	usWidth					= (UINT32)hSrcVSurface->usWidth;

	// Add to start position of dest buffer
	iTempX = iX;
	iTempY = iY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );

	LeftSkip=pRect->iLeft;
	RightSkip=usWidth-pRect->iRight;
	TopSkip=pRect->iTop*uiSrcPitch;
	BlitLength=pRect->iRight-pRect->iLeft;
	BlitHeight=pRect->iBottom-pRect->iTop;
	SrcSkip=uiSrcPitch-BlitLength;

	SrcPtr= (UINT8 *)(pSrcBuffer+TopSkip+LeftSkip);
	DestPtr = ((UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL)));
	p16BPPPalette = hSrcVSurface->p16BPPPalette;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Raw (non-ETRLE) 8bpp subrect copy into 16bpp via palette LUT.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		for (UINT32 row = 0; row < BlitHeight; ++row) {
			for (UINT32 x = 0; x < BlitLength; ++x) {
				dest[x] = p16BPPPalette[src[x]];
			}
			src  += uiSrcPitch;
			dest  = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)SrcSkip;
		(void)LineSkip;
		(void)RightSkip;
	}

	return( TRUE );

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBuffer

	Blits from a flat surface to a 16-bit buffer.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBuffer( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVSURFACE hSrcVSurface, UINT8 *pSrcBuffer, INT32 iX, INT32 iY)
{
	PIXEL *p16BPPPalette;
//	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
//	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	UINT32 rows;

	// Assertions
	Assert( hSrcVSurface != NULL );
	Assert( pSrcBuffer != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	usHeight				= (UINT32)hSrcVSurface->usHeight;
	usWidth					= (UINT32)hSrcVSurface->usWidth;

	// Add to start position of dest buffer
	iTempX = iX;
	iTempY = iY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)pSrcBuffer;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVSurface->p16BPPPalette;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Portable Blt8BPPDataTo16BPPBuffer: not ETRLE -- pSrcBuffer is a
	// raw 8bpp pixel buffer (usWidth*usHeight bytes). Convert each
	// pixel through p16BPPPalette and write opaque to dest.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 _rows = usHeight;
		while (_rows-- > 0) {
			PIXEL* rowDest = dest;
			for (UINT32 x = 0; x < usWidth; ++x) {
				*rowDest++ = p16BPPPalette[*src++];
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)rows;  // silence unused-variable warning
	}

	return( TRUE );

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferHalf

	Blits from a flat surface to a 16-bit buffer, dividing the source image into
exactly half the size.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferHalf( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVSURFACE hSrcVSurface, UINT8 *pSrcBuffer, UINT32 uiSrcPitch, INT32 iX, INT32 iY)
{
	PIXEL *p16BPPPalette;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	INT32	iTempX, iTempY;
	UINT32 uiSrcSkip;

	// Assertions
	Assert( hSrcVSurface != NULL );
	Assert( pSrcBuffer != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	usHeight				= (UINT32)hSrcVSurface->usHeight;
	usWidth					= (UINT32)hSrcVSurface->usWidth;

	// Add to start position of dest buffer
	iTempX = iX;
	iTempY = iY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );

	SrcPtr= (UINT8 *)pSrcBuffer;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVSurface->p16BPPPalette;
	LineSkip=(uiDestPitchBYTES-(usWidth&0xfffffffe));
	uiSrcSkip=(uiSrcPitch*2)-(usWidth&0xfffffffe);

	// 2x downscale: sample every other pixel in X and every other row
	// in Y. Output dims are (usWidth/2, usHeight/2).
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		const UINT32 outW = usWidth / 2;
		const UINT32 outH = usHeight / 2;
		for (UINT32 row = 0; row < outH; ++row) {
			for (UINT32 x = 0; x < outW; ++x) {
				dest[x] = p16BPPPalette[src[x * 2]];
			}
			src  += uiSrcPitch * 2;
			dest  = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)LineSkip;
		(void)uiSrcSkip;
	}

	return( TRUE );

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferHalfRect

	Blits from a flat surface to a 16-bit buffer, dividing the source image into
exactly half the size, from a sub-region.
	- Source rect is in source units.
	- In order to make sure the same pixels are skipped, always align the top and
		left coordinates to the same factor of two.
	- A rect specifying an odd number of pixels will divide out to an even
		number of pixels blitted to the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferHalfRect( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVSURFACE hSrcVSurface, UINT8 *pSrcBuffer, UINT32 uiSrcPitch, INT32 iX, INT32 iY, SGPRect *pRect)
{
	PIXEL *p16BPPPalette;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	INT32	iTempX, iTempY;
	UINT32 uiSrcSkip;

	// Assertions
	Assert( hSrcVSurface != NULL );
	Assert( pSrcBuffer != NULL );
	Assert( pBuffer != NULL );
	Assert( pRect != NULL );

	// Get Offsets from Index into structure
	usWidth		= (UINT32)(pRect->iRight-pRect->iLeft);
	usHeight	= (UINT32)(pRect->iBottom-pRect->iTop);

	// Add to start position of dest buffer
	iTempX = iX;
	iTempY = iY;

	// Validations
	CHECKF( iTempX	>= 0 );
	CHECKF( iTempY	>= 0 );
	CHECKF(	usWidth	>	0 );
	CHECKF(	usHeight >	0 );
	CHECKF( usHeight <= hSrcVSurface->usHeight);
	CHECKF( usWidth <= hSrcVSurface->usWidth);

	SrcPtr				= (UINT8 *)pSrcBuffer + (uiSrcPitch*pRect->iTop) + (pRect->iLeft);
	DestPtr				= (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVSurface->p16BPPPalette;
	LineSkip			= (uiDestPitchBYTES-(usWidth&0xfffffffe));
	uiSrcSkip			= (uiSrcPitch*2)-(usWidth&0xfffffffe);

	// 2x downscale of a subrect: SrcPtr already points at pRect's
	// origin in source, so the same every-other-pixel/every-other-row
	// pattern as Half applies, just with usWidth/usHeight derived from
	// the rect.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		const UINT32 outW = usWidth / 2;
		const UINT32 outH = usHeight / 2;
		for (UINT32 row = 0; row < outH; ++row) {
			for (UINT32 x = 0; x < outW; ++x) {
				dest[x] = p16BPPPalette[src[x * 2]];
			}
			src  += uiSrcPitch * 2;
			dest  = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)LineSkip;
		(void)uiSrcSkip;
	}

	return( TRUE );

}

/****************************INCOMPLETE***********************************************/

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferMask

	Blits an image into the destination buffer, using an ETRLE brush as a source, another ETRLE
	for a mask, and a 16-bit buffer as a destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferMask(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, HVOBJECT hMaskObject, INT32 iMOX, INT32 iMOY, UINT16 usMask)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 uiMOffset;
	UINT32 usHeight, usWidth;
	UINT32 usMHeight, usMWidth;
	UINT8	*SrcPtr, *DestPtr, *MaskPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav						= &(hSrcVObject->pETRLEObject[usIndex]);
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Get Offsets from Index into structure for mask
	pTrav						=	&(hMaskObject->pETRLEObject[usMask]);
	usMHeight				= (UINT32)pTrav->usHeight;
	usMWidth				= (UINT32)pTrav->usWidth;
	uiMOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	MaskPtr= (UINT8 *)hMaskObject->pPixData + uiMOffset + (iMOY*usMWidth) + iMOX;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Legacy asm here did not actually reference MaskPtr/usMask -- it
	// was an identical copy of the Transparent blit. No callers
	// reference this function in the current tree either, but preserve
	// the observable behavior (plain ETRLE blit) for safety.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					*rowDest++ = p16BPPPalette[*src++];
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)MaskPtr;
		(void)usMHeight;
		(void)LineSkip;
	}

	return(TRUE);

}

void SetClippingRect(SGPRect *clip)
{
	Assert(clip!=NULL);
	Assert(clip->iLeft < clip->iRight);
	Assert(clip->iTop < clip->iBottom);

	memcpy(&ClippingRect, clip, sizeof(SGPRect));

}

void GetClippingRect(SGPRect *clip)
{
	Assert(clip!=NULL);

	memcpy(clip, &ClippingRect, sizeof(SGPRect));
}

/**********************************************************************************************
	Blt16BPPBufferPixelateRectWithColor

		Given an 8x8 pattern and a color, pixelates an area by repeatedly "applying the color" to pixels whereever there
		is a non-zero value in the pattern.

		KM:	Added Nov. 23, 1998
		This is all the code that I moved from Blt16BPPBufferPixelateRect().
		This function now takes a color field (which previously was
		always black.	The 3rd assembler line in this function:

				mov		ax, usColor				// color of pixel

		used to be:

				xor	eax, eax					// color of pixel (black or 0)

	This was the only internal modification I made other than adding the usColor argument.

*********************************************************************************************/
BOOLEAN Blt16BPPBufferPixelateRectWithColor(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area, UINT8 Pattern[8][8], PIXEL usColor )
{
	INT32	width, height;
	UINT32 LineSkip;
	PIXEL *DestPtr;
	INT32	iLeft, iTop, iRight, iBottom;

	// Assertions
	Assert( pBuffer != NULL );
	Assert( Pattern != NULL );

	iLeft=__max(ClippingRect.iLeft, area->iLeft);
	iTop=__max(ClippingRect.iTop, area->iTop);
	iRight=__min(ClippingRect.iRight-1, area->iRight);
	iBottom=__min(ClippingRect.iBottom-1, area->iBottom);

	DestPtr=(pBuffer+(iTop*(uiDestPitchBYTES/sizeof(PIXEL)))+iLeft);
	width=iRight-iLeft+1;
	height=iBottom-iTop+1;
	LineSkip=(uiDestPitchBYTES-(width*2));

	CHECKF(width >=1);
	CHECKF(height >=1);

	// Tile the 8x8 Pattern over the rect (anchored at the rect's
	// origin, not the buffer's): for each dest pixel at column x, row
	// y within the rect, write usColor iff Pattern[y%8][x%8] is set.
	{
		const PIXEL pxColor = PixFromColor16(usColor);
		const UINT8* pat = &Pattern[0][0];
		PIXEL* rowDest = DestPtr;
		for (INT32 y = 0; y < height; ++y) {
			const UINT8* patRow = pat + ((y & 7) * 8);
			for (INT32 x = 0; x < width; ++x) {
				if (patRow[x & 7] != 0) {
					rowDest[x] = pxColor;
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);
}

//KM:	Modified Nov. 23, 1998
//Original prototype (this function) didn't have a color field.	I've added the color field to
//Blt16BPPBufferPixelateRectWithColor(), moved the previous implementation of this function there, and added
//the modification to allow a specific color.
BOOLEAN Blt16BPPBufferPixelateRect(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area, UINT8 Pattern[8][8] )
{
	return Blt16BPPBufferPixelateRectWithColor( pBuffer, uiDestPitchBYTES, area, Pattern, 0 );
}

/**********************************************************************************************
	Blt16BPPBufferHatchRect

		A wrapper for Blt16BPPBufferPixelateRect(), it automatically sends a hatch pattern to it
		of the specified color

*********************************************************************************************/
BOOLEAN Blt16BPPBufferHatchRectWithColor(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area, PIXEL usColor )
{
	UINT8 Pattern[8][8] =
	{
		1,0,1,0,1,0,1,0,
		0,1,0,1,0,1,0,1,
		1,0,1,0,1,0,1,0,
		0,1,0,1,0,1,0,1,
		1,0,1,0,1,0,1,0,
		0,1,0,1,0,1,0,1,
		1,0,1,0,1,0,1,0,
		0,1,0,1,0,1,0,1
	};
	return Blt16BPPBufferPixelateRectWithColor( pBuffer, uiDestPitchBYTES, area, Pattern, usColor );
}

//Uses black hatch color
BOOLEAN Blt16BPPBufferHatchRect(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area )
{
	UINT8 Pattern[8][8] =
	{
		1,0,1,0,1,0,1,0,
		0,1,0,1,0,1,0,1,
		1,0,1,0,1,0,1,0,
		0,1,0,1,0,1,0,1,
		1,0,1,0,1,0,1,0,
		0,1,0,1,0,1,0,1,
		1,0,1,0,1,0,1,0,
		0,1,0,1,0,1,0,1
	};
	return Blt16BPPBufferPixelateRectWithColor( pBuffer, uiDestPitchBYTES, area, Pattern, 0 );
}

BOOLEAN Blt16BPPBufferLooseHatchRectWithColor(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area, PIXEL usColor )
{
	UINT8 Pattern[8][8] =
	{
		1,0,0,0,1,0,0,0,
		0,0,0,0,0,0,0,0,
		0,0,1,0,0,0,1,0,
		0,0,0,0,0,0,0,0,
		1,0,0,0,1,0,0,0,
		0,0,0,0,0,0,0,0,
		0,0,1,0,0,0,1,0,
		0,0,0,0,0,0,0,0,
	};
	return Blt16BPPBufferPixelateRectWithColor( pBuffer, uiDestPitchBYTES, area, Pattern, usColor );
}

BOOLEAN Blt16BPPBufferLooseHatchRect(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area )
{
	UINT8 Pattern[8][8] =
	{
		1,0,0,0,1,0,0,0,
		0,0,0,0,0,0,0,0,
		0,0,1,0,0,0,1,0,
		0,0,0,0,0,0,0,0,
		1,0,0,0,1,0,0,0,
		0,0,0,0,0,0,0,0,
		0,0,1,0,0,0,1,0,
		0,0,0,0,0,0,0,0,
	};
	return Blt16BPPBufferPixelateRectWithColor( pBuffer, uiDestPitchBYTES, area, Pattern, 0 );
}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferShadow

	Modifies the destination buffer. Darkens the destination pixels by 25%, using the source
	image as a mask. Any Non-zero index pixels are used to darken destination pixels.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferShadow( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Portable ShadowNoZ: darken dest via PixShade(*dest). Source
	// palette indices are read past for ETRLE stride but never used.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					*rowDest = PixShade(*rowDest);
					++rowDest;
				}
				src += cmd;  // skip the palette payload bytes
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransparent

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination.

**********************************************************************************************/

BOOLEAN Blt8BPPDataTo16BPPBufferTransparent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// ETRLE row decoder, no Z-buffer. Workhorse UI sprite blit -- the
	// legacy asm unrolled this 4-at-a-time but the compiler vectorizes
	// a tight inner loop just fine.
	{
		PIXEL* dest = (PIXEL *)DestPtr;
		const UINT8* src = SrcPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					*rowDest++ = p16BPPPalette[*src++];
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);

}



//*****************************************************************************************
// Blt8BPPDataTo16BPPBufferTransMirror
//
// Blits an 8bpp ETRLE to a 16-bit buffer, mirroring the image, with transparency.
//
// Returns BOOLEAN			- TRUE if successful
//
//	PIXEL *pBuffer			- 16bpp Destination buffer
// UINT32 uiDestPitchBYTES	- Destination pitch in bytes
// HVOBJECT hSrcVObject		- Source VOBJECT handle
// INT32 iX					- X-location of blit
// INT32 iY					- Y-location of blit
// UINT16 usIndex			 - VOBJECT image index to blit from
//
// Created:	7/28/99 Derek Beland
//*****************************************************************************************
BOOLEAN Blt8BPPDataTo16BPPBufferTransMirror( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 uiDestSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
//	iTempX = iX + pTrav->sOffsetX;
	iTempX = iX + usWidth - pTrav->sOffsetX-1;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	uiDestSkip=(uiDestPitchBYTES+(usWidth*2));

	// Portable ETRLE row decoder, mirrored. DestPtr already points at
	// the rightmost pixel of the first row; we write right-to-left
	// (dest--) for each opaque pixel, skip left for transparent runs,
	// and jump to the next row's rightmost pixel at end-of-row.
	// No clipping in this variant.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest -= (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					*rowDest-- = p16BPPPalette[*src++];
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransparentClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. Clips the brush.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransparentClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable ETRLE row decoder with clipping. Skip TopSkip rows of
	// input without writing, then for BlitHeight rows write pixels
	// whose source-X falls inside [LeftSkip, usWidth - RightSkip).
	{
		const UINT8* src = SrcPtr;

		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}

		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowBase = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					if (srcX >= LeftSkip && srcX < rightEdge) {
						rowBase[srcX - LeftSkip] = p16BPPPalette[*src];
					}
					++src;
				}
			}
			rowBase = (PIXEL *)((UINT8 *)rowBase + uiDestPitchBYTES);
		}
	}

	return(TRUE);

}




/**********************************************************************************************
 BltIsClipped

	Determines whether a given blit will need clipping or not. Returns TRUE/FALSE.

**********************************************************************************************/
BOOLEAN BltIsClipped(HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion )
{
	UINT32 usHeight, usWidth;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}


	// Calculate rows hanging off each side of the screen
	if(__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth))
		return(TRUE);

	if(__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth))
		return(TRUE);

	if(__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight))
		return(TRUE);

	if(__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight))
		return(TRUE);

	return(FALSE);
}



/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferShadowClip

	Modifies the destination buffer. Darkens the destination pixels by 25%, using the source
	image as a mask. Any Non-zero index pixels are used to darken destination pixels. Blitter
	clips brush if it doesn't fit on the viewport.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferShadowClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Portable ShadowClip: clipped variant of ShadowNoZ. Darkens dest.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = (INT32)usWidth - RightSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					++src;  // skip palette index byte (unused for shadow)
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						rowDest[dx] = PixShade(rowDest[dx]);
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
	}

	return(TRUE);

}



/**********************************************************************************************
	Blt16BPPBufferShadowRect

		Darkens a rectangular area by 25%. This blitter is used by ShadowVideoObjectRect.

	pBuffer						Pointer to a 16BPP buffer
	uiDestPitchBytes	Pitch of the destination surface
	area							An SGPRect, the area to darken

*********************************************************************************************/
BOOLEAN Blt16BPPBufferShadowRect(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area)
{
INT32	width, height;
UINT32 LineSkip;
PIXEL *DestPtr;

	// Assertions
	Assert( pBuffer != NULL );

	// Clipping
	if( area->iLeft < ClippingRect.iLeft )
		area->iLeft = ClippingRect.iLeft;
	if( area->iTop < ClippingRect.iTop )
		area->iTop = ClippingRect.iTop;
	if( area->iRight >= ClippingRect.iRight )
		area->iRight = ClippingRect.iRight - 1;
	if( area->iBottom >= ClippingRect.iBottom )
		area->iBottom = ClippingRect.iBottom - 1;
	//CHECKF(area->iLeft >= ClippingRect.iLeft );
	//CHECKF(area->iTop >= ClippingRect.iTop );
	//CHECKF(area->iRight <= ClippingRect.iRight );
	//CHECKF(area->iBottom <= ClippingRect.iBottom );

	DestPtr=(pBuffer+(area->iTop*(uiDestPitchBYTES/sizeof(PIXEL)))+area->iLeft);
	width=area->iRight-area->iLeft+1;
	height=area->iBottom-area->iTop+1;
	LineSkip=(uiDestPitchBYTES-(width*2));

	CHECKF(width >=1);
	CHECKF(height >=1);

	{
		PIXEL* rowDest = DestPtr;
		for (INT32 y = 0; y < height; ++y) {
			for (INT32 x = 0; x < width; ++x) {
				rowDest[x] = PixShade(rowDest[x]);
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);
}


/**********************************************************************************************
	Blt16BPPBufferShadowRect

		Darkens a rectangular area by 25%. This blitter is used by ShadowVideoObjectRect.

	pBuffer						Pointer to a 16BPP buffer
	uiDestPitchBytes	Pitch of the destination surface
	area							An SGPRect, the area to darken

*********************************************************************************************/
BOOLEAN Blt16BPPBufferShadowRectAlternateTable(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area)
{
INT32	width, height;
UINT32 LineSkip;
PIXEL *DestPtr;

	// Assertions
	Assert( pBuffer != NULL );

	// Clipping
	if( area->iLeft < ClippingRect.iLeft )
		area->iLeft = ClippingRect.iLeft;
	if( area->iTop < ClippingRect.iTop )
		area->iTop = ClippingRect.iTop;
	if( area->iRight >= ClippingRect.iRight )
		area->iRight = ClippingRect.iRight - 1;
	if( area->iBottom >= ClippingRect.iBottom )
		area->iBottom = ClippingRect.iBottom - 1;
	//CHECKF(area->iLeft >= ClippingRect.iLeft );
	//CHECKF(area->iTop >= ClippingRect.iTop );
	//CHECKF(area->iRight <= ClippingRect.iRight );
	//CHECKF(area->iBottom <= ClippingRect.iBottom );

	DestPtr=(pBuffer+(area->iTop*(uiDestPitchBYTES/sizeof(PIXEL)))+area->iLeft);
	width=area->iRight-area->iLeft+1;
	height=area->iBottom-area->iTop+1;
	LineSkip=(uiDestPitchBYTES-(width*2));

	CHECKF(width >=1);
	CHECKF(height >=1);

	{
		PIXEL* rowDest = DestPtr;
		for (INT32 y = 0; y < height; ++y) {
			for (INT32 x = 0; x < width; ++x) {
				rowDest[x] = PixIntensity(rowDest[x]);
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);
}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferMonoShadow

	Uses a bitmap an 8BPP template for blitting. Anywhere a 1 appears in the bitmap, a shadow
	is blitted to the destination (a black pixel). Any other value above zero is considered a
	forground color, and zero is background. If the parameter for the background color is zero,
	transparency is used for the background.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferMonoShadow( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, UINT16 usForeground, UINT16 usBackground)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					if (usBackground != 0) {
						for (UINT8 i = 0; i < n; ++i) *rowDest++ = usBackground;
					} else {
						rowDest += n;
					}
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (v >= 1) {
						*rowDest = 0;
					} else if (usBackground != 0) {
						*rowDest = usBackground;
					}
					++rowDest;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)usForeground;
		(void)p16BPPPalette;
		(void)LineSkip;
	}

	return(TRUE);

}

/*
BOOLEAN Blt8BPPDataTo16BPPBufferFullTransparent( HVOBJECT hDestVObject, HVOBJECT hSrcVObject, UINT16 usX, UINT16 usY, SGPRect *srcRect )
{
	UINT32 uiSrcStart, uiDestStart, uiNumLines, uiLineSize;
//	UINT32 rows, cols;
	UINT8 *pSrc; //, *pSrcTemp;
	PIXEL *pDest; //*pDestTemp,
	UINT32	uiSrcPitch, uiDestPitch;
	PIXEL *p16BPPPalette;
	UINT16 usEffectiveSrcWidth;
	UINT16 usEffectiveDestWidth;
	UINT16 us16BPPSrcTransColor;
	UINT16 us16BPPDestTransColor;
//	UINT16 us16BPPValue;
	UINT32 count;
	UINT8	maskcolor;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( hDestVObject != NULL );

	// Validations
	CHECKF( usX >= 0 );
	CHECKF( usY >= 0 );
	CHECKF( srcRect->iRight > srcRect->iLeft );
	CHECKF( srcRect->iBottom > srcRect->iTop );

	p16BPPPalette = hSrcVObject->p16BPPPalette;
	CHECKF( p16BPPPalette != NULL );

	// Lock Data
	pSrc = LockVideoObjectBuffer( hSrcVObject, &uiSrcPitch );

	// Effective width ( in PIXELS ) is Pitch ( in bytes ) converted to pitch ( IN PIXELS )
	usEffectiveSrcWidth = (UINT16)( uiSrcPitch / ( hSrcVObject->ubBitDepth / 8 ) );

	pDest = (UINT16*)LockVideoObjectBuffer( hDestVObject, &uiDestPitch );

	// Effective width ( in PIXELS ) is Pitch ( in bytes ) converted to pitch ( IN PIXELS )
	usEffectiveDestWidth = (UINT16)( uiDestPitch / ( hDestVObject->ubBitDepth / 8 ) );

	// Determine memcopy coordinates
	uiSrcStart = srcRect->iTop * usEffectiveSrcWidth + srcRect->iLeft;
	uiDestStart = usY * usEffectiveDestWidth + usX;
	uiNumLines = ( srcRect->iBottom - srcRect->iTop );
	uiLineSize = ( srcRect->iRight - srcRect->iLeft );

	CHECKF( hDestVObject->usWidth >= uiLineSize );
	CHECKF( hDestVObject->usHeight >= uiNumLines );

	// Find 16 BPP transparent color
	us16BPPSrcTransColor = Get16BPPColor( hSrcVObject->TransparentColor );
	for(count=0; (count < 256) && (p16BPPPalette[count]!=us16BPPSrcTransColor); count++);

	if(count==256)
	{
		DebugMsg(TOPIC_VIDEOOBJECT, DBG_LEVEL_2, String( "Transparency color does not exist in palette table for source object" ));
		maskcolor=0;
	}
	else
			maskcolor=(UINT8)count;

	us16BPPDestTransColor = Get16BPPColor( hDestVObject->TransparentColor );

	// Convert to Pixel specification
	pDest = pDest + uiDestStart;
	pSrc =	pSrc + uiSrcStart;

	// Portable FullTransparent: skip if dest is its transparent
	// colour OR if src is the mask colour (the palette index that
	// resolves to src's transparent RGB565).
	{
		for (UINT32 y = 0; y < uiNumLines; ++y) {
			for (UINT32 x = 0; x < uiLineSize; ++x) {
				if (pDest[x] == us16BPPDestTransColor) continue;
				if (pSrc[x]  == maskcolor)            continue;
				pDest[x] = p16BPPPalette[pSrc[x]];
			}
			pSrc  += uiSrcPitch;   // pSrc is UINT8*
			pDest = (PIXEL *)((UINT8 *)pDest + uiDestPitch);
		}
	}

	ReleaseVideoObjectBuffer( hSrcVObject );
	ReleaseVideoObjectBuffer( hDestVObject );

	return( TRUE );

}	*/




// UTILITY FUNCTIONS FOR BLITTING
/*
BOOLEAN ClipReleatedSrcAndDestRectangles( HVOBJECT hDestVObject, HVOBJECT hSrcVObject, RECT *DestRect, RECT *SrcRect )
{

	Assert( hDestVObject != NULL );
	Assert( hSrcVObject != NULL );

	// Check for invalid start positions and clip by ignoring blit
	if ( DestRect->iLeft >= hDestVObject->usWidth || DestRect->iTop >= hDestVObject->usHeight )
	{
		return( FALSE );
	}

	if ( SrcRect->iLeft >= hSrcVObject->usWidth || SrcRect->iTop >= hSrcVObject->usHeight )
	{
		return( FALSE );
	}

	// For overruns
	// Clip destination rectangles
	if ( DestRect->iRight > hDestVObject->usWidth )
	{
		// Both have to be modified or by default streching occurs
		DestRect->iRight = hDestVObject->usWidth;
		SrcRect->iRight = SrcRect->iLeft + ( DestRect->iRight - DestRect->iLeft );
	}
	if ( DestRect->iBottom > hDestVObject->usHeight )
	{
		// Both have to be modified or by default streching occurs
		DestRect->iBottom = hDestVObject->usHeight;
		SrcRect->iBottom = SrcRect->iTop + ( DestRect->iBottom - DestRect->iTop );
	}

	// Clip src rectangles
	if ( SrcRect->iRight > hSrcVObject->usWidth )
	{
		// Both have to be modified or by default streching occurs
		SrcRect->iRight = hSrcVObject->usWidth;
		DestRect->iRight = DestRect->iLeft	+ ( SrcRect->iRight - SrcRect->iLeft );
	}
	if ( SrcRect->iBottom > hSrcVObject->usHeight )
	{
		// Both have to be modified or by default streching occurs
		SrcRect->iBottom = hSrcVObject->usHeight;
		DestRect->iBottom = DestRect->iTop + ( SrcRect->iBottom - SrcRect->iTop );
	}

	// For underruns
	// Clip destination rectangles
	if ( DestRect->iLeft < 0 )
	{
		// Both have to be modified or by default streching occurs
		DestRect->iLeft = 0;
		SrcRect->iLeft = SrcRect->iRight - ( DestRect->iRight - DestRect->iLeft );
	}
	if ( DestRect->iTop < 0 )
	{
		// Both have to be modified or by default streching occurs
		DestRect->iTop = 0;
		SrcRect->iTop = SrcRect->iBottom - ( DestRect->iBottom - DestRect->iTop );
	}

	// Clip src rectangles
	if ( SrcRect->iLeft < 0 )
	{
		// Both have to be modified or by default streching occurs
		SrcRect->iLeft = 0;
		DestRect->iLeft = DestRect->iRight	- ( SrcRect->iRight - SrcRect->iLeft );
	}
	if ( SrcRect->iTop < 0 )
	{
		// Both have to be modified or by default streching occurs
		SrcRect->iTop = 0;
		DestRect->iTop = DestRect->iBottom - ( SrcRect->iBottom - SrcRect->iTop );
	}

	return( TRUE );
}


BOOLEAN FillSurface( HVOBJECT hDestVObject, blt_fx *pBltFx )
{
	DDBLTFX				BlitterFX;

	Assert( hDestVObject != NULL );
	CHECKF( pBltFx != NULL );

	BlitterFX.dwSize = sizeof( DDBLTFX );
	BlitterFX.dwFillColor = pBltFx->ColorFill;

	DDBltSurface( (LPDIRECTDRAWSURFACE2)hDestVObject->pSurfaceData, NULL, NULL, NULL, DDBLT_COLORFILL, &BlitterFX );

	if ( hDestVObject->fFlags & VOBJECT_VIDEO_MEM_USAGE && !hDestVObject->fFlags & VOBJECT_RESERVED_SURFACE )
	{
		UpdateBackupSurface( hDestVObject );
	}

	return( TRUE );
}

BOOLEAN FillSurfaceRect( HVOBJECT hDestVObject, blt_fx *pBltFx )
{
	DDBLTFX				BlitterFX;

	Assert( hDestVObject != NULL );
	CHECKF( pBltFx != NULL );

	BlitterFX.dwSize = sizeof( DDBLTFX );
	BlitterFX.dwFillColor = pBltFx->ColorFill;

	DDBltSurface( (LPDIRECTDRAWSURFACE2)hDestVObject->pSurfaceData, (LPRECT)&(pBltFx->FillRect), NULL, NULL, DDBLT_COLORFILL, &BlitterFX );

	if ( hDestVObject->fFlags & VOBJECT_VIDEO_MEM_USAGE && !hDestVObject->fFlags & VOBJECT_RESERVED_SURFACE )
	{
		UpdateBackupSurface( hDestVObject );
	}

	return( TRUE );
}


BOOLEAN BltVObjectUsingDD( HVOBJECT hDestVObject, HVOBJECT hSrcVObject, UINT32 fBltFlags, INT32 iDestX, INT32 iDestY, RECT *SrcRect )
{
	UINT32		uiDDFlags;
	RECT			DestRect;

	// Blit using the correct blitter
	if ( fBltFlags & VO_BLT_FAST )
	{

		// Validations
		CHECKF( iDestX >= 0 );
		CHECKF( iDestY >= 0 );

		// Default flags
		uiDDFlags = 0;

		// Convert flags into DD flags, ( for transparency use, etc )
		if ( fBltFlags & VO_BLT_USECOLORKEY )
		{
			uiDDFlags != DDBLTFAST_SRCCOLORKEY;
		}

		// Convert flags into DD flags, ( for transparency use, etc )
		if ( fBltFlags & VO_BLT_USEDESTCOLORKEY )
		{
			uiDDFlags != DDBLTFAST_DESTCOLORKEY;
		}

		if ( uiDDFlags == 0 )
		{
			// Default here is no colorkey
			uiDDFlags = DDBLTFAST_NOCOLORKEY;
		}

		DDBltFastSurface( (LPDIRECTDRAWSURFACE2)hDestVObject->pSurfaceData, iDestX, iDestY, (LPDIRECTDRAWSURFACE2)hSrcVObject->pSurfaceData, SrcRect, uiDDFlags );

	}
	else
	{
		// Normal, specialized blit for clipping, etc

		// Default flags
		uiDDFlags = DDBLT_WAIT;

		// Convert flags into DD flags, ( for transparency use, etc )
		if ( fBltFlags & VO_BLT_USECOLORKEY )
		{
			uiDDFlags |= DDBLT_KEYSRC;
		}

		// Setup dest rectangle
		DestRect.top =	(int)iDestY;
		DestRect.left = (int)iDestX;
		DestRect.bottom = (int)iDestY + ( SrcRect->iBottom - SrcRect->iTop );
		DestRect.right = (int)iDestX + ( SrcRect->iRight - SrcRect->iLeft );

		// Do Clipping of rectangles
		if ( !ClipReleatedSrcAndDestRectangles( hDestVObject, hSrcVObject, &DestRect, SrcRect ) )
		{
			// Returns false because dest start is > dest size
			return( TRUE );
		}

		DDBltSurface( (LPDIRECTDRAWSURFACE2)hDestVObject->pSurfaceData, &DestRect, (LPDIRECTDRAWSURFACE2)hSrcVObject->pSurfaceData,
							SrcRect, uiDDFlags, NULL );

	}

	// Update backup surface with new data
	if ( hDestVObject->fFlags & VOBJECT_VIDEO_MEM_USAGE && !hDestVObject->fFlags & VOBJECT_RESERVED_SURFACE )
	{
		UpdateBackupSurface( hDestVObject );
	}

	return( TRUE );
}


// Blt to backup buffer
BOOLEAN UpdateBackupSurface( HVOBJECT hVObject )
{
	RECT		aRect;

	// Assertions
	Assert( hVObject != NULL );

	// Validations
	CHECKF( hVObject->pSavedSurfaceData != NULL );

	aRect.top = (int)0;
	aRect.left = (int)0;
	aRect.bottom = (int)hVObject->usHeight;
	aRect.right = (int)hVObject->usWidth;

	// Copy all contents into backup buffer
	DDBltFastSurface( (LPDIRECTDRAWSURFACE2)hVObject->pSurfaceData, 0, 0, (LPDIRECTDRAWSURFACE2)hVObject->pSavedSurfaceData, &aRect, DDBLTFAST_NOCOLORKEY );

	return( TRUE );

}

*/


BOOLEAN FillRect16BPP(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, INT32 x1, INT32 y1, INT32 x2, INT32 y2, PIXEL color)
{
INT32		x1real, y1real, x2real, y2real;
UINT32	linelength, lines, lineskip;
PIXEL		*startoffset;

	// check parameters
	Assert(pBuffer!=NULL);
	Assert(uiDestPitchBYTES > 0);
	Assert(x2 > x1);
	Assert(y2 > y1);

	// clip edges of rect if hanging off screen

	x1real=__max(0, x1);
	x2real=__min(639, x2);
	y1real=__max(0, y1);
	y2real=__min(479, y2);

	startoffset=pBuffer+(y1real*uiDestPitchBYTES/sizeof(PIXEL))+x1real;
	lines=y2real-y1real+1;
	linelength=x2real-x1real+1;
	lineskip=uiDestPitchBYTES-(linelength*sizeof(PIXEL));

	// Portable FillRect16BPP.
	{
		const PIXEL pxColor = PixFromColor16(color);
		PIXEL* row = startoffset;
		for (UINT32 y = 0; y < lines; ++y) {
			for (UINT32 x = 0; x < linelength; ++x) row[x] = pxColor;
			row = (PIXEL*)((UINT8*)row + uiDestPitchBYTES);
		}
		(void)lineskip;
	}

	return(TRUE);
}


/**********************************************************************************************
 BltIsClippedOrOffScreen

	Determines whether a given blit will need clipping or not. Returns TRUE/FALSE.

**********************************************************************************************/
CHAR8 BltIsClippedOrOffScreen( HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion )
{
	UINT32 usHeight, usWidth;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}


	// Calculate rows hanging off each side of the screen
	gLeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	gRightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	gTopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	gBottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	gfUsePreCalcSkips = TRUE;

	// check if whole thing is clipped
	if((gLeftSkip >=(INT32)usWidth) || (gRightSkip >=(INT32)usWidth))
		return(-1 );

	// check if whole thing is clipped
	if((gTopSkip >=(INT32)usHeight) || (gBottomSkip >=(INT32)usHeight))
		return(-1 );


	if ( gLeftSkip )
		return( TRUE );

	if ( gRightSkip )
		return( TRUE );

	if ( gTopSkip )
		return( TRUE );

	if ( gBottomSkip )
		return( TRUE );


	return(FALSE);
}




// Blt8BPPDataTo16BPPBufferOutline
// ATE New blitter for rendering a differrent color for value 254. Can be transparent if fDoOutline is FALSE
BOOLEAN Blt8BPPDataTo16BPPBufferOutline( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline )
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	PIXEL *p16BPPPalette;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	p16BPPPalette = hSrcVObject->pShadeCurrent;

	// ETRLE blit with outline marker. src==254 means "outline pixel":
	// if fDoOutline write s16BPPColor, else skip (transparent).
	// Otherwise normal palette LUT blit.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (v == 254) {
						if (fDoOutline) {
							*rowDest = PixFromColor16((UINT16)s16BPPColor);
						}
					} else {
						*rowDest = p16BPPPalette[v];
					}
					++rowDest;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);

}


// ATE New blitter for rendering a differrent color for value 254. Can be transparent if fDoOutline is FALSE
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline, SGPRect *clipregion )
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;
	PIXEL *p16BPPPalette;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));
	p16BPPPalette = hSrcVObject->pShadeCurrent;

	// Clipped ETRLE with outline marker. src==254 + fDoOutline writes
	// s16BPPColor; src==254 + !fDoOutline is transparent.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (v == 254) {
							if (fDoOutline) {
								rowDest[dx] = PixFromColor16((UINT16)s16BPPColor);
							}
						} else {
							rowDest[dx] = p16BPPPalette[v];
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
		(void)LineSkip;
		(void)Unblitted;
		(void)LSCount;
	}

	return(TRUE);

}


BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline, SGPRect *clipregion )
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;
	PIXEL *p16BPPPalette;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);

	LineSkip=(uiDestPitchBYTES-(BlitLength*2));
	p16BPPPalette = hSrcVObject->pShadeCurrent;

	// Clipped ETRLE with Z test (Z update on opaque normal pixels) +
	// outline marker. Z test passes when usZValue >= zbuf. src==254
	// renders as s16BPPColor outline (no Z update); other src values
	// blit normally and update the Z buffer.
	//
	// Note: the legacy asm had a subtle bug where the z-write
	// 'mov [ebx], ax' executed after 'mov al, [esi]' clobbered al,
	// so the z-buffer received (usZ_high<<8 | src_byte) instead of
	// usZValue. Restoring the obvious intent here.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (usZValue >= rowZ[dx]) {
							if (v == 254) {
								if (fDoOutline) {
									rowDest[dx] = PixFromColor16((UINT16)s16BPPColor);
								}
							} else {
								rowZ[dx]    = usZValue;
								rowDest[dx] = p16BPPPalette[v];
							}
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
		(void)LineSkip;
		(void)Unblitted;
		(void)LSCount;
	}

	return(TRUE);

}


BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZPixelateObscuredClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline, SGPRect *clipregion )
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;
	PIXEL *p16BPPPalette;
	UINT32 uiLineFlag;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);

	LineSkip=(uiDestPitchBYTES-(BlitLength*2));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	uiLineFlag=(iTempY&1);


	// Clipped ETRLE with outline marker + pixelate-when-obscured.
	// Front-facing pixels (usZ >= zbuf) render normally and update Z.
	// Obscured pixels (usZ < zbuf) render only on a checkerboard
	// (uiLineFlag toggles per row, initial = iTempY & 1) -- destX
	// parity is taken from the dest byte cursor, matching legacy.
	// Obscured pixels do NOT update Z.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		UINT32 lineFlag = uiLineFlag;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX < LeftSkip || srcX >= rightEdge) continue;
					const INT32 dx = srcX - LeftSkip;
					const bool frontFacing = (usZValue >= rowZ[dx]);
					if (frontFacing) {
						rowZ[dx] = usZValue;
					} else {
						const UINT32 destParity =
							((UINT32)(uintptr_t)(rowDest + dx) >> 1) & 1u;
						if (lineFlag != destParity) continue;
					}
					if (v == 254) {
						if (fDoOutline) {
							rowDest[dx] = PixFromColor16((UINT16)s16BPPColor);
						}
					} else {
						rowDest[dx] = p16BPPPalette[v];
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
		(void)LineSkip;
		(void)Unblitted;
		(void)LSCount;
	}

	return(TRUE);

}



BOOLEAN Blt8BPPDataTo16BPPBufferOutlineShadow( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	PIXEL *p16BPPPalette;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	p16BPPPalette = hSrcVObject->pShadeCurrent;

	// Body-shadow companion to Outline: for each opaque non-outline
	// pixel (src != 254), darken dest via ShadeTable. Outline pixels
	// (src==254) are skipped so the outline ring is left untouched.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (v != 254) {
						*rowDest = PixShade(*rowDest);
					}
					++rowDest;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
		(void)LineSkip;
	}

	return(TRUE);

}



BOOLEAN Blt8BPPDataTo16BPPBufferOutlineShadowClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Clipped body-shadow companion: PixShade() every opaque pixel
	// in the clipped rect. NB the legacy asm does not check for the
	// outline marker (254) in the unrolled inner loop -- so unlike
	// the no-clip OutlineShadow, this variant darkens outline pixels
	// too. Preserved.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					(void)*src++;  // src byte unused -- shade dest unconditionally
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						rowDest[dx] = PixShade(rowDest[dx]);
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
		(void)LineSkip;
		(void)Unblitted;
	}

	return(TRUE);

}


BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZ( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));


	// Compressed sequence consist of multiple subsequences of transparent and non - transparent bytes(SirTech have transparent color as zero).
	// Every subsequence on transparent bytes is replaced by one byte with highest bit set to 1. Lower 7 bits hold number of transparent bytes.
	// If sequence of transparent bytes is longer than 127, then new byte for transparent bytes encoding used and so on.

	// One service byte used before subsequence of non - transparent bytes. Its highest bit is set to 0. 
	// Lower 7 bits hold number of non - transparent byte in subsequence. 
	// If non - transparent bytes subsequence exceeds 127, new service byte is used and so on.
	// Every row has zero byte on its end.


	// No-clip ETRLE Z-tested (Z update) + outline marker. Same shape
	// as OutlineZClip without the clip path. As with that variant,
	// the legacy asm corrupts the z-write -- restored to write the
	// obvious usZValue.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (usZValue >= *rowZ) {
						if (v == 254) {
							if (fDoOutline) {
								*rowDest = PixFromColor16((UINT16)s16BPPColor);
							}
						} else {
							*rowZ    = usZValue;
							*rowDest = p16BPPPalette[v];
						}
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);

}


BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZPixelateObscured( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;
	UINT32 uiLineFlag;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));
	uiLineFlag=(iTempY&1);

	// No-clip ETRLE with outline marker + pixelate-when-obscured.
	// NB the legacy asm uses 'jbe' for the obscured test (so equal Z
	// counts as obscured), unlike the clipped variant which uses 'jb'
	// (equal counts as front-facing). Preserved each variant's quirk.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		UINT32 lineFlag = uiLineFlag;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			UINT32 col = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					col     += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++col) {
					const UINT8 v = *src++;
					const bool frontFacing = (usZValue > *rowZ);
					bool render = true;
					if (frontFacing) {
						*rowZ = usZValue;
					} else {
						const UINT32 destParity =
							((UINT32)(uintptr_t)rowDest >> 1) & 1u;
						render = (lineFlag == destParity);
					}
					if (render) {
						if (v == 254) {
							if (fDoOutline) {
								*rowDest = PixFromColor16((UINT16)s16BPPColor);
							}
						} else {
							*rowDest = p16BPPPalette[v];
						}
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
		(void)LineSkip;
	}

	return(TRUE);

}


// This is the same as above, but DONOT WRITE to Z!
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZNB( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// No-clip ETRLE Z-tested but no Z update (NB = no buffer update),
	// with outline marker.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					const UINT8 v = *src++;
					if (usZValue >= *rowZ) {
						if (v == 254) {
							if (fDoOutline) {
								*rowDest = PixFromColor16((UINT16)s16BPPColor);
							}
						} else {
							*rowDest = p16BPPPalette[v];
						}
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
		(void)LineSkip;
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferIntensityZ

	Creates a shadow using a brush, but modifies the destination buffer only if the current
	Z level is equal to higher than what's in the Z buffer at that pixel location. It
	updates the Z buffer with the new Z level.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZ( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// Intensity blits ignore the sprite's pixel values -- the ETRLE
	// shape acts as a mask, and dest pixels under opaque src get
	// PixIntensity(dest) (a brightness-tweaked dest). Z test passes
	// when usZValue > zbuf (legacy 'jae BlitNTL5' bypasses on
	// zbuf>=usZ, so write when zbuf<usZ). Z is updated on write.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					(void)*src++;  // shape mask only -- src value unused
					if (*rowZ < usZValue) {
						*rowDest = PixIntensity(*rowDest);
						*rowZ    = usZValue;
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
		(void)LineSkip;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferIntensityZClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Clipped IntensityZ: shape from ETRLE acts as mask, dest pixels
	// get PixIntensity() applied where zbuf < usZValue, Z updated.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					(void)*src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (rowZ[dx] < usZValue) {
							rowZ[dx]    = usZValue;
							rowDest[dx] = PixIntensity(rowDest[dx]);
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
		(void)LineSkip;
		(void)Unblitted;
		(void)LSCount;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferIntensityZNB

	Creates a shadow using a brush, but modifies the destination buffer only if the current
	Z level is equal to higher than what's in the Z buffer at that pixel location. It does
	NOT update the Z buffer with the new Z value.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZNB( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex )
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;


	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// IntensityZNB: same as IntensityZ but no Z update.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT16* zbuf = (UINT16*)ZPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			UINT16* rowZ    = zbuf;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					const UINT8 n = cmd & 0x7F;
					rowDest += n;
					rowZ    += n;
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					(void)*src++;
					if (*rowZ < usZValue) {
						*rowDest = PixIntensity(*rowDest);
					}
					++rowDest;
					++rowZ;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
			zbuf = (UINT16*)((UINT8*)zbuf + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
		(void)LineSkip;
	}

	return(TRUE);

}

/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferIntensityZNBClip

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, the Z value is
	not updated,	for any non-transparent pixels. The Z-buffer is 16 bit, and	must be the
	same dimensions (including Pitch) as the destination.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZNBClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr, *ZPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Clipped IntensityZ with no Z update.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					(void)*src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						if (rowZ[dx] < usZValue) {
							rowDest[dx] = PixIntensity(rowDest[dx]);
						}
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
		(void)LineSkip;
		(void)Unblitted;
		(void)LSCount;
	}

	return(TRUE);

}



/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferIntensityClip

	Modifies the destination buffer. Darkens the destination pixels by 25%, using the source
	image as a mask. Any Non-zero index pixels are used to darken destination pixels. Blitter
	clips brush if it doesn't fit on the viewport.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth, Unblitted;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight;
	INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));

	// Clipped IntensityClip (no Z): shape from ETRLE masks where to
	// apply PixIntensity() to dest.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					(void)*src++;
					if (srcX >= LeftSkip && srcX < rightEdge) {
						const INT32 dx = srcX - LeftSkip;
						rowDest[dx] = PixIntensity(rowDest[dx]);
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
		(void)LineSkip;
		(void)Unblitted;
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferIntensity

	Modifies the destination buffer. Darkens the destination pixels by 25%, using the source
	image as a mask. Any Non-zero index pixels are used to darken destination pixels.

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferIntensity( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex)
{
	PIXEL *p16BPPPalette;
	UINT32 uiOffset;
	UINT32 usHeight, usWidth;
	UINT8	*SrcPtr, *DestPtr;
	UINT32 LineSkip;
	ETRLEObject *pTrav;
	INT32	iTempX, iTempY;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	// Validations
	CHECKF( iTempX >= 0 );
	CHECKF( iTempY >= 0 );


	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*iTempY) + (iTempX*sizeof(PIXEL));
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(usWidth*2));

	// No-clip Intensity (no Z): shape from ETRLE masks where to apply
	// PixIntensity() to dest pixels.
	{
		const UINT8* src = SrcPtr;
		PIXEL* dest = (PIXEL *)DestPtr;
		UINT32 rows = usHeight;
		while (rows-- > 0) {
			PIXEL* rowDest = dest;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					rowDest += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i) {
					(void)*src++;
					*rowDest = PixIntensity(*rowDest);
					++rowDest;
				}
			}
			dest = (PIXEL *)((UINT8 *)dest + uiDestPitchBYTES);
		}
		(void)p16BPPPalette;
		(void)LineSkip;
	}

	return(TRUE);

}


/**********************************************************************************************
 Blt8BPPDataTo16BPPBufferTransZClipPixelateObscured

	Blits an image into the destination buffer, using an ETRLE brush as a source, and a 16-bit
	buffer as a destination. As it is blitting, it checks the Z value of the ZBuffer, and if the
	pixel's Z level is below that of the current pixel, it is written on, and the Z value is
	NOT updated to the current value,	for any non-transparent pixels. The Z-buffer is 16 bit, and
	must be the same dimensions (including Pitch) as the destination.

	Blits every second pixel ("pixelates").

**********************************************************************************************/
BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipPixelateObscured( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion)
{
PIXEL *p16BPPPalette;
UINT32 uiOffset, uiLineFlag;
UINT32 usHeight, usWidth, Unblitted;
UINT8	*SrcPtr, *DestPtr, *ZPtr;
UINT32 LineSkip;
ETRLEObject *pTrav;
INT32	iTempX, iTempY, LeftSkip, RightSkip, TopSkip, BottomSkip, BlitLength, BlitHeight, LSCount;
INT32	ClipX1, ClipY1, ClipX2, ClipY2;

	// Assertions
	Assert( hSrcVObject != NULL );
	Assert( pBuffer != NULL );

	// Get Offsets from Index into structure
	pTrav = &(hSrcVObject->pETRLEObject[ usIndex ] );
	usHeight				= (UINT32)pTrav->usHeight;
	usWidth					= (UINT32)pTrav->usWidth;
	uiOffset				= pTrav->uiDataOffset;

	// Add to start position of dest buffer
	iTempX = iX + pTrav->sOffsetX;
	iTempY = iY + pTrav->sOffsetY;

	if(clipregion==NULL)
	{
		ClipX1=ClippingRect.iLeft;
		ClipY1=ClippingRect.iTop;
		ClipX2=ClippingRect.iRight;
		ClipY2=ClippingRect.iBottom;
	}
	else
	{
		ClipX1=clipregion->iLeft;
		ClipY1=clipregion->iTop;
		ClipX2=clipregion->iRight;
		ClipY2=clipregion->iBottom;
	}

	// Calculate rows hanging off each side of the screen
	LeftSkip=__min(ClipX1 - min(ClipX1, iTempX), (INT32)usWidth);
	RightSkip=__min(max(ClipX2, (iTempX+(INT32)usWidth)) - ClipX2, (INT32)usWidth);
	TopSkip=__min(ClipY1 - __min(ClipY1, iTempY), (INT32)usHeight);
	BottomSkip=__min(__max(ClipY2, (iTempY+(INT32)usHeight)) - ClipY2, (INT32)usHeight);

	// calculate the remaining rows and columns to blit
	BlitLength=((INT32)usWidth-LeftSkip-RightSkip);
	BlitHeight=((INT32)usHeight-TopSkip-BottomSkip);

	// check if whole thing is clipped
	if((LeftSkip >=(INT32)usWidth) || (RightSkip >=(INT32)usWidth))
		return(TRUE);

	// check if whole thing is clipped
	if((TopSkip >=(INT32)usHeight) || (BottomSkip >=(INT32)usHeight))
		return(TRUE);

	SrcPtr= (UINT8 *)hSrcVObject->pPixData + uiOffset;
	DestPtr = (UINT8 *)pBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*sizeof(PIXEL));
	ZPtr = (UINT8 *)pZBuffer + (uiDestPitchBYTES*(iTempY+TopSkip)) + ((iTempX+LeftSkip)*2);
	p16BPPPalette = hSrcVObject->pShadeCurrent;
	LineSkip=(uiDestPitchBYTES-(BlitLength*2));
	uiLineFlag=(iTempY&1);

	// Clipped TransZ with pixelate-on-obscured. Z test: usZValue > zbuf
	// renders normally and updates Z; usZValue <= zbuf renders only on
	// a checkerboard mask (no Z update). The legacy asm toggles
	// uiLineFlag for every row including top-skipped ones, so by the
	// first rendered row lineFlag = (iTempY + TopSkip) & 1 = absY & 1.
	{
		const UINT8* src = SrcPtr;
		for (INT32 i = 0; i < TopSkip; ++i) {
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (!(cmd & 0x80)) src += cmd;
			}
		}
		const INT32 rightEdge = BlitLength + LeftSkip;
		PIXEL* rowDest = (PIXEL *)DestPtr;
		UINT16* rowZ    = (UINT16*)ZPtr;
		UINT32 lineFlag = (uiLineFlag ^ ((UINT32)TopSkip & 1u));
		for (INT32 row = 0; row < BlitHeight; ++row) {
			INT32 srcX = 0;
			for (;;) {
				const UINT8 cmd = *src++;
				if (cmd == 0) break;
				if (cmd & 0x80) {
					srcX += (cmd & 0x7F);
					continue;
				}
				for (UINT8 i = 0; i < cmd; ++i, ++srcX) {
					const UINT8 v = *src++;
					if (srcX < LeftSkip || srcX >= rightEdge) continue;
					const INT32 dx = srcX - LeftSkip;
					bool render;
					if (usZValue > rowZ[dx]) {
						rowZ[dx] = usZValue;
						render = true;
					} else {
						const UINT32 destParity =
							((UINT32)(uintptr_t)(rowDest + dx) >> 1) & 1u;
						render = (lineFlag == destParity);
					}
					if (render) {
						rowDest[dx] = p16BPPPalette[v];
					}
				}
			}
			rowDest = (PIXEL *)((UINT8 *)rowDest + uiDestPitchBYTES);
			rowZ    = (UINT16*)((UINT8*)rowZ    + uiDestPitchBYTES);
			lineFlag ^= 1;
		}
		(void)LineSkip;
		(void)Unblitted;
		(void)LSCount;
	}

	return(TRUE);

}
