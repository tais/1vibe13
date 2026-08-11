#include "STIConvert.h"
#include "ImageUtilityModel.h"
#include "MemMan.h"

#include <vfs/Core/vfs.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

BOOLEAN ConvertToETRLE( UINT8 ** ppDest, UINT32 * puiDestLen, UINT8 ** ppSubImageBuffer, UINT16 * pusNumberOfSubImages, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, UINT32 fFlags );

#define CONVERT_ETRLE_COMPRESS_SINGLE					0x0040
#define CONVERT_ETRLE_NO_SUBIMAGE_SHRINKING		0x0080
#define CONVERT_ETRLE_DONT_SKIP_BLANKS				0x0100
namespace
{
	static_assert(STCI_HEADER_SIZE == 64,
		"STCI header wire size must remain byte-compatible");
	static_assert(STCI_SUBIMAGE_SIZE == 16,
		"STCI subimage wire size must remain byte-compatible");
	static_assert(STCI_PALETTE_ELEMENT_SIZE == 3,
		"STCI palette wire size must remain byte-compatible");
	static_assert(sizeof(STCIHeader) >= STCI_HEADER_SIZE &&
		offsetof(STCIHeader, usHeight) == 20 &&
		offsetof(STCIHeader, usWidth) == 22 &&
		offsetof(STCIHeader, ubDepth) == 44 &&
		offsetof(STCIHeader, uiAppDataSize) == 48,
		"The legacy STI reader ABI must match the explicit wire encoder");
	static_assert(sizeof(STCISubImage) == STCI_SUBIMAGE_SIZE &&
		offsetof(STCISubImage, uiDataOffset) == 0 &&
		offsetof(STCISubImage, uiDataLength) == 4 &&
		offsetof(STCISubImage, sOffsetX) == 8 &&
		offsetof(STCISubImage, sOffsetY) == 10 &&
		offsetof(STCISubImage, usHeight) == 12 &&
		offsetof(STCISubImage, usWidth) == 14,
		"The legacy STI subimage ABI must match the explicit wire encoder");

	struct MemFreeDeleter
	{
		void operator()(void* pointer) const noexcept
		{
			if (pointer) MemFree(pointer);
		}
	};

	template<typename T>
	using MemOwner = std::unique_ptr<T, MemFreeDeleter>;

	bool AppendBytes(std::vector<UINT8>& destination,
		const void* source, std::size_t size)
	{
		if ((size && !source) ||
			!UtilsImageUtilityModel::CanAppendSerializedBytes(
				destination.size(), size))
		{
			return false;
		}
		if (size == 0) return true;
		const UINT8* const bytes = static_cast<const UINT8*>(source);
		destination.insert(destination.end(), bytes, bytes + size);
		return true;
	}

	bool AppendZeros(std::vector<UINT8>& destination, std::size_t size)
	{
		if (!UtilsImageUtilityModel::CanAppendSerializedBytes(
			destination.size(), size))
		{
			return false;
		}
		destination.insert(destination.end(), size, 0);
		return true;
	}

	bool AppendLittleEndian16(std::vector<UINT8>& destination, UINT16 value)
	{
		const UINT8 bytes[] = {
			static_cast<UINT8>(value),
			static_cast<UINT8>(value >> 8)
		};
		return AppendBytes(destination, bytes, sizeof(bytes));
	}

	bool AppendLittleEndian32(std::vector<UINT8>& destination, UINT32 value)
	{
		const UINT8 bytes[] = {
			static_cast<UINT8>(value),
			static_cast<UINT8>(value >> 8),
			static_cast<UINT8>(value >> 16),
			static_cast<UINT8>(value >> 24)
		};
		return AppendBytes(destination, bytes, sizeof(bytes));
	}

	bool AppendIndexedHeader(std::vector<UINT8>& destination,
		UINT32 originalSize, UINT32 storedSize, UINT32 flags,
		UINT16 width, UINT16 height, UINT16 numberOfSubImages,
		UINT32 appDataSize)
	{
		const std::size_t start = destination.size();
		const UINT8 depths[] = { 8, 8, 8 };
		return AppendBytes(destination, STCI_ID_STRING, STCI_ID_LEN) &&
			AppendLittleEndian32(destination, originalSize) &&
			AppendLittleEndian32(destination, storedSize) &&
			AppendLittleEndian32(destination, 0) &&
			AppendLittleEndian32(destination, flags) &&
			AppendLittleEndian16(destination, height) &&
			AppendLittleEndian16(destination, width) &&
			AppendLittleEndian32(destination, 256) &&
			AppendLittleEndian16(destination, numberOfSubImages) &&
			AppendBytes(destination, depths, sizeof(depths)) &&
			AppendZeros(destination, 11) &&
			AppendBytes(destination, depths, 1) &&
			AppendZeros(destination, 3) &&
			AppendLittleEndian32(destination, appDataSize) &&
			AppendZeros(destination, 12) &&
			destination.size() - start == STCI_HEADER_SIZE;
	}

