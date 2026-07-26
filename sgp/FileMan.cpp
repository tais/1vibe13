//**************************************************************************
//
// Filename :	FileMan.c
//
//	Purpose :	function definitions for the memory manager
//
// Modification history :
//
//		24sep96:HJH		->creation
//	08Apr97:ARM	->Assign return value from Push() calls back to HStack
//					 handle, because it may possibly do a MemRealloc()
//		29Dec97:Kris Morness 
//									->Added functionality for setting file attributes which
//									allows for read-only attribute overriding
//									->Also added a simple function that clears all file attributes
//										to normal.
//
//		5 Feb 98:Dave French->extensive modification to support libraries
//
//**************************************************************************

//**************************************************************************
//
//				Includes
//
//**************************************************************************
	#include "types.h"
	#include <stdlib.h>
	#include <stdio.h>

	#include "FileMan.h"
	#include "MemMan.h"
	#include "DEBUG.H"
	#include "LibraryDataBase.h"
	#include "sgp_logger.h"

using namespace std;

#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_os_functions.h>
#include <vfs/Aspects/vfs_settings.h>


#include <vfs/Core/vfs_file_raii.h>
#include <vfs/Tools/vfs_parser_tools.h>
#include <memory>
#include <new>

namespace
{
// HWFILE is intentionally opaque outside FileMan. Keep the access mode and the
// already-resolved VFS interface beside the underlying file instead of looking
// them up in a process-global std::map for every scalar save/load operation.
// Write-capable VFS files also implement IReadable, so storing the interface
// selected by FileOpen is what preserves the legacy read/write separation.
struct FileHandle
{
	vfs::IBaseFile* file;
	vfs::tReadableFile* reader;
	vfs::tWritableFile* writer;
};

FileHandle* GetFileHandle(HWFILE handle)
{
	return reinterpret_cast<FileHandle*>(handle);
}

HWFILE MakeReadHandle(vfs::tReadableFile* file)
{
	if(!file)
		return 0;
	FileHandle* handle = new (std::nothrow) FileHandle{file, file, nullptr};
	return reinterpret_cast<HWFILE>(handle);
}

HWFILE MakeWriteHandle(vfs::tWritableFile* file)
{
	if(!file)
		return 0;
	FileHandle* handle = new (std::nothrow) FileHandle{file, nullptr, file};
	return reinterpret_cast<HWFILE>(handle);
}
}

//**************************************************************************
//
//				Defines
//
//**************************************************************************

#define FILENAME_LENGTH					600

#define CHECKF(exp)	if (!(exp)) { return(FALSE); }
#define CHECKV(exp)	if (!(exp)) { return; }
#define CHECKN(exp)	if (!(exp)) { return(NULL); }
#define CHECKBI(exp) if (!(exp)) { return(-1); }

//**************************************************************************
//
//				Typedefs
//
//**************************************************************************

typedef struct FMFileInfoTag
{
	CHAR		strFilename[FILENAME_LENGTH];
	UINT8		uiFileAccess;
	UINT32	uiFilePosition;
	HANDLE	hFileHandle;

} FMFileInfo;	// for 'File Manager File Information'

typedef struct FileSystemTag
{
	FMFileInfo	*pFileInfo;
	UINT32	uiNumHandles;
	BOOLEAN	fDebug;
	BOOLEAN	fDBInitialized;

	CHAR		*pcFileNames;
	UINT32	uiNumFilesInDirectory;
} FileSystem;

//**************************************************************************
//
//				Variables
//
//**************************************************************************


//The FileDatabaseHeader
DatabaseManagerHeaderStruct gFileDataBase;


//FileSystem gfs;

WIN32_FIND_DATA Win32FindInfo[20];
BOOLEAN fFindInfoInUse[20] = {FALSE,FALSE,FALSE,FALSE,FALSE,
															FALSE,FALSE,FALSE,FALSE,FALSE,
															FALSE,FALSE,FALSE,FALSE,FALSE,
															FALSE,FALSE,FALSE,FALSE,FALSE };
HANDLE hFindInfoHandle[20] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE,
															INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };

//**************************************************************************
//
//				Function Prototypes
//
//**************************************************************************

