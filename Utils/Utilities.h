#ifndef _UTILITIES_H_
#define _UTILITIES_H_

#include "Overhead Types.h"
#include "sgp.h"

#include <cstddef>


// WANNE: Maximum number of characters in german description (German xml files)
#define MAXLINE		200

BOOLEAN CreateSGPPaletteFromCOLFile( SGPPaletteEntry *pPalette, SGPFILENAME ColFile );
BOOLEAN DisplayPaletteRep( PaletteRepID aPalRep, UINT8 ubXPos, UINT8 ubYPos, UINT32 uiDestSurface );

BOOLEAN FilenameForBPP(
	STR pFilename, CHAR8* pDestination, std::size_t destinationCapacity);
template <std::size_t Capacity>
inline BOOLEAN FilenameForBPP(STR pFilename, CHAR8 (&pDestination)[Capacity])
{
	return FilenameForBPP(pFilename, pDestination, Capacity);
}

BOOLEAN WrapString(
	CHAR16* pStr, std::size_t strCapacity,
	CHAR16* pStr2, std::size_t str2Capacity,
	UINT16 usWidth, INT32 uiFont);
template <std::size_t FirstCapacity, std::size_t SecondCapacity>
inline BOOLEAN WrapString(
	CHAR16 (&pStr)[FirstCapacity], CHAR16 (&pStr2)[SecondCapacity],
	UINT16 usWidth, INT32 uiFont)
{
	return WrapString(
		pStr, FirstCapacity, pStr2, SecondCapacity, usWidth, uiFont);
}

BOOLEAN HandleJA2CDCheck( );
BOOLEAN DoJA2FilesExistsOnDrive(const CHAR8* zCdLocation);


// Snap: integer division that rounds the result to the nearest integer
template<class Integer>
inline Integer idiv(Integer a, Integer b)
{
	return a > 0 ? b > 0 ? (a + b/2) / b : (a - b/2) / b :
				b > 0 ? (a - b/2) / b : (a + b/2) / b ;
}

#endif