	bool AppendSubImages(std::vector<UINT8>& destination,
		const STCISubImage* subImages, UINT16 count)
	{
		if (count && !subImages) return false;
		for (UINT16 index = 0; index < count; ++index)
		{
			const STCISubImage& subImage = subImages[index];
			if (!AppendLittleEndian32(destination, subImage.uiDataOffset) ||
				!AppendLittleEndian32(destination, subImage.uiDataLength) ||
				!AppendLittleEndian16(destination,
					static_cast<UINT16>(subImage.sOffsetX)) ||
				!AppendLittleEndian16(destination,
					static_cast<UINT16>(subImage.sOffsetY)) ||
				!AppendLittleEndian16(destination, subImage.usHeight) ||
				!AppendLittleEndian16(destination, subImage.usWidth))
			{
				return false;
			}
		}
		return true;
	}
}

BOOLEAN TryWriteSTIFile( const INT8 *pData, const SGPPaletteEntry *pPalette,
	INT16 sWidth, INT16 sHeight, STR cOutputName, UINT32 fFlags,
	UINT32 uiAppDataSize )
{
	std::size_t originalSize = 0;
	if (!pData || !pPalette || !cOutputName || !*cOutputName ||
		!UtilsImageUtilityModel::CheckedImageByteCount(
			sWidth, sHeight, 1, originalSize))
	{
		return FALSE;
	}

	UINT8* outputBuffer = NULL;
	UINT32 compressedSize = 0;
	UINT8* rawSubImageBuffer = NULL;
	UINT16 numberOfSubImages = 0;
	if (fFlags & CONVERT_ETRLE_COMPRESS)
	{
		if (!ConvertToETRLE(&outputBuffer, &compressedSize,
			&rawSubImageBuffer, &numberOfSubImages,
			reinterpret_cast<const UINT8*>(pData),
			static_cast<UINT16>(sWidth), static_cast<UINT16>(sHeight), fFlags))
		{
			return FALSE;
		}
	}
	MemOwner<UINT8> outputOwner(outputBuffer);
	MemOwner<UINT8> subImageOwner(rawSubImageBuffer);
	const STCISubImage* const subImageBuffer =
		reinterpret_cast<const STCISubImage*>(rawSubImageBuffer);

	const UINT32 storedSize = (fFlags & CONVERT_ETRLE_COMPRESS)
		? compressedSize : static_cast<UINT32>(originalSize);
	UINT32 headerFlags = STCI_INDEXED;
	if (fFlags & CONVERT_ETRLE_COMPRESS)
		headerFlags |= STCI_ETRLE_COMPRESSED;

	std::size_t subImageBytes = 0;
	if (!UtilsImageUtilityModel::CheckedProduct(numberOfSubImages,
		static_cast<std::size_t>(STCI_SUBIMAGE_SIZE), subImageBytes))
	{
		return FALSE;
	}

	try
	{
		std::vector<UINT8> staged;
		const std::size_t paletteBytes = 256 * STCI_PALETTE_ELEMENT_SIZE;
		std::size_t reserveSize = STCI_HEADER_SIZE;
		for (const std::size_t addition : {
			paletteBytes, subImageBytes,
			static_cast<std::size_t>(storedSize),
			static_cast<std::size_t>(uiAppDataSize)})
		{
			if (!UtilsImageUtilityModel::CanAppendSerializedBytes(
				reserveSize, addition)) return FALSE;
			reserveSize += addition;
		}
		staged.reserve(reserveSize);
		if (!AppendIndexedHeader(staged, static_cast<UINT32>(originalSize),
			storedSize, headerFlags, static_cast<UINT16>(sWidth),
			static_cast<UINT16>(sHeight), numberOfSubImages, uiAppDataSize))
		{
			return FALSE;
		}
		for (UINT32 index = 0; index < 256; ++index)
		{
			const UINT8 entry[] = {
				pPalette[index].peRed,
				pPalette[index].peGreen,
				pPalette[index].peBlue
			};
			if (!AppendBytes(staged, entry, sizeof(entry)))
				return FALSE;
		}
		if (!AppendSubImages(staged, subImageBuffer, numberOfSubImages))
			return FALSE;
		const void* const imageData = (fFlags & CONVERT_ETRLE_COMPRESS)
			? static_cast<const void*>(outputBuffer)
			: static_cast<const void*>(pData);
		if (!AppendBytes(staged, imageData, storedSize) ||
			!AppendZeros(staged, uiAppDataSize))
		{
			return FALSE;
		}

		vfs::CVirtualFileSystem* const fileSystem = getVFS();
		return fileSystem && fileSystem->replaceFileAtomically(
			vfs::Path(cOutputName),
			reinterpret_cast<const vfs::Byte*>(staged.data()), staged.size());
	}
	catch (...)
	{
		return FALSE;
	}
}