//**************************************************************************
//
//				Functions
//
//**************************************************************************

static bool gFileManagerInitialized = false;

//**************************************************************************
//
// FileSystemInit
//
//		Starts up the file system.
//
// Parameter List :
// Return Value :
// Modification history :
//
//		24sep96:HJH		->creation
//
//**************************************************************************
BOOLEAN	InitializeFileManager(	STR strIndexFilename )
{
	if (gFileManagerInitialized) return TRUE;
	RegisterDebugTopic( TOPIC_FILE_MANAGER, "File Manager" );
	gFileManagerInitialized = true;
	return( TRUE );
}



//**************************************************************************
//
// FileSystemShutdown
//
//		Shuts down the file system.
//
// Parameter List :
// Return Value :
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

void ShutdownFileManager( void )
{
	if (!gFileManagerInitialized) return;
	gFileManagerInitialized = false;
	UnRegisterDebugTopic( TOPIC_FILE_MANAGER, "File Manager" );
}


//**************************************************************************
//
// FileExists
//
//		Checks if a file exists.
//
// Parameter List :
//
//		STR	->name of file to check existence of
//
// Return Value :
//
//		BOOLEAN	->TRUE if it exists
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//	Oct 2005: Snap - Rewrote, made to check data catalogues
//
//**************************************************************************
BOOLEAN	FileExists( STR strFilename )
{
	return getVFS()->fileExists(vfs::Path(strFilename));
}

