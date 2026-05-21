#ifndef __VOBJECT_BLITTERS
#define __VOBJECT_BLITTERS

#include "shading.h"
#include "pixfmt.h"   // PIXEL (Phase 6b)
#include "vsurface.h"
#include "vobject.h"

/*
#ifdef __cplusplus
extern "C" {
#endif
*/

class ClipRectangle
{
public:
	enum ClipType
	{
		NoClip, PartialClip, FullClip,
	};
public:
	ClipRectangle();

	void SetRect(SGPRect const& rect);
	void SetRect(unsigned int w, unsigned int h, int x=0, int y=0);
	void Set(int x1, int y1, int x2, int y2);

	/**
	 * @returns: 
	 *   NoClip      : value references are not modified
	 *   FullClip    : value references are not modified, as the values lie completely outside. Why bother doing the extra work.
	 *   PartialClip : value references are modified 
	 */
	ClipType Clip(int& x, int& y, unsigned int& w, unsigned int& h);
	ClipType Clip(int& x1, int& y1, int& x2, int& y2);
private:
	SGPRect	cr;
};

extern SGPRect		ClippingRect;
extern UINT32			guiTranslucentMask;
extern PIXEL			White16BPPPalette[ 256 ];

PIXEL blendWithAlpha(unsigned int colNew, unsigned int colOld, unsigned int alpha);

extern void SetClippingRect(SGPRect *clip);
void GetClippingRect(SGPRect *clip);