void WriteSTIFile( INT8 *pData, SGPPaletteEntry *pPalette, INT16 sWidth,
	INT16 sHeight, STR cOutputName, UINT32 fFlags, UINT32 uiAppDataSize )
{
	TryWriteSTIFile(pData, pPalette, sWidth, sHeight, cOutputName,
		fFlags, uiAppDataSize);
}



#define COMPRESS_TRANSPARENT				0x80
#define COMPRESS_NON_TRANSPARENT		0x00
#define COMPRESS_RUN_LIMIT					0x7F

#define TCI		0x00
#define WI		0xFF

UINT32 ETRLECompressSubImage( UINT8 * pDest, UINT32 uiDestLen, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage );
UINT32 ETRLECompress( UINT8 * pDest, UINT32 uiDestLen, const UINT8 * pSource, UINT32 uiSourceLen );
BOOLEAN DetermineOffset( UINT32 * puiOffset, UINT16 usWidth, UINT16 usHeight, INT16 sX, INT16 sY );
BOOLEAN GoPastWall( INT16 * psNewX, INT16 * psNewY, UINT16 usWidth, UINT16 usHeight, const UINT8 * pCurrent, INT16 sCurrX, INT16 sCurrY );
BOOLEAN GoToNextSubImage( INT16 * psNewX, INT16 * psNewY, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, INT16 sOrigX, INT16 sOrigY );
BOOLEAN DetermineSubImageSize( const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage );
BOOLEAN DetermineSubImageUsedSize( const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage );
BOOLEAN CheckForDataInRows( INT16 * psXValue, INT16 sXIncrement, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage );
BOOLEAN CheckForDataInCols( INT16 * psXValue, INT16 sXIncrement, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage );
const UINT8 * CheckForDataInRowOrColumn( const UINT8 * pPixel, UINT16 usIncrement, UINT16 usNumberOfPixels );