// Like FileExists, but also accepts the externalized .jpc.7z form of a graphic.
// The real loader (CreateImage, default JPC_FALLBACK) loads "X.jpc.7z" in place of
// "X.sti", so existence prechecks that gate image/surface/VObject loading must too --
// otherwise a PNG-only install (original .sti removed) rejects graphics the engine can
// actually render. Use this instead of FileExists for any such graphic precheck.
BOOLEAN	GraphicFileExists( STR strFilename )
{
	if( FileExists( strFilename ) )
	{
		return TRUE;
	}
	// Only .sti graphics fall back to the .jpc.7z form -- this mirrors CreateImage's
	// JPC_FALLBACK, which only remaps STCI files. .pcx/.tga/etc. are loaded literally,
	// so we must not claim a .jpc.7z stands in for them.
	std::string alt( strFilename );
	std::string::size_type dot = alt.find_last_of( '.' );
	if( dot != std::string::npos && (alt.size() - dot) == 4
		&& (alt[dot+1] == 's' || alt[dot+1] == 'S')
		&& (alt[dot+2] == 't' || alt[dot+2] == 'T')
		&& (alt[dot+3] == 'i' || alt[dot+3] == 'I') )
	{
		alt.replace( dot, std::string::npos, ".jpc.7z" );
		if( FileExists( alt.c_str() ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

//**************************************************************************
//
// FileExistsNoDB
//
//		Checks if a file exists, but doesn't check the database files.
//
// Parameter List :
//
//		STR	->name of file to check existence of
//
// Return Value :
//
//		BOOLEAN	->TRUE if it exists
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//	Oct 2005: Snap - Rewrote, made to check data catalogues
//
//**************************************************************************
extern BOOLEAN	FileExistsNoDB( STR strFilename )
{
	return getVFS()->fileExists(vfs::Path(strFilename));
}

//**************************************************************************
//
// FileDelete
//
//		Deletes a file.
//
// Parameter List :
//
//		STR	->name of file to delete
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//**************************************************************************	
BOOLEAN	FileDelete( STR strFilename )
{
	return getVFS()->removeFileFromFS(vfs::Path(strFilename));
}

//**************************************************************************
//
// FileOpen
//
//		Opens a file.
//
// Parameter List :
//
//		STR	->filename
//		UIN32		->access - read or write, or both
//		BOOLEAN	->delete on close
//
// Return Value :
//
//		HWFILE	->handle of opened file
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//	Oct 2005: Snap - modified to work with the custom Data directory
//
//**************************************************************************
HWFILE FileOpen( STR strFilename, UINT32 uiOptions, BOOLEAN fDeleteOnClose, STR strProfilename )//dnl ch81 021213
{
	if(strFilename == NULL || strFilename[0] == '\0' || fDeleteOnClose)
	{
		// Delete-on-close was never implemented by the VFS backend. Reject it
		// explicitly instead of returning a handle that silently keeps the file.
		return 0;
	}

	const UINT32 access = uiOptions & FILE_ACCESS_READWRITE;
	const UINT32 dispositionMask = FILE_CREATE_NEW | FILE_CREATE_ALWAYS |
		FILE_OPEN_EXISTING | FILE_OPEN_ALWAYS | FILE_TRUNCATE_EXISTING;
	const UINT32 disposition = uiOptions & dispositionMask;
	if(access != FILE_ACCESS_READ && access != FILE_ACCESS_WRITE)
	{
		// bfVFS exposes separate readable and writable handles. No current JA2
		// caller requests a read/write handle, so fail rather than returning a
		// write-only handle for FILE_ACCESS_READWRITE.
		return 0;
	}
	if(disposition != 0 && (disposition & (disposition - 1)) != 0)
	{
		return 0;
	}

	vfs::Path path(strFilename);
	vfs::IBaseFile *pFile = NULL;
	try
	{
		if(access == FILE_ACCESS_WRITE)
		{
			// Historically callers that omitted a disposition got OPEN_ALWAYS.
			// Preserve that source-compatible default while honoring every
			// disposition that was explicitly requested.
			const UINT32 effectiveDisposition = disposition == 0
				? FILE_OPEN_ALWAYS : disposition;
			const bool exists = getVFS()->fileExists(path);
			if(effectiveDisposition == FILE_CREATE_NEW && exists)
				return 0;
			if((effectiveDisposition == FILE_OPEN_EXISTING ||
				effectiveDisposition == FILE_TRUNCATE_EXISTING) && !exists)
				return 0;

			const bool create = effectiveDisposition == FILE_CREATE_NEW ||
				effectiveDisposition == FILE_CREATE_ALWAYS ||
				effectiveDisposition == FILE_OPEN_ALWAYS;
			const bool truncate = effectiveDisposition == FILE_CREATE_ALWAYS ||
				effectiveDisposition == FILE_TRUNCATE_EXISTING;
			// 'vfs::CVirtualFile::SF_TOP' should be enough, but if for some strange reason
			// file creation fails, we will stop at a writable profile
			// and won't unintentionally mess up a file from another profile
			vfs::COpenWriteFile open_w( path, create, truncate,
				vfs::CVirtualFile::SF_STOP_ON_WRITABLE_PROFILE);
			pFile = &open_w.file();
			const HWFILE handle =
				MakeWriteHandle(vfs::tWritableFile::cast(pFile));
			if(handle == 0)
				return 0;
			open_w.release();
			return handle;
		}
		else
		{
			// Read handles have no meaningful creation operation in bfVFS. The
			// previous implementation always opened an existing file, including
			// the handful of legacy READ|OPEN_ALWAYS call sites, so retain that
			// behavior and reject only dispositions that explicitly require a
			// write/truncate operation.
			if(disposition == FILE_CREATE_NEW || disposition == FILE_CREATE_ALWAYS ||
				disposition == FILE_TRUNCATE_EXISTING)
				return 0;
			if(strProfilename && strProfilename[0])
			{
				vfs::COpenReadFile open_r(vfs::tReadableFile::cast(getVFS()->getFile(path, strProfilename)));
				pFile = &open_r.file();
				const HWFILE handle =
					MakeReadHandle(vfs::tReadableFile::cast(pFile));
				if(handle == 0)
					return 0;
				open_r.release();
				return handle;
			}
			else
			{
				vfs::COpenReadFile open_r(path, vfs::CVirtualFile::SF_TOP);
				pFile = &open_r.file();
				const HWFILE handle =
					MakeReadHandle(vfs::tReadableFile::cast(pFile));
				if(handle == 0)
					return 0;
				open_r.release();
				return handle;
			}
		}
	}
	// sometimes a file is supposed to opened that does not exist (not tested with FileExists())
	// this operation can fail with an exception that the calling code doesn't catch
	// instead we catch it (any exception, not just CBasicException) here and return 0
	catch(vfs::Exception& ex) { SGP_ERROR(ex.what()); }
	catch(...)
	{
		SGP_ERROR( "Caught undefined exception" );
	}
	return 0;
}

//**************************************************************************
//
// FileClose
//
//
// Parameter List :
//
//		HWFILE hFile	->handle to file to close
//
// Return Value :
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************
void FileClose( HWFILE hFile )
{
	std::unique_ptr<FileHandle> handle(GetFileHandle(hFile));
	if(handle && handle->file)
	{
		handle->file->close();
	}
}

//**************************************************************************
//
// FileRead
//
//		To read a file.
//
// Parameter List :
//
//		HWFILE		->handle to file to read from
//		void	*	->source buffer
//		UINT32	->num bytes to read
//		UINT32	->num bytes read
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//		08Dec97:ARM		->return FALSE if bytes to read != bytes read
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

#ifdef JA2TESTVERSION
	extern UINT32 uiTotalFileReadTime;
	extern UINT32 uiTotalFileReadCalls;
	#include "Timer Control.h"

class TimeCounter
{
public:
	TimeCounter() : start_time(GetJA2Clock()) {}
	~TimeCounter()
	{
		uiTotalFileReadTime += GetJA2Clock() - start_time;
		uiTotalFileReadCalls++;
	}
private:
	UINT32 start_time;
};

#endif

BOOLEAN FileRead( HWFILE hFile, PTR pDest, UINT32 uiBytesToRead, UINT32 *puiBytesRead )
{
#ifdef JA2TESTVERSION
	TimeCounter timer;
#endif
	FileHandle* handle = GetFileHandle(hFile);
	if(handle && handle->reader)
	{
		vfs::tReadableFile *pRF = handle->reader;
		if(pRF)
		{
			UINT32 uiBytesRead = 0;
			try
			{
				uiBytesRead = pRF->read((vfs::Byte*)pDest, uiBytesToRead);
			}
			catch(std::exception& ex)
			{
				pRF->close();
				SGP_RETHROW(L"", ex);
			}

			if(puiBytesRead)
			{
				*puiBytesRead = uiBytesRead;
			}
			if(uiBytesToRead != uiBytesRead)
			{
				// Zero the tail we did NOT read so a caller that ignores this FALSE return
				// consumes defined zeros instead of uninitialized memory as data/offsets
				// (the recurring 'ignored FileRead -> uninitialized-data-as-index' crash class).
				if ( pDest && uiBytesRead < uiBytesToRead )
					memset( (UINT8*)pDest + uiBytesRead, 0, uiBytesToRead - uiBytesRead );
				return FALSE;
			}
			return TRUE;
		}
	}
	// No readable file (NULL handle / not opened for read / not castable): nothing was written
	// to pDest. Zero it and report 0 bytes read so the same ignored-return class stays safe.
	if ( pDest )
		memset( pDest, 0, uiBytesToRead );
	if ( puiBytesRead )
		*puiBytesRead = 0;
	return FALSE;
}

BOOLEAN FileReadLine( HWFILE hFile, std::string* pDest )
{
	FileHandle* handle = GetFileHandle(hFile);
	if ( handle && handle->reader &&
		FileCheckEndOfFile( hFile ) == FALSE )
	{
		vfs::tReadableFile *pRF = handle->reader;
		if ( pRF && pDest )
		{
			vfs::CReadLine rl( *pRF, false );
			rl.getLine( *pDest );
			return TRUE;
		}
	}
	return FALSE;
}

//**************************************************************************
//
// FileWrite
//
//		To write a file.
//
// Parameter List :
//
//		HWFILE		->handle to file to write to
//		void	*	->destination buffer
//		UINT32	->num bytes to write
//		UINT32	->num bytes written
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//		08Dec97:ARM		->return FALSE if dwNumBytesToWrite != dwNumBytesWritten
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

BOOLEAN FileWrite( HWFILE hFile, const void *pDest, UINT32 uiBytesToWrite, UINT32 *puiBytesWritten )
{
	if(uiBytesToWrite == 0)//dnl ch38 110909
	{
		if (puiBytesWritten) *puiBytesWritten = 0;
		return(TRUE);
	}
	FileHandle* handle = GetFileHandle(hFile);
	if(handle && handle->writer)
	{
		vfs::tWritableFile *pWF = handle->writer;
		if(pWF)
		{
			UINT32 uiBytesWritten;
			try
			{
				uiBytesWritten = pWF->write((vfs::Byte*)pDest, uiBytesToWrite);
			}
			catch(std::exception& ex)
			{
				pWF->close();
				SGP_RETHROW(L"", ex);
			}

			if (uiBytesToWrite != uiBytesWritten)
			{
				return FALSE;
			}
			if ( puiBytesWritten )
			{
				*puiBytesWritten = uiBytesWritten;
			}
			return TRUE;
		}
	}
	return FALSE;
}

//**************************************************************************
//
// FileLoad
//
//		To open, read, and close a file.
//
// Parameter List :
//
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//		08Dec97:ARM		->return FALSE if bytes to read != bytes read (CHECKF is inappropriate?)
//
//**************************************************************************

BOOLEAN FileLoad( STR strFilename, PTR pDest, UINT32 uiBytesToRead, UINT32 *puiBytesRead )
{
	vfs::tReadableFile *pFile = getVFS()->getReadFile(vfs::Path(strFilename));
	vfs::COpenReadFile rfile(pFile);
	if(pFile)
	{
		UINT32 uiNumBytesRead;
		SGP_TRYCATCH_RETHROW(uiNumBytesRead = pFile->read((vfs::Byte*)pDest,uiBytesToRead), L"");

		if (uiBytesToRead != uiNumBytesRead)
		{
			return FALSE;
		}
		if ( puiBytesRead )
		{
			*puiBytesRead = uiNumBytesRead;
		}
		CHECKF( uiNumBytesRead == uiBytesToRead );
		return TRUE;
	}
	return FALSE;
}

//**************************************************************************
//
// FilePrintf
//
//		To printf to a file.
//
// Parameter List :
//
//		HWFILE	->handle to file to seek in
//		...		->arguments, 1st of which should be a string
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************
#ifndef DIM
# define DIM(x) (sizeof(x)/sizeof(x[0]))	/* made StringLen Save, Sergeant_Kolja, 2007-06-10 */
#endif


BOOLEAN FilePrintf( HWFILE hFile, STR8	strFormatted, ... )
{
	CHAR8		strToSend[160]; /* itemdescription of item 0 will NOT fit if only 80 Chars per Line!, Sergeant_Kolja, 2007-06-10 */
	va_list	argptr;
	BOOLEAN fRetVal = FALSE;

	va_start(argptr, strFormatted);
	vsnprintf( strToSend, DIM(strToSend), strFormatted, argptr ); /* made StringLen Save, Sergeant_Kolja, 2007-06-10 */
	strToSend[ DIM(strToSend)-1 ] = 0;
	va_end(argptr);
	
	fRetVal = FileWrite( hFile, strToSend, strlen(strToSend), NULL );
	return( fRetVal );
}

//**************************************************************************
//
// FileSeek
//
//		To seek to a position in a file.
//
// Parameter List :
//
//		HWFILE	->handle to file to seek in
//		UINT32	->distance to seek
//		UINT8		->how to seek
//
// Return Value :
//
//		BOOLEAN	->TRUE if successful
//					->FALSE if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

BOOLEAN FileSeek( HWFILE hFile, UINT32 uiDistance, UINT8 uiHow )
{
	INT32 iDistance = (INT32)uiDistance;

	FileHandle* handle = GetFileHandle(hFile);
	if(handle)
	{
		vfs::IBaseFile::ESeekDir eSD;
		if ( uiHow == FILE_SEEK_FROM_START )
		{
			eSD = vfs::IBaseFile::SD_BEGIN;
		}
		else if ( uiHow == FILE_SEEK_FROM_END )
		{
			eSD = vfs::IBaseFile::SD_END;
			if( iDistance > 0 )
			{
				iDistance = -(iDistance);
			}
		}
		else
		{
			eSD = vfs::IBaseFile::SD_CURRENT;
		}

		if(handle->writer)
		{
			vfs::tWritableFile *pWF = handle->writer;
			if(pWF)
			{
				SGP_TRYCATCH_RETHROW(pWF->setWritePosition(iDistance, eSD), L"");
				return TRUE;
			}
		}
		else if(handle->reader)
		{
			vfs::tReadableFile *pRF = handle->reader;
			if(pRF)
			{
				SGP_TRYCATCH_RETHROW(pRF->setReadPosition(iDistance, eSD), L"");
				return TRUE;
			}
		}
	}
	return FALSE;
}

//**************************************************************************
//
// FileGetPos
//
//		To get the current position in a file.
//
// Parameter List :
//
//		HWFILE	->handle to file
//
// Return Value :
//
//		INT32		->current offset in file if successful
//					->-1 if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

INT32 FileGetPos( HWFILE hFile )
{
	FileHandle* handle = GetFileHandle(hFile);
	if(handle && handle->writer)
	{
		vfs::tWritableFile *pWF = handle->writer;
		if(pWF)
		{
			return pWF->getWritePosition();
		}
	}
	else if(handle && handle->reader)
	{
		vfs::tReadableFile *pRF = handle->reader;
		if(pRF)
		{
			return pRF->getReadPosition();
		}
	}

	return BAD_INDEX;
}

//**************************************************************************
//
// FileGetSize
//
//		To get the current file size.
//
// Parameter List :
//
//		HWFILE	->handle to file
//
// Return Value :
//
//		INT32		->file size in file if successful
//					->0 if not
//
// Modification history :
//
//		24sep96:HJH		->creation
//
//		9 Feb 98	DEF - modified to work with the library system
//
//**************************************************************************

UINT32 FileGetSize( HWFILE hFile )
{
	FileHandle* handle = GetFileHandle(hFile);
	if(handle && handle->file)
	{
		return handle->file->getSize();
	}
	return 0;
}


BOOLEAN SetFileManCurrentDirectory( STR pcDirectory )
{
	try
	{
		vfs::OS::setCurrectDirectory(pcDirectory);
	}
	catch(vfs::Exception& ex)
	{
		SGP_ERROR(ex.what());
		return FALSE;
	}
	return TRUE;
}


BOOLEAN GetFileManCurrentDirectory( STRING512 pcDirectory )
{
	try
	{
		vfs::Path sDir;
		vfs::OS::getCurrentDirectory(sDir);
		strncpy(pcDirectory, sDir.to_string().c_str(), 511); pcDirectory[511] = '\0';
	}
	catch(vfs::Exception& ex)
	{
		SGP_ERROR(ex.what());
		return FALSE;
	}
	return TRUE;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Removes ALL FILES in the specified directory (and all subdirectories with their files if fRecursive is TRUE)
// Use EraseDirectory() to simply delete directory contents without deleting the directory itself
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN RemoveFileManDirectory( const CHAR8 *pcDirectory, BOOLEAN fRecursive )
{
	// ignore 'recursive' flag, just delete every file in that subtree (but leave the directories)
	return getVFS()->removeDirectoryFromFS(pcDirectory);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Removes ALL FILES in the specified directory but leaves the directory alone.	Does not affect any subdirectories!
// Use RemoveFilemanDirectory() to also delete the directory itself, or to recursively delete subdirectories.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN EraseDirectory( const CHAR8 *pcDirectory)
{
	// ignore 'recursive' flag, just delete every file in that subtree (but leave the directories)
	return getVFS()->removeDirectoryFromFS(pcDirectory);
}


BOOLEAN GetExecutableDirectory( STRING512 pcDirectory )
{
	vfs::Path exe_dir, exe_file;
	vfs::OS::getExecutablePath(exe_dir, exe_file);
	strncpy(pcDirectory, exe_dir.to_string().c_str(), 511); pcDirectory[511] = '\0';
	return true;
}

static vfs::CVirtualFileSystem::Iterator file_iter; 
BOOLEAN GetFileFirst( const CHAR8 *pSpec, GETFILESTRUCT *pGFStruct )
{
	CHECKF( pSpec != NULL );
	CHECKF( pGFStruct != NULL );

	file_iter = getVFS()->begin(pSpec);
	if(!file_iter.end())
	{
		vfs::Path const& path = file_iter.value()->getName();
		std::string s = path.to_string();
		::size_t size = s.length();
		size = std::min< ::size_t>(size,260-1);
		snprintf( pGFStruct->zFileName, size + 1, "%s", s.c_str());	// was sprintf(dst, s.c_str()): non-literal format + copied full string past the 260 cap (overflow)
		pGFStruct->zFileName[size] = 0;
		
		pGFStruct->iFindHandle = 0;
		pGFStruct->uiFileSize = file_iter.value()->getSize();
		pGFStruct->uiFileAttribs = ( file_iter.value()->implementsWritable() ? FILE_IS_NORMAL : FILE_IS_READONLY );

		return TRUE;
	}
	return FALSE;
}

BOOLEAN GetFileNext( GETFILESTRUCT *pGFStruct )
{
	if(!file_iter.end())
	{
		file_iter.next();
	}
	if(!file_iter.end())
	{
		vfs::Path const& path = file_iter.value()->getName();
		std::string s = path.to_string();
		::size_t size = s.length();
		size = std::min< ::size_t>(size,260-1);
		snprintf( pGFStruct->zFileName, size + 1, "%s", s.c_str());	// was sprintf(dst, s.c_str()): non-literal format + copied full string past the 260 cap (overflow)
		pGFStruct->zFileName[size] = 0;

		pGFStruct->iFindHandle = 0;
		pGFStruct->uiFileSize = file_iter.value()->getSize();
		pGFStruct->uiFileAttribs = ( file_iter.value()->implementsWritable() ? FILE_IS_NORMAL : FILE_IS_READONLY );

		return TRUE;
	}
	return FALSE;
}

void GetFileClose( GETFILESTRUCT *pGFStruct )
{
	file_iter = vfs::CVirtualFileSystem::Iterator();
}


//returns true if at end of file, else false
BOOLEAN	FileCheckEndOfFile( HWFILE hFile )
{
	vfs::size_t current_position, max_position;
	FileHandle* handle = GetFileHandle(hFile);

	if(handle && handle->writer)
	{
		vfs::tWritableFile *pWF = handle->writer;
		if(pWF)
		{
			current_position = pWF->getWritePosition();
			max_position = pWF->getSize();
			return current_position >= max_position;
		}
	}
	else if(handle && handle->reader)
	{
		vfs::tReadableFile *pRF = handle->reader;
		if(pRF)
		{
			current_position = pRF->getReadPosition();
			max_position = pRF->getSize();
			return current_position >= max_position;
		}
	}
	return FALSE;
}


UINT32 FileSize(STR strFilename)
{
	vfs::IBaseFile *pFile = getVFS()->getFile(vfs::Path(strFilename));
	if(pFile)
	{
		return pFile->getSize();
	}
	return 0;
}


// Flugente: simple wrapper to check whether an audio file in mp3/ogg/wav format exists
BOOLEAN	SoundFileExists( STR strFilename, CHAR8 *zFoundFilename, size_t foundFilenameSize )
{
	if (!strFilename || !zFoundFilename || foundFilenameSize == 0)
	{
		return FALSE;
	}

	int result = snprintf( zFoundFilename, foundFilenameSize, "%s.mp3", strFilename );
	if (result < 0 || (size_t)result >= foundFilenameSize)
	{
		zFoundFilename[0] = '\0';
		return FALSE;
	}
	if ( !FileExists( zFoundFilename ) )
	{
		result = snprintf( zFoundFilename, foundFilenameSize, "%s.ogg", strFilename );
		if (result < 0 || (size_t)result >= foundFilenameSize)
		{
			zFoundFilename[0] = '\0';
			return FALSE;
		}
		if ( !FileExists( zFoundFilename ) )
		{
			result = snprintf( zFoundFilename, foundFilenameSize, "%s.wav", strFilename );
			if (result < 0 || (size_t)result >= foundFilenameSize)
			{
				zFoundFilename[0] = '\0';
				return FALSE;
			}
		}
	}

	return FileExists( zFoundFilename );
}
