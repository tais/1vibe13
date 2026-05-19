#ifndef __TYPES_
#define __TYPES_

#ifndef _SIRTECH_TYPES_
#define _SIRTECH_TYPES_

	#ifdef	RELEASE_WITH_DEBUG_INFO

		//For JA2 Release with debug info build, disable these warnigs messages
		#pragma warning( disable : 4201 4214 4057 4100 4514 4115 4711 4244 )

	#endif




// build defines header....
#include "builddefines.h"
	



#include <cstdint>			// fixed-width integer types
#include <wchar.h>			// for wide-character strings
#include "msvc_compat.h"	// portable shims for MSVC-only names

// *** SIR-TECH TYPE DEFINITIONS ***

typedef uint32_t	UINT32;
typedef int64_t		INT64;		// WANNE - BMP: Used for Big Maps
typedef int32_t		INT32;
typedef uint64_t	UINT64;

// integers
typedef uint8_t		UINT8;
typedef int8_t		INT8;
typedef uint16_t	UINT16;
typedef int16_t		INT16;
// floats
typedef float			FLOAT;
typedef double			DOUBLE;
// strings
typedef char			CHAR8;
typedef wchar_t			CHAR16;
// STR/STR8/STR16 are read-only string pointer types -- they exist to
// accept string literals at function boundaries. Use plain CHAR8 * /
// CHAR16 * (or a typed buffer like CHAR8 buf[N]) when you actually need
// to write through the pointer.
typedef const CHAR8 *	STR;
typedef const CHAR8 *	STR8;
typedef const CHAR16 *	STR16;
// flags (individual bits used)
typedef unsigned char	FLAGS8;
typedef unsigned short	FLAGS16;
typedef unsigned long	FLAGS32;
typedef UINT64			FLAGS64;
// other
typedef unsigned char	BOOLEAN;
typedef void *			PTR;
typedef unsigned short	HNDL;
typedef UINT8			BYTE;
typedef CHAR8			STRING512[512];
// HWFILE has historically been UINT32 -- that worked when the VFS
// backend in FileMan.cpp packed a 32-bit handle, but the SGP layer now
// stores a vfs::IBaseFile* in it, which is 64 bits on every modern
// target. Widen to uintptr_t to stop the cast from truncating real
// pointers and segfaulting in FileClose. The legacy bit-packed
// LibraryDataBase usage (lower 22 bits = file id, rest = library id)
// continues to work since uintptr_t is also an integer type.
typedef uintptr_t		HWFILE;

#define SGPFILENAME_LEN 100
typedef CHAR8 SGPFILENAME[SGPFILENAME_LEN];	

// *** SIR-TECH TYPE DEFINITIONS ***

#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define BAD_INDEX -1

#define NULL_HANDLE 65535

#define PI 3.1415926

#define ST_EPSILON 0.00001	// define a sir-tech epsilon value

#ifndef NULL
#define NULL 0
#endif

typedef struct
{
	INT32 x;
	INT32 y;
	INT32 width;
	INT32 height;

} SGPRectangle;

#ifndef _WIN32
// Portable equivalent of Win32 POINT struct for non-Windows builds.
typedef struct
{
	INT32 x;
	INT32 y;
} POINT;
#endif

typedef struct
{ 
	INT32 iLeft;
	INT32 iTop;
	INT32 iRight;
	INT32 iBottom;

} SGPRect;

typedef struct
{
	INT32 	iX;
	INT32	iY;

} SGPPoint;

typedef struct
{
	INT32 Min;
	INT32 Max;

} SGPRange;


typedef FLOAT	 VECTOR2[2];		// 2d vector (2x1 matrix)
typedef FLOAT	 VECTOR3[3];		// 3d vector (3x1 matrix)
typedef FLOAT	 VECTOR4[4];		// 4d vector (4x1 matrix)

typedef INT32		IVECTOR2[2];		// 2d vector (2x1 matrix)
typedef INT32		IVECTOR3[3];		// 3d vector (3x1 matrix)
typedef INT32		IVECTOR4[4];		// 4d vector (4x1 matrix)

typedef VECTOR3	MATRIX3[3];		// 3x3 matrix
typedef VECTOR4	MATRIX4[4];		// 4x4 matrix

//typedef VECTOR3	ANGLE;			// angle return array //lal removed
typedef	VECTOR4	COLOR;			// rgba color array


#include <vfs/Aspects/vfs_settings.h>
#include <vfs/Core/vfs_string.h>

inline void convert_string(std::wstring const& str_in, std::string &str_out)
{
	if(vfs::Settings::getUseUnicode())
	{
		str_out = vfs::String::as_utf8(str_in);
	}
	else
	{
		vfs::String::narrow(str_in, str_out);
	}
}

inline void convert_string(std::string const& str_in, std::wstring &str_out)
{
	if(vfs::Settings::getUseUnicode())
	{
		vfs::String::as_utf16(str_in, str_out);
	}
	else
	{
		vfs::String::widen(str_in, str_out);
	}
}


#endif