BOOLEAN ConvertToETRLE( UINT8 ** ppDest, UINT32 * puiDestLen, UINT8 ** ppSubImageBuffer, UINT16 * pusNumberOfSubImages, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, UINT32 fFlags )
{
	INT16						sCurrX;
	INT16						sCurrY;
	INT16						sNextX;
	INT16						sNextY;
	UINT8 *					pOutputNext;
	UINT8 *					pTemp;
	BOOLEAN					fContinue = TRUE;
	BOOLEAN					fOk = TRUE;
	BOOLEAN					fStore;
	BOOLEAN					fNextExists;
	STCISubImage *	pCurrSubImage;
	STCISubImage		TempSubImage;
	UINT32					uiSubImageCompressedSize;
	UINT32					uiSpaceLeft;

	if (!ppDest || !puiDestLen || !ppSubImageBuffer ||
		!pusNumberOfSubImages)
	{
		return FALSE;
	}
	*ppDest = NULL;
	*puiDestLen = 0;
	*ppSubImageBuffer = NULL;
	*pusNumberOfSubImages = 0;
	if (!p8BPPBuffer ||
		usWidth > static_cast<UINT16>((std::numeric_limits<INT16>::max)()) ||
		usHeight > static_cast<UINT16>((std::numeric_limits<INT16>::max)()))
	{
		return FALSE;
	}

	// Worst-case ETRLE output is source bytes, one run byte per source
	// byte, and one scanline terminator per source byte.
	if (!UtilsImageUtilityModel::CheckedEtrleCapacity(
		usWidth, usHeight, uiSpaceLeft)) return FALSE;
	*ppDest = (UINT8 *) MemAlloc( uiSpaceLeft );
	if (!*ppDest) return FALSE;
	*puiDestLen = uiSpaceLeft;

	pOutputNext = *ppDest;

	if (fFlags & CONVERT_ETRLE_COMPRESS_SINGLE)
	{
		// there are no walls in this image, but we treat it as a "subimage" for
		// the purposes of calling the compressor

		// we want a 1-element SubImage array for this...
		// allocate!
		*ppSubImageBuffer = (UINT8 *) MemAlloc( STCI_SUBIMAGE_SIZE );
		if (!(*ppSubImageBuffer))
		{
			MemFree( *ppDest );
			*ppDest = NULL;
			*puiDestLen = 0;
			return( FALSE );
		}
		*pusNumberOfSubImages = 1;
		pCurrSubImage = (STCISubImage *) *ppSubImageBuffer;
		memset(pCurrSubImage, 0, STCI_SUBIMAGE_SIZE);
		pCurrSubImage->sOffsetX = 0;
		pCurrSubImage->sOffsetY = 0;
		pCurrSubImage->usWidth = usWidth;
		pCurrSubImage->usHeight = usHeight;
		if (!(fFlags & CONVERT_ETRLE_NO_SUBIMAGE_SHRINKING))
		{
			if (!(DetermineSubImageUsedSize( p8BPPBuffer, usWidth, usHeight, pCurrSubImage )))
			{
				MemFree( *ppDest );
				MemFree( *ppSubImageBuffer );
				*ppDest = NULL;
				*ppSubImageBuffer = NULL;
				*puiDestLen = 0;
				*pusNumberOfSubImages = 0;
				return( FALSE );
			}
		}
		uiSubImageCompressedSize = ETRLECompressSubImage( pOutputNext, uiSpaceLeft, p8BPPBuffer, usWidth, usHeight, pCurrSubImage );
		if (uiSubImageCompressedSize == 0)
		{
			MemFree( *ppDest );
			MemFree( *ppSubImageBuffer );
			*ppDest = NULL;
			*ppSubImageBuffer = NULL;
			*puiDestLen = 0;
			*pusNumberOfSubImages = 0;
			return( FALSE );
		}
		else
		{
			pCurrSubImage->uiDataOffset = 0;
			pCurrSubImage->uiDataLength = uiSubImageCompressedSize;
			*puiDestLen = uiSubImageCompressedSize;
			return( TRUE );
		}
	}
	else
	{
		// skip any initial wall bytes to find the first subimage
		if (!GoPastWall( &sCurrX, &sCurrY, usWidth, usHeight, p8BPPBuffer, 0, 0 ))
		{ // no subimages!
			MemFree( *ppDest );
			*ppDest = NULL;
			*puiDestLen = 0;
			return( FALSE );
		}
		while (fContinue)
		{
			if (*pusNumberOfSubImages ==
				(std::numeric_limits<UINT16>::max)())
			{
				fOk = FALSE;
				break;
			}
			// allocate more memory for SubImage structures, and set the current pointer to the last one
			std::size_t nextSubImageBytes = 0;
			if (!UtilsImageUtilityModel::CheckedProduct(
				static_cast<std::size_t>(*pusNumberOfSubImages) + 1,
				static_cast<std::size_t>(STCI_SUBIMAGE_SIZE),
				nextSubImageBytes))
			{
				fOk = FALSE;
				break;
			}
			pTemp = (UINT8 *) MemRealloc( *ppSubImageBuffer, nextSubImageBytes );
			if (pTemp == NULL)
			{
				fOk = FALSE;
				break;
			}
			else
			{
				*ppSubImageBuffer = pTemp;
			}
			pCurrSubImage = (STCISubImage *) (*ppSubImageBuffer + (*pusNumberOfSubImages) * STCI_SUBIMAGE_SIZE);
			memset(pCurrSubImage, 0, STCI_SUBIMAGE_SIZE);

			pCurrSubImage->sOffsetX = sCurrX;
			pCurrSubImage->sOffsetY = sCurrY;
			// determine the subimage's full size
			if (!DetermineSubImageSize( p8BPPBuffer, usWidth, usHeight, pCurrSubImage ))
			{
				fOk = FALSE;
				break;
			}
			if (*pusNumberOfSubImages == 0 && pCurrSubImage->usWidth == usWidth && pCurrSubImage->usHeight == usHeight)
			{
				printf( "\tWarning: no walls (subimage delimiters) found.\n" );
			}

			memcpy( &TempSubImage, pCurrSubImage, STCI_SUBIMAGE_SIZE );
			if (DetermineSubImageUsedSize( p8BPPBuffer, usWidth, usHeight, &TempSubImage))
			{
				// image has nontransparent data; we definitely want to store it
				fStore = TRUE;
				if (!(fFlags & CONVERT_ETRLE_NO_SUBIMAGE_SHRINKING))
				{
					memcpy( pCurrSubImage, &TempSubImage, STCI_SUBIMAGE_SIZE );
				}
			}
			else if (fFlags & CONVERT_ETRLE_DONT_SKIP_BLANKS)
			{
				// image is transparent; we will store it if there is another subimage
				// to the right of it on the same line
				// find the next subimage
				fNextExists = GoToNextSubImage( &sNextX, &sNextY, p8BPPBuffer, usWidth, usHeight, sCurrX, sCurrY );
				if (fNextExists && sNextY == sCurrY )
				{
					fStore = TRUE;
				}
				else
				{
					// junk transparent section at the end of the line!
					fStore = FALSE;
				}
			}
			else
			{
				// transparent data; discarding
				fStore = FALSE;
			}

			if (fStore)
			{
				// we want to store this subimage!
				uiSubImageCompressedSize = ETRLECompressSubImage( pOutputNext, uiSpaceLeft, p8BPPBuffer, usWidth, usHeight, pCurrSubImage );
				if (uiSubImageCompressedSize == 0)
				{
					fOk = FALSE;
					break;
				}
				pCurrSubImage->uiDataOffset = (*puiDestLen - uiSpaceLeft);
				pCurrSubImage->uiDataLength = uiSubImageCompressedSize;
				// this is a cheap hack; the sOffsetX and sOffsetY values have been used
				// to store the location of the subimage within the whole image.	Now
				// we want the offset within the subimage, so, we subtract the coordatines
				// for the upper-left corner of the subimage.
				pCurrSubImage->sOffsetX -= sCurrX;
				pCurrSubImage->sOffsetY -= sCurrY;
				(*pusNumberOfSubImages)++;
				pOutputNext += uiSubImageCompressedSize;
				uiSpaceLeft -= uiSubImageCompressedSize;
			}
			// find the next subimage
			fContinue = GoToNextSubImage( &sCurrX, &sCurrY, p8BPPBuffer, usWidth, usHeight, sCurrX, sCurrY );
		}
	}
	if (fOk)
	{
		*puiDestLen -= uiSpaceLeft;
		return( TRUE );
	}
	else
	{
		MemFree( *ppDest );
		*ppDest = NULL;
		*puiDestLen = 0;
		if (*ppSubImageBuffer != NULL)
		{
			MemFree( *ppSubImageBuffer );
			*ppSubImageBuffer = NULL;
		}
		*pusNumberOfSubImages = 0;
		return( FALSE );
	}
}

