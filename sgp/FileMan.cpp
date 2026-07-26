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
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>

namespace
{
const UINT32 kReadBufferSize = 8192;
const UINT32 kMaximumFileSearches = 20;

typedef vfs::CVirtualFileSystem::Iterator FileSearchIterator;
std::unique_ptr<FileSearchIterator> s_fileSearches[kMaximumFileSearches];

enum class FileAccessMode
{
	Read,
	Write
};

// A bfVFS file object owns one native cursor. Multiple calls to openRead()
// return that same cursor, and close() from either caller closes it for all
// callers. Keep one shared native lifetime per VFS object while every public
// FileMan handle below owns its own logical cursor and read-ahead buffer.
struct OpenFileState
{
	OpenFileState(vfs::IBaseFile* base, vfs::tReadableFile* read,
		vfs::tWritableFile* write, FileAccessMode access) noexcept
		: file(base), reader(read), writer(write), mode(access)
	{
	}

	void Close() noexcept
	{
		if(!file)
			return;
		vfs::IBaseFile* closingFile = file;
		file = nullptr;
		reader = nullptr;
		writer = nullptr;
		try
		{
			closingFile->close();
		}
		catch(std::exception& ex)
		{
			try { SGP_ERROR(ex.what()); } catch(...) {}
		}
		catch(...)
		{
			try
			{
				SGP_ERROR("Caught undefined exception while closing file");
			}
			catch(...)
			{
			}
		}
	}

	~OpenFileState() noexcept
	{
		Close();
	}

	vfs::IBaseFile* file;
	vfs::tReadableFile* reader;
	vfs::tWritableFile* writer;
	FileAccessMode mode;
	std::mutex ioMutex;
};

std::mutex s_openFileStatesMutex;
std::unordered_map<vfs::IBaseFile*, std::weak_ptr<OpenFileState>>
	s_openFileStates;

// The registry mutex must be held while resolving or pruning an entry.
std::shared_ptr<OpenFileState> OpenFileStateFor(vfs::IBaseFile* file)
{
	const auto found = s_openFileStates.find(file);
	if(found == s_openFileStates.end())
		return {};

	std::shared_ptr<OpenFileState> state = found->second.lock();
	if(!state)
	{
		s_openFileStates.erase(found);
		return {};
	}
	return state;
}

std::shared_ptr<OpenFileState> AcquireReadFileState(
	vfs::tReadableFile* file)
{
	if(!file)
		return {};

	std::lock_guard<std::mutex> lock(s_openFileStatesMutex);
	if(std::shared_ptr<OpenFileState> existing = OpenFileStateFor(file))
	{
		return existing->mode == FileAccessMode::Read
			? existing : std::shared_ptr<OpenFileState>();
	}

	vfs::COpenReadFile opened(file);
	std::shared_ptr<OpenFileState> state = std::make_shared<OpenFileState>(
		file, file, nullptr, FileAccessMode::Read);
	s_openFileStates[file] = state;
	opened.release();
	return state;
}

std::shared_ptr<OpenFileState> AcquireWriteFileState(
	const vfs::Path& path, bool create, bool truncate)
{
	std::lock_guard<std::mutex> lock(s_openFileStatesMutex);
	vfs::IBaseFile* file = getVFS()->getFile(
		path, vfs::CVirtualFile::SF_STOP_ON_WRITABLE_PROFILE);
	if(file)
	{
		if(std::shared_ptr<OpenFileState> existing =
			OpenFileStateFor(file))
		{
			if(existing->mode != FileAccessMode::Write)
				return {};
			// Truncating underneath another live logical handle would silently
			// invalidate that handle's position and buffered expectations.
			return truncate ? std::shared_ptr<OpenFileState>() : existing;
		}
	}

	vfs::COpenWriteFile opened(path, create, truncate,
		vfs::CVirtualFile::SF_STOP_ON_WRITABLE_PROFILE);
	vfs::tWritableFile* writer = &opened.file();
	file = writer;
	std::shared_ptr<OpenFileState> state = std::make_shared<OpenFileState>(
		file, nullptr, writer, FileAccessMode::Write);
	s_openFileStates[file] = state;
	opened.release();
	return state;
}

// HWFILE is intentionally opaque outside FileMan. Resolve its VFS interface
// once at open time; hot reads and writes retain direct pointers while their
// shared state serializes the one native VFS cursor.
struct FileHandle
{
	explicit FileHandle(std::shared_ptr<OpenFileState> openState) noexcept
		: state(std::move(openState)),
		  file(state ? state->file : nullptr),
		  reader(state ? state->reader : nullptr),
		  writer(state ? state->writer : nullptr),
		  position(0),
		  readBufferPos(0), readBufferLen(0)
	{
	}
	~FileHandle() noexcept;

