#include "types.h"
#include "sgp.h"
#include "vobject.h"
#include "FileMan.h"
#include "Utilities.h"
#include "Font Control.h"
#include "Overhead.h"
#include "Overhead Types.h"
#include "Soldier Palette.h"
#include "WCheck.h"
#include "Sys Globals.h"
#include "DataBoundaryModel.h"
#include "LegacyUtilitiesModel.h"

#include <Engine/Core/UniqueResourceHandle.h>

#include <array>
#include <cstdint>
#include <cwchar>
#include <limits>
#include <string>


namespace
{
	struct LegacyFileTag {};
	struct LegacyFileReleaser
	{
		void operator()(HWFILE file) const { FileClose(file); }
	};
	using LegacyFileOwner = UniqueResourceHandle<
		LegacyFileTag, LegacyFileReleaser, HWFILE, static_cast<HWFILE>(0)>;
}

BOOLEAN FilenameForBPP(
	STR pFilename, CHAR8* pDestination, std::size_t destinationCapacity)
{
	if (!pDestination || destinationCapacity == 0) return FALSE;
	if (!pFilename)
	{
		pDestination[0] = '\0';
		return FALSE;
	}
	return UtilsDataBoundaryModel::CopyString(
		pDestination, destinationCapacity, pFilename) ? TRUE : FALSE;
}

BOOLEAN CreateSGPPaletteFromCOLFile( SGPPaletteEntry *pPalette, SGPFILENAME ColFile )
{
	if (!pPalette || !ColFile || !ColFile[0]) return FALSE;

	LegacyFileOwner file(FileOpen(ColFile, FILE_ACCESS_READ, FALSE));
	if (!file)
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Cannot open COL file");
		return FALSE;
	}

	constexpr std::size_t HeaderSize = 8;
	constexpr std::size_t ComponentCount = 3;
	std::array<BYTE, HeaderSize + 256 * ComponentCount> bytes{};
	UINT32 bytesRead = 0;
	if (!FileRead(file.get(), bytes.data(), static_cast<UINT32>(bytes.size()),
		&bytesRead) || bytesRead != bytes.size())
	{
		return FALSE;
	}

	struct PaletteRgb
	{
		UINT8 red;
		UINT8 green;
		UINT8 blue;
	};
	std::array<PaletteRgb, 256> staged{};
	for (std::size_t index = 0; index < staged.size(); ++index)
	{
		const std::size_t source = HeaderSize + index * ComponentCount;
		staged[index] = {
			bytes[source], bytes[source + 1], bytes[source + 2]};
	}
	for (std::size_t index = 0; index < staged.size(); ++index)
	{
		pPalette[index].peRed = staged[index].red;
		pPalette[index].peGreen = staged[index].green;
		pPalette[index].peBlue = staged[index].blue;
	}

	return TRUE;
}

BOOLEAN DisplayPaletteRep( PaletteRepID aPalRep, UINT8 ubXPos, UINT8 ubYPos, UINT32 uiDestSurface )
{
	PIXEL										us16BPPColor;
	UINT32										cnt1;
	UINT8											ubSize, ubType;
	INT16											sTLX, sTLY, sBRX, sBRY;
	UINT8											ubPaletteRep;

	// Create 16BPP Palette
	CHECKF( GetPaletteRepIndexFromID( aPalRep, &ubPaletteRep ) );

	SetFont( LARGEFONT1 );

	ubType = gpPalRep[ ubPaletteRep ].ubType;
	ubSize = gpPalRep[ ubPaletteRep ].ubPaletteSize;

	for ( cnt1 = 0; cnt1 < ubSize; cnt1++ )
	{
		sTLX = ubXPos + (UINT16)( ( cnt1 % 16 ) * 20 );
		sTLY = ubYPos + (UINT16)( ( cnt1 / 16 ) * 20 );
		sBRX = sTLX + 20;
		sBRY = sTLY + 20;

		us16BPPColor = Get16BPPColor( FROMRGB( gpPalRep[ ubPaletteRep ].r[ cnt1 ], gpPalRep[ ubPaletteRep ].g[ cnt1 ], gpPalRep[ ubPaletteRep ].b[ cnt1 ] ) );

		ColorFillVideoSurfaceArea( uiDestSurface, sTLX, sTLY, sBRX, sBRY, us16BPPColor );

	}

	gprintf( ubXPos + ( 16 * 20 ), ubYPos, L"%S", gpPalRep[ ubPaletteRep ].ID );

	return( TRUE );
}


BOOLEAN WrapString(
	CHAR16* pStr, std::size_t strCapacity,
	CHAR16* pStr2, std::size_t str2Capacity,
	UINT16 usWidth, INT32 uiFont)
{
	if (!pStr || strCapacity == 0 || !pStr2 || str2Capacity == 0)
		return FALSE;
	pStr2[0] = L'\0';

	const CHAR16* terminator = static_cast<const CHAR16*>(
		std::wmemchr(pStr, L'\0', strCapacity));
	if (!terminator) return FALSE;
	const std::size_t length = static_cast<std::size_t>(terminator - pStr);
	HVOBJECT font = GetFontObject(uiFont);
	if (!font) return FALSE;

	std::uint64_t width = 0;
	std::size_t overflowIndex = length;
	for (std::size_t index = 0; index < length; ++index)
	{
		const INT16 glyph = GetIndex(pStr[index]);
		const UINT32 glyphWidth = GetWidth(font, glyph);
		width = glyphWidth > std::numeric_limits<std::uint64_t>::max() - width
			? std::numeric_limits<std::uint64_t>::max()
			: width + glyphWidth;
		if (width > usWidth)
		{
			overflowIndex = index;
			break;
		}
	}
	if (overflowIndex == length) return FALSE;

	LegacyUtilitiesModel::WideTextSplit split;
	if (!LegacyUtilitiesModel::SplitWideText(
		std::wstring_view(pStr, length), overflowIndex, split) ||
		split.first.size() >= strCapacity ||
		split.second.size() >= str2Capacity)
	{
		return FALSE;
	}

	std::wmemcpy(pStr, split.first.data(), split.first.size());
	pStr[split.first.size()] = L'\0';
	std::wmemcpy(pStr2, split.second.data(), split.second.size());
	pStr2[split.second.size()] = L'\0';
	return TRUE;
}


// Preserve the established media-file probe order used by game settings.
SGPFILENAME gCheckFilenames[] =
{
	"DATA\\INTRO.SLF",
	"DATA\\LOADSCREENS.SLF",
	"DATA\\MAPS.SLF",
	"DATA\\NPC_SPEECH.SLF",
	"DATA\\SPEECH.SLF",
};

BOOLEAN HandleJA2CDCheck()
{
	// Retail CD and time-limited demo enforcement has been disabled in 1.13 for
	// decades. Keep the active startup ABI while retiring the dead Win9x paths.
	return TRUE;
}

BOOLEAN DoJA2FilesExistsOnDrive( const CHAR8 *zCdLocation )
{
	if (!zCdLocation) return FALSE;
	constexpr std::size_t RequiredFileCount = 4;
	for (std::size_t index = 0; index < RequiredFileCount; ++index)
	{
		std::string path;
		if (!LegacyUtilitiesModel::JoinPath(
			zCdLocation, gCheckFilenames[index], SGPFILENAME_LEN, path))
			return FALSE;
		LegacyFileOwner file(FileOpen(
			const_cast<char*>(path.c_str()),
			FILE_ACCESS_READ | FILE_OPEN_EXISTING, FALSE));
		if (!file) return FALSE;
	}
	return TRUE;
}