UINT32 ETRLECompressSubImage( UINT8 * pDest, UINT32 uiDestLen, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage )
{
	UINT16		usLoop;
	UINT32		uiScanLineCompressedSize;
	UINT32		uiSpaceLeft = uiDestLen;
	UINT32		uiOffset;
	const UINT8 *		pCurrent;

	if (!pDest || !p8BPPBuffer || !pSubImage || uiDestLen == 0 ||
		!UtilsImageUtilityModel::ContainsRectangle(usWidth, usHeight,
			pSubImage->sOffsetX, pSubImage->sOffsetY,
			pSubImage->usWidth, pSubImage->usHeight) ||
		!DetermineOffset(&uiOffset, usWidth, usHeight,
			pSubImage->sOffsetX, pSubImage->sOffsetY))
	{
		return 0;
	}
	pCurrent = p8BPPBuffer + uiOffset;

	for (usLoop = 0; usLoop < pSubImage->usHeight; usLoop++)
	{
		uiScanLineCompressedSize = ETRLECompress( pDest, uiSpaceLeft, pCurrent, pSubImage->usWidth );
		if (uiScanLineCompressedSize == 0 )
		{ // there wasn't enough room to complete the compression!
			return( 0 );
		}
		// reduce the amount of available space
		uiSpaceLeft -= uiScanLineCompressedSize;
		pDest += uiScanLineCompressedSize;
		// go to the next scanline
		pCurrent += usWidth;
	}
	return( uiDestLen - uiSpaceLeft );
}