	std::shared_ptr<OpenFileState> state;
	vfs::IBaseFile* file;
	vfs::tReadableFile* reader;
	vfs::tWritableFile* writer;
	vfs::size_t position;
	// Windows VFS reads map directly to ReadFile. Cache small legacy reads on
	// the handle so scalar save fields share one native I/O block across reader
	// objects while FileGetPos/FileSeek continue to expose a logical position.
	std::unique_ptr<UINT8[]> readBuffer;
	UINT32 readBufferPos;
	UINT32 readBufferLen;
};

void ReleaseOpenFileState(
	std::shared_ptr<OpenFileState>& state) noexcept
{
	if(!state)
		return;

	try
	{
		std::lock_guard<std::mutex> lock(s_openFileStatesMutex);
		if(state.use_count() == 1)
		{
			vfs::IBaseFile* file = state->file;
			state->Close();
			if(file)
				s_openFileStates.erase(file);
		}
		state.reset();
	}
	catch(...)
	{
		// Destructors must not leak a close failure into legacy callers.
		state.reset();
	}
}

FileHandle::~FileHandle() noexcept
{
	// Drop the final owner while the registry is locked, so a concurrent open
	// cannot attach to the native stream between its last close and destruction.
	ReleaseOpenFileState(state);
}

FileHandle* GetFileHandle(HWFILE handle)
{
	return reinterpret_cast<FileHandle*>(handle);
}

UINT32 GetBufferedBytes(const FileHandle* handle)
{
	return handle && handle->readBufferLen >= handle->readBufferPos ?
		handle->readBufferLen - handle->readBufferPos : 0;
}

void ClearReadBuffer(FileHandle* handle)
{
	if (!handle)
		return;
	handle->readBufferPos = 0;
	handle->readBufferLen = 0;
}

bool EnsureReadBuffer(FileHandle* handle)
{
	if (!handle)
		return false;
	if (!handle->readBuffer)
		handle->readBuffer.reset(new (std::nothrow) UINT8[kReadBufferSize]);
	return handle->readBuffer != nullptr;
}

void SynchronizeReadBuffer(FileHandle* handle)
{
	ClearReadBuffer(handle);
}

bool PositionNativeCursor(FileHandle* handle)
{
	if(!handle || !handle->state ||
		handle->position > static_cast<vfs::size_t>(
			std::numeric_limits<vfs::offset_t>::max()))
	{
		return false;
	}

	const vfs::offset_t position =
		static_cast<vfs::offset_t>(handle->position);
	if(handle->reader)
	{
		handle->reader->setReadPosition(
			position, vfs::IBaseFile::SD_BEGIN);
		return true;
	}
	if(handle->writer)
	{
		handle->writer->setWritePosition(
			position, vfs::IBaseFile::SD_BEGIN);
		return true;
	}
	return false;
}

vfs::size_t ReadNative(FileHandle* handle, void* destination,
	vfs::size_t bytes)
{
	if(!handle || !handle->state || !handle->reader)
		return 0;
	std::lock_guard<std::mutex> lock(handle->state->ioMutex);
	if(!PositionNativeCursor(handle))
		return 0;
	return handle->reader->read(
		static_cast<vfs::Byte*>(destination), bytes);
}

vfs::size_t WriteNative(FileHandle* handle, const void* source,
	vfs::size_t bytes)
{
	if(!handle || !handle->state || !handle->writer)
		return 0;
	std::lock_guard<std::mutex> lock(handle->state->ioMutex);
	if(!PositionNativeCursor(handle))
		return 0;
	return handle->writer->write(
		static_cast<const vfs::Byte*>(source), bytes);
}

FileSearchIterator* GetFileSearch(INT32 handle)
{
	if(handle <= 0 ||
		handle > static_cast<INT32>(kMaximumFileSearches))
	{
		return nullptr;
	}
	return s_fileSearches[handle - 1].get();
}

INT32 StoreFileSearch(std::unique_ptr<FileSearchIterator> iterator)
{
	for(UINT32 index = 0; index < kMaximumFileSearches; ++index)
	{
		if(!s_fileSearches[index])
		{
			s_fileSearches[index] = std::move(iterator);
			return static_cast<INT32>(index + 1);
		}
	}
	return -1;
}

void ReleaseFileSearch(INT32 handle)
{
	if(handle > 0 &&
		handle <= static_cast<INT32>(kMaximumFileSearches))
	{
		s_fileSearches[handle - 1].reset();
	}
}

void ReleaseAllFileSearches()
{
	for(UINT32 index = 0; index < kMaximumFileSearches; ++index)
		s_fileSearches[index].reset();
}

HWFILE MakeFileHandle(std::shared_ptr<OpenFileState> state)
{
	if(!state)
		return 0;
	FileHandle* handle =
		new (std::nothrow) FileHandle(std::move(state));
	if(!handle)
		ReleaseOpenFileState(state);
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
	ReleaseAllFileSearches();
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
			return MakeFileHandle(
				AcquireWriteFileState(path, create, truncate));
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
				return MakeFileHandle(AcquireReadFileState(
					vfs::tReadableFile::cast(
						getVFS()->getFile(path, strProfilename))));
			}
			else
			{
				return MakeFileHandle(AcquireReadFileState(
					getVFS()->getReadFile(
						path, vfs::CVirtualFile::SF_TOP)));
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
	delete GetFileHandle(hFile);
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
	if(puiBytesRead)
		*puiBytesRead = 0;

	FileHandle* handle = GetFileHandle(hFile);
	if(handle && handle->reader)
	{
		vfs::tReadableFile *pRF = handle->reader;
		if(pRF && (pDest || uiBytesToRead == 0))
		{
			UINT8* destination = static_cast<UINT8*>(pDest);
			UINT32 totalRead = 0;
			try
			{
				while(totalRead < uiBytesToRead)
				{
					const UINT32 buffered = GetBufferedBytes(handle);
					if(buffered != 0)
					{
						const UINT32 remaining = uiBytesToRead - totalRead;
						const UINT32 take =
							(remaining < buffered) ? remaining : buffered;
						memcpy(destination + totalRead,
							handle->readBuffer.get() + handle->readBufferPos,
							take);
						handle->readBufferPos += take;
						totalRead += take;
						handle->position += take;
						continue;
					}

					ClearReadBuffer(handle);
					const UINT32 remaining = uiBytesToRead - totalRead;
					if(remaining >= kReadBufferSize ||
						!EnsureReadBuffer(handle))
					{
						const UINT32 read = static_cast<UINT32>(
							ReadNative(handle, destination + totalRead,
								remaining));
						totalRead += read;
						handle->position += read;
						break;
					}

					handle->readBufferLen = static_cast<UINT32>(
						ReadNative(handle, handle->readBuffer.get(),
							kReadBufferSize));
					if(handle->readBufferLen == 0)
						break;
				}
			}
			catch(std::exception& ex)
			{
				SGP_RETHROW(L"", ex);
			}

			if(puiBytesRead)
			{
				*puiBytesRead = totalRead;
			}
			if(uiBytesToRead != totalRead)
			{
				// Zero the tail we did NOT read so a caller that ignores this FALSE return
				// consumes defined zeros instead of uninitialized memory as data/offsets
				// (the recurring 'ignored FileRead -> uninitialized-data-as-index' crash class).
				if ( pDest && totalRead < uiBytesToRead )
					memset( (UINT8*)pDest + totalRead, 0,
						uiBytesToRead - totalRead );
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
	if ( handle && handle->reader && pDest )
	{
		// CReadLine has its own line-sized buffer and rewinds its unread tail.
		// First put our block cache at the same logical position.
		SynchronizeReadBuffer(handle);
		if ( FileCheckEndOfFile( hFile ) != FALSE )
			return FALSE;

		vfs::tReadableFile *pRF = handle->reader;
		if ( pRF && handle->state )
		{
			std::lock_guard<std::mutex> lock(handle->state->ioMutex);
			if(!PositionNativeCursor(handle))
				return FALSE;
			vfs::CReadLine rl( *pRF, false );
			rl.getLine( *pDest );
			handle->position = pRF->getReadPosition();
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
				uiBytesWritten = static_cast<UINT32>(
					WriteNative(handle, pDest, uiBytesToWrite));
				handle->position += uiBytesWritten;
			}
			catch(std::exception& ex)
			{
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
	HWFILE file = FileOpen(strFilename,
		FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	if(!file)
	{
		if(puiBytesRead)
			*puiBytesRead = 0;
		if(pDest)
			memset(pDest, 0, uiBytesToRead);
		return FALSE;
	}

	try
	{
		const BOOLEAN loaded =
			FileRead(file, pDest, uiBytesToRead, puiBytesRead);
		FileClose(file);
		return loaded;
	}
	catch(...)
	{
		FileClose(file);
		throw;
	}
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
	FileHandle* handle = GetFileHandle(hFile);
	if(!handle || !handle->state)
		return FALSE;

	const INT32 distance = static_cast<INT32>(uiDistance);
	if(handle->reader && uiHow == FILE_SEEK_FROM_CURRENT)
	{
		const INT64 bufferedTarget =
			static_cast<INT64>(handle->readBufferPos) +
			static_cast<INT64>(distance);
		if(bufferedTarget >= 0 &&
			bufferedTarget <= static_cast<INT64>(handle->readBufferLen))
		{
			handle->readBufferPos = static_cast<UINT32>(bufferedTarget);
			handle->position = static_cast<vfs::size_t>(
				static_cast<INT64>(handle->position) + distance);
			return TRUE;
		}
	}

	std::lock_guard<std::mutex> lock(handle->state->ioMutex);
	INT64 target = 0;
	if(uiHow == FILE_SEEK_FROM_START)
	{
		target = distance;
	}
	else if(uiHow == FILE_SEEK_FROM_END)
	{
		const vfs::size_t size = handle->file->getSize();
		if(size > static_cast<vfs::size_t>(
			std::numeric_limits<INT64>::max()))
		{
			return FALSE;
		}
		const INT64 endDistance =
			distance > 0 ? -static_cast<INT64>(distance) : distance;
		target = static_cast<INT64>(size) + endDistance;
	}
	else
	{
		if(handle->position > static_cast<vfs::size_t>(
			std::numeric_limits<INT64>::max()))
		{
			return FALSE;
		}
		target = static_cast<INT64>(handle->position) + distance;
	}

	if(target < 0 ||
		static_cast<UINT64>(target) >
			static_cast<UINT64>(
				std::numeric_limits<vfs::offset_t>::max()))
	{
		return FALSE;
	}

	const vfs::size_t previous = handle->position;
	handle->position = static_cast<vfs::size_t>(target);
	try
	{
		if(!PositionNativeCursor(handle))
		{
			handle->position = previous;
			return FALSE;
		}
	}
	catch(...)
	{
		handle->position = previous;
		throw;
	}

	ClearReadBuffer(handle);
	return TRUE;
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
	if(!handle || (!handle->reader && !handle->writer) ||
		handle->position > static_cast<vfs::size_t>(
			std::numeric_limits<INT32>::max()))
	{
		return BAD_INDEX;
	}
	return static_cast<INT32>(handle->position);
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
	if(handle && handle->file && handle->state)
	{
		std::lock_guard<std::mutex> lock(handle->state->ioMutex);
		const vfs::size_t size = handle->file->getSize();
		return size <= std::numeric_limits<UINT32>::max()
			? static_cast<UINT32>(size)
			: 0;
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

static BOOLEAN PopulateFileSearchResult(
	FileSearchIterator& iterator, GETFILESTRUCT* result)
{
	if(!result || iterator.end())
		return FALSE;

	vfs::tReadableFile* file = iterator.value();
	if(!file)
		return FALSE;

	const std::string name = file->getName().to_string();
	snprintf(result->zFileName, sizeof(result->zFileName),
		"%s", name.c_str());
	result->zFileName[sizeof(result->zFileName) - 1] = '\0';
	result->uiFileSize = file->getSize();
	result->uiFileAttribs =
		file->implementsWritable() ? FILE_IS_NORMAL : FILE_IS_READONLY;
	return TRUE;
}

BOOLEAN GetFileFirst( const CHAR8 *pSpec, GETFILESTRUCT *pGFStruct )
{
	CHECKF( pSpec != NULL );
	CHECKF( pGFStruct != NULL );

	pGFStruct->iFindHandle = -1;
	std::unique_ptr<FileSearchIterator> iterator(
		new (std::nothrow) FileSearchIterator(getVFS()->begin(pSpec)));
	if(!iterator || iterator->end())
		return FALSE;

	const INT32 handle = StoreFileSearch(std::move(iterator));
	if(handle < 0)
		return FALSE;

	FileSearchIterator* stored = GetFileSearch(handle);
	if(!stored || !PopulateFileSearchResult(*stored, pGFStruct))
	{
		ReleaseFileSearch(handle);
		return FALSE;
	}

	pGFStruct->iFindHandle = handle;
	return TRUE;
}

BOOLEAN GetFileNext( GETFILESTRUCT *pGFStruct )
{
	CHECKF( pGFStruct != NULL );

	const INT32 handle = pGFStruct->iFindHandle;
	FileSearchIterator* iterator = GetFileSearch(handle);
	if(!iterator)
	{
		pGFStruct->iFindHandle = -1;
		return FALSE;
	}

	iterator->next();
	if(iterator->end())
	{
		ReleaseFileSearch(handle);
		pGFStruct->iFindHandle = -1;
		return FALSE;
	}

	if(PopulateFileSearchResult(*iterator, pGFStruct))
		return TRUE;

	ReleaseFileSearch(handle);
	pGFStruct->iFindHandle = -1;
	return FALSE;
}

void GetFileClose( GETFILESTRUCT *pGFStruct )
{
	if(!pGFStruct)
		return;
	ReleaseFileSearch(pGFStruct->iFindHandle);
	pGFStruct->iFindHandle = -1;
}


//returns true if at end of file, else false
BOOLEAN	FileCheckEndOfFile( HWFILE hFile )
{
	FileHandle* handle = GetFileHandle(hFile);
	if(handle && handle->file && handle->state &&
		(handle->reader || handle->writer))
	{
		std::lock_guard<std::mutex> lock(handle->state->ioMutex);
		return handle->position >= handle->file->getSize();
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