BOOLEAN BltIsClipped(HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
CHAR8 BltIsClippedOrOffScreen( HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion );


UINT16 *InitZBuffer(UINT32 uiPitch, UINT32 uiHeight);
BOOLEAN ShutdownZBuffer(UINT16 *pBuffer);


// 8-Bit to 16-Bit Blitters
BOOLEAN Blt8BPPDataTo16BPPBufferTransMirror( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );

BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBColor( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL usColor);
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipColor( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL usColor);

// pixelation blitters
BOOLEAN Blt8BPPDataTo16BPPBufferTransZPixelate( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipPixelate( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBPixelate( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipPixelate( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);


// translucency blitters
BOOLEAN Blt8BPPDataTo16BPPBufferTransZTranslucent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipTranslucent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBTranslucent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClipTranslucent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);

BOOLEAN Blt8BPPDataTo16BPPBufferMonoShadowClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL usForeground, PIXEL usBackground, PIXEL usShadow );

BOOLEAN Blt8BPPDataTo16BPPBufferTransZ( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNB( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferTransZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferTransZNBClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZ(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowClip(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowClipAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNB(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows); 
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZNB( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZNBClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZ( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferShadowZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZClip(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZClipAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBClip(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBClipAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);

// Next blitters are for blitting mask as intensity
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZNB( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZNBClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZ( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferIntensityClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferIntensity( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );


BOOLEAN Blt8BPPDataTo16BPPBufferTransparentClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferTransparent( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadow(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);

BOOLEAN Blt8BPPDataTo16BPPBufferShadowClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);

BOOLEAN Blt8BPPTo8BPP(UINT8 *pDest, UINT32 uiDestPitch, UINT8 *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight);
BOOLEAN Blt16BPPTo16BPP(PIXEL *pDest, UINT32 uiDestPitch, PIXEL *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight);
BOOLEAN Blt16BPPTo16BPPTrans(PIXEL *pDest, UINT32 uiDestPitch, PIXEL *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight, UINT16 usTrans);
BOOLEAN Blt16BPPTo16BPPTransShadow(PIXEL *pDest, UINT32 uiDestPitch, PIXEL *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight, UINT16 usTrans);
BOOLEAN Blt16BPPTo16BPPMirror(PIXEL *pDest, UINT32 uiDestPitch, PIXEL *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight);


BOOLEAN Blt16BPPBufferPixelateRectWithColor( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area, UINT8 Pattern[8][8], PIXEL usColor );
BOOLEAN Blt16BPPBufferPixelateRect(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area, UINT8 Pattern[8][8]);

//A wrapper for the Blt16BPPBufferPixelateRect that automatically passes a hatch pattern.
BOOLEAN Blt16BPPBufferHatchRectWithColor(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area, PIXEL usColor );
BOOLEAN Blt16BPPBufferHatchRect(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area );
BOOLEAN Blt16BPPBufferLooseHatchRectWithColor(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area, PIXEL usColor );
BOOLEAN Blt16BPPBufferLooseHatchRect(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area );

BOOLEAN Blt16BPPBufferShadowRect(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area);

BOOLEAN Blt8BPPDataTo16BPPBufferShadow( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );

BOOLEAN Blt8BPPDataTo16BPPBuffer( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVSURFACE hSrcVSurface, UINT8 *pSrcBuffer, INT32 iX, INT32 iY);
BOOLEAN Blt8BPPDataSubTo16BPPBuffer( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVSURFACE hSrcVSurface, UINT8 *pSrcBuffer, UINT32 uiSrcPitch, INT32 iX, INT32 iY, SGPRect *pRect);

// Blits from flat 8bpp source, to 16bpp dest, divides in half
BOOLEAN Blt8BPPDataTo16BPPBufferHalf( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVSURFACE hSrcVSurface, UINT8 *pSrcBuffer, UINT32 uiSrcPitch, INT32 iX, INT32 iY);
BOOLEAN Blt8BPPDataTo16BPPBufferHalfRect( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVSURFACE hSrcVSurface, UINT8 *pSrcBuffer, UINT32 uiSrcPitch, INT32 iX, INT32 iY, SGPRect *pRect);


BOOLEAN BlitZRect(UINT16 *pZBuffer, UINT32 uiPitch, INT16 sLeft, INT16 sTop, INT16 sRight, INT16 sBottom, UINT16 usZValue);

// New 16/16 blitters

BOOLEAN Blt32BPPTo16BPPTrans(PIXEL *pDest, UINT32 uiDestPitch, UINT32 *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight);
BOOLEAN Blt32BPPTo16BPPTransShadow(PIXEL *pDest, UINT32 uiDestPitch, UINT32 *pSrc, UINT32 uiSrcPitch, INT32 iDestXPos, INT32 iDestYPos, INT32 iSrcXPos, INT32 iSrcYPos, UINT32 uiWidth, UINT32 uiHeight);

BOOLEAN Blt16BPPDataTo16BPPBufferTransparentClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt16BPPDataTo16BPPBufferTransZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);

// ATE: New blitters for showing an outline at color 254
BOOLEAN Blt8BPPDataTo16BPPBufferOutline( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline );
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline, SGPRect *clipregion );
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZ( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline );
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineShadow( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineShadowClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion );
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZNB( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline );
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZPixelateObscured( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline );
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZPixelateObscuredClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline, SGPRect *clipregion );
BOOLEAN Blt8BPPDataTo16BPPBufferOutlineZClip( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL s16BPPColor, BOOLEAN fDoOutline, SGPRect *clipregion );


// ATE: New blitter for included shadow, but pixellate if obscured by z
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscured(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows = FALSE);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredTest(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows = FALSE);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows = FALSE);

BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredClip(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransShadowZNBObscuredClipAlpha(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, HVOBJECT hAlphaVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion, PIXEL *p16BPPPalette, BOOLEAN fIgnoreShadows);
BOOLEAN Blt8BPPDataTo16BPPBufferTransZClipPixelateObscured( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex, SGPRect *clipregion);
BOOLEAN Blt8BPPDataTo16BPPBufferTransZPixelateObscured( PIXEL *pBuffer, UINT32 uiDestPitchBYTES, UINT16 *pZBuffer, UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );

BOOLEAN FillRect16BPP(PIXEL *pBuffer, UINT32 uiDestPitchBYTES, INT32 x1, INT32 y1, INT32 x2, INT32 y2, PIXEL color);

/*
#ifdef __cplusplus
}
#endif
*/
#endif