UINT32 ETRLECompress( UINT8 * pDest, UINT32 uiDestLen, const UINT8 * pSource, UINT32 uiSourceLen )
{
	// Compress a buffer (a scanline) into ETRLE format, which is a series of runs.
	// Each run starts with a byte whose high bit is 1 if the run is compressed, 0 otherwise.
	// The lower seven bits of that byte indicate the length of the run

	// ETRLECompress returns the number of bytes used by the compressed buffer, or 0 if an error
	// occurred

	// uiSourceLoc keeps track of our current position in the
	// source
	UINT32	uiSourceLoc = 0;
	// uiCurrentSourceLoc is used to look ahead in the source to
	// determine the length of runs
	UINT32	uiCurrentSourceLoc = 0;
	UINT32	uiDestLoc = 0;
	UINT8		ubLength = 0;


	if (!pDest || !pSource || uiDestLen == 0 || uiSourceLen == 0) return 0;

	while (uiSourceLoc < uiSourceLen && uiDestLoc < uiDestLen)
	{
		if (pSource[uiSourceLoc] == TCI)
		{ // transparent run - determine its length
			do
			{
				uiCurrentSourceLoc++;
				ubLength++;
			}
			while ((uiCurrentSourceLoc < uiSourceLen) && pSource[uiCurrentSourceLoc] == TCI && (ubLength < COMPRESS_RUN_LIMIT));
			// output run-byte
			pDest[uiDestLoc] = ubLength | COMPRESS_TRANSPARENT;

			// update location
			uiSourceLoc += ubLength;
			uiDestLoc += 1;
		}
		else
		{	// non-transparent run - determine its length
			do
			{
				uiCurrentSourceLoc++;
				ubLength++;
			}
			while ((uiCurrentSourceLoc < uiSourceLen) && (pSource[uiCurrentSourceLoc] != TCI) && (ubLength < COMPRESS_RUN_LIMIT));
			if (ubLength < uiDestLen - uiDestLoc)
			{
				// output run-byte
				pDest[uiDestLoc++] = ubLength | COMPRESS_NON_TRANSPARENT;

				// output run (and update location)
				memcpy( pDest + uiDestLoc, pSource + uiSourceLoc, ubLength );
				uiSourceLoc += ubLength;
				uiDestLoc += ubLength;
			}
			else
			{ // not enough room in dest buffer to copy the run!
				return( 0 );
			}
		}
		uiCurrentSourceLoc = uiSourceLoc;
		ubLength = 0;
	}
	if (uiDestLoc >= uiDestLen)
	{
		return( 0 );
	}
	else
	{
		// end with a run of 0 length (which might as well be non-transparent,
		// giving a 0-byte
		pDest[uiDestLoc++] = 0;
		return( uiDestLoc );
	}
}

BOOLEAN DetermineOffset( UINT32 * puiOffset, UINT16 usWidth, UINT16 usHeight, INT16 sX, INT16 sY )
{
	if (!puiOffset) return FALSE;
	UINT32 staged = 0;
	if (!UtilsImageUtilityModel::PixelOffset(
		usWidth, usHeight, sX, sY, staged)) return FALSE;
	*puiOffset = staged;
	return TRUE;
}

BOOLEAN GoPastWall( INT16 * psNewX, INT16 * psNewY, UINT16 usWidth, UINT16 usHeight, const UINT8 * pCurrent, INT16 sCurrX, INT16 sCurrY )
{
	if (!psNewX || !psNewY || !pCurrent ||
		!UtilsImageUtilityModel::ContainsRectangle(
			usWidth, usHeight, sCurrX, sCurrY, 1, 1))
	{
		return FALSE;
	}
	// If the current pixel is a wall, we assume that it is on a horizontal wall and
	// search right, wrapping around the end of scanlines, until we find non-wall data.
	while (*pCurrent == WI)
	{
		if (++sCurrX == usWidth)
		{ // wrap our logical coordinates!
			sCurrX = 0;
			if (++sCurrY == usHeight)
			{
				// no more images!
				return( FALSE );
			}
		}
		++pCurrent;
	}

	*psNewX = sCurrX;
	*psNewY = sCurrY;
	return( TRUE );
}

BOOLEAN GoToNextSubImage( INT16 * psNewX, INT16 * psNewY, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, INT16 sOrigX, INT16 sOrigY )
{
	// return the coordinates of the next subimage in the image
	// (either to the right, or the first of the next row down
	INT16				sCurrX = sOrigX;
	INT16				sCurrY = sOrigY;
	UINT32			uiOffset;
	const UINT8 *			pCurrent;
	if (!psNewX || !psNewY || !p8BPPBuffer ||
		!DetermineOffset(&uiOffset, usWidth, usHeight, sCurrX, sCurrY))
	{
		return FALSE;
	}
	pCurrent = p8BPPBuffer + uiOffset;

	if (*pCurrent == WI)
	{
		return( GoPastWall( psNewX, psNewY, usWidth, usHeight, pCurrent, sCurrX, sCurrY ) );
	}
	else
	{
		// The current pixel is not a wall.	We scan right past all non-wall data to skip to
		// the right-hand end of the subimage, then right past all wall data to skip a vertical
		// wall, and should find ourselves at another subimage.

		// If we hit the right edge of the image, we back up to our start point, go DOWN to
		// the bottom of the image to the horizontal wall, and then recurse to go along it
		// to the right place on the next scanline

		while (sCurrX < usWidth && *pCurrent != WI)
		{
			++sCurrX;
			++pCurrent;
		}
		if (sCurrX < usWidth)
		{
			// skip all wall data to the right, starting at the new current position
			while (sCurrX < usWidth && *pCurrent == WI)
			{
				++sCurrX;
				++pCurrent;
			}
		}
		if (sCurrX < usWidth)
		{
			*psNewX = sCurrX;
			*psNewY = sCurrY;
			return( TRUE );
		}
		else
		{
			// go back to the beginning of the subimage and scan down
			sCurrX = sOrigX;
			sCurrY = sOrigY;
			pCurrent = p8BPPBuffer + uiOffset;

			// skip all non-wall data below, starting at the current position
			while (sCurrY < usHeight && *pCurrent != WI)
			{
				++sCurrY;
				if (sCurrY == usHeight) return FALSE;
				pCurrent += usWidth;
			}
			// We are now at the horizontal wall at the bottom of the current image
			return( GoPastWall( psNewX, psNewY, usWidth, usHeight, pCurrent, sCurrX, sCurrY ) );
		}
	}
}

BOOLEAN DetermineSubImageSize( const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage )
{
	UINT32		uiOffset;
	const UINT8 *		pCurrent;
	if (!p8BPPBuffer || !pSubImage) return FALSE;
	INT16 sCurrX = pSubImage->sOffsetX;
	INT16 sCurrY = pSubImage->sOffsetY;
	if (
		!DetermineOffset( &uiOffset, usWidth, usHeight, sCurrX, sCurrY ))
	{
			return( FALSE );
	}

	// determine width
	pCurrent = p8BPPBuffer + uiOffset;
	while (sCurrX < usWidth && *pCurrent != WI)
	{
		++sCurrX;
		++pCurrent;
	}
	pSubImage->usWidth = sCurrX - pSubImage->sOffsetX;

	// determine height
	sCurrY = pSubImage->sOffsetY;
	pCurrent = p8BPPBuffer + uiOffset;
	while (sCurrY < usHeight && *pCurrent != WI)
	{
		++sCurrY;
		if (sCurrY == usHeight) break;
		pCurrent += usWidth;
	}
	pSubImage->usHeight = sCurrY - pSubImage->sOffsetY;

	return pSubImage->usWidth > 0 && pSubImage->usHeight > 0;
}

BOOLEAN DetermineSubImageUsedSize( const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage )
{
	if (!p8BPPBuffer || !pSubImage ||
		!UtilsImageUtilityModel::ContainsRectangle(usWidth, usHeight,
			pSubImage->sOffsetX, pSubImage->sOffsetY,
			pSubImage->usWidth, pSubImage->usHeight))
	{
		return FALSE;
	}
	INT16		sNewValue;
	// to do our search loops properly, we can't change the height and width of the
	// subimages until we're done all of our shrinks
	UINT16	usNewHeight;
	UINT16	usNewWidth;
	UINT16	usNewX;
	UINT16	usNewY;

	// shrink from the top
	if (CheckForDataInRows( &sNewValue, 1, p8BPPBuffer, usWidth, usHeight, pSubImage ))
	{
		usNewY = sNewValue;
	}
	else
	{
		return( FALSE );
	}
	// shrink from the bottom
	if (CheckForDataInRows( &sNewValue, -1, p8BPPBuffer, usWidth, usHeight, pSubImage ))
	{
		usNewHeight = (UINT16) sNewValue - usNewY + 1;
	}
	else
	{
		return( FALSE );
	}
	// shrink from the left
	if (CheckForDataInCols( &sNewValue, 1, p8BPPBuffer, usWidth, usHeight, pSubImage ))
	{
		usNewX = sNewValue;
	}
	else
	{
		return( FALSE );
	}
	// shrink from the right
	if (CheckForDataInCols( &sNewValue, -1, p8BPPBuffer, usWidth, usHeight, pSubImage ))
	{
		usNewWidth = (UINT16) sNewValue - usNewX + 1;
	}
	else
	{
		return( FALSE );
	}
	pSubImage->sOffsetX = usNewX;
	pSubImage->sOffsetY = usNewY;
	pSubImage->usHeight = usNewHeight;
	pSubImage->usWidth = usNewWidth;
	return( TRUE );
}

BOOLEAN CheckForDataInRows( INT16 * psYValue, INT16 sYIncrement, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage )
{
	INT16		sCurrY;
	UINT32	uiOffset;
	const UINT8 *	pCurrent;
	UINT16	usLoop;

	if (!psYValue || !p8BPPBuffer || !pSubImage ||
		!UtilsImageUtilityModel::ContainsRectangle(usWidth, usHeight,
			pSubImage->sOffsetX, pSubImage->sOffsetY,
			pSubImage->usWidth, pSubImage->usHeight))
	{
		return FALSE;
	}
	if (sYIncrement == 1)
	{
		sCurrY = pSubImage->sOffsetY;
	}
	else if (sYIncrement == -1)
	{
		sCurrY = pSubImage->sOffsetY + (INT16) pSubImage->usHeight - 1;
	}
	else
	{
		// invalid value!
		return( FALSE );
	}
	for (usLoop = 0; usLoop < pSubImage->usHeight; usLoop++)
	{
		if (!DetermineOffset( &uiOffset, usWidth, usHeight, pSubImage->sOffsetX, (INT16) sCurrY))
		{
			return( FALSE );
		}
		pCurrent = p8BPPBuffer + uiOffset;
		pCurrent = CheckForDataInRowOrColumn( pCurrent, 1, pSubImage->usWidth );
		if (pCurrent)
		{
			// non-null data found!
			*psYValue = sCurrY;
			return( TRUE );
		}
		sCurrY += sYIncrement;
	}
	return( FALSE );
}

BOOLEAN CheckForDataInCols( INT16 * psXValue, INT16 sXIncrement, const UINT8 * p8BPPBuffer, UINT16 usWidth, UINT16 usHeight, STCISubImage * pSubImage )
{
	INT16		sCurrX;
	UINT32	uiOffset;
	const UINT8 *	pCurrent;
	UINT16	usLoop;

	if (!psXValue || !p8BPPBuffer || !pSubImage ||
		!UtilsImageUtilityModel::ContainsRectangle(usWidth, usHeight,
			pSubImage->sOffsetX, pSubImage->sOffsetY,
			pSubImage->usWidth, pSubImage->usHeight))
	{
		return FALSE;
	}
	if (sXIncrement == 1)
	{
		sCurrX = pSubImage->sOffsetX;
	}
	else if (sXIncrement == -1)
	{
		sCurrX = pSubImage->sOffsetX + (INT16) pSubImage->usWidth - 1;
	}
	else
	{
		// invalid value!
		return( FALSE );
	}
	for (usLoop = 0; usLoop < pSubImage->usWidth; usLoop++)
	{
		if (!DetermineOffset( &uiOffset, usWidth, usHeight, (UINT16) sCurrX, pSubImage->sOffsetY))
		{
			return( FALSE );
		}
		pCurrent = p8BPPBuffer + uiOffset;
		pCurrent = CheckForDataInRowOrColumn( pCurrent, usWidth, pSubImage->usHeight );
		if (pCurrent)
		{
			// non-null data found!
			*psXValue = sCurrX;
			return( TRUE );
		}
		sCurrX += sXIncrement;
	}
	return( FALSE );
}

const UINT8 * CheckForDataInRowOrColumn( const UINT8 * pPixel, UINT16 usIncrement, UINT16 usNumberOfPixels )
{
	// This function, passed the right increment value, can scan either across or
	// down an image to find a non-transparent pixel

	if (!pPixel || usIncrement == 0 || usNumberOfPixels == 0) return NULL;
	UINT16	usLoop;

	for (usLoop = 0; usLoop < usNumberOfPixels; usLoop++)
	{
		if (*pPixel != TCI)
		{
			return( pPixel );
		}
		else
		{
			pPixel += usIncrement;
		}
	}
	return( NULL );
}
