// Legacy SLF archive reader, superseded by bfVFS's own SLF backend
// (`ext/VFS/src/Ext/slf/vfs_slf_library.cpp`, enabled by VFS_WITH_SLF).
// Mounting + reading SLFs goes through bfVFS now; this TU only needs
// to keep its publicly-advertised symbols linkable.
//
// Two real call sites remain in the source tree:
//   * `ShutDownFileDatabase()` from Ja2/gameloop.cpp -- cleanup. The
//     database is never initialized, so the no-op is correct.
//   * `gzCdDirectory[]` from Ja2/GameSettings.cpp -- the CD-root path
//     buffer is still owned here.
// Everything else (OpenLibrary, CloseLibrary, LoadDataFromLibrary,
// etc.) is dead code; the two commented-out laptop.cpp call sites are
// the only references and they've been disabled for years.
//
// Dropping the legacy CreateFile/ReadFile/WriteFile-based bodies lets
// us also drop the matching Win32 file I/O stubs from msvc_compat.h
// (Phase 2, item 8).

#include "types.h"
#include "LibraryDataBase.h"

// Owned-here globals that other TUs still reference.
INT16   gsCurrentLibrary = -1;
CHAR8   gzCdDirectory[ SGPFILENAME_LEN ];

BOOLEAN InitializeFileDatabase()                                                                            { return TRUE; }
BOOLEAN ReopenCDLibraries(void)                                                                             { return TRUE; }
BOOLEAN ShutDownFileDatabase()                                                                              { return TRUE; }
BOOLEAN CheckForLibraryExistence( STR )                                                                     { return FALSE; }
BOOLEAN InitializeLibrary( STR, LibraryHeaderStruct *, BOOLEAN )                                            { return FALSE; }
BOOLEAN LoadDataFromLibrary( INT16, UINT32, PTR, UINT32, UINT32 *pBytesRead )                               { if (pBytesRead) *pBytesRead = 0; return FALSE; }
BOOLEAN CheckIfFileExistInLibrary( STR )                                                                    { return FALSE; }
HWFILE  OpenFileFromLibrary( STR )                                                                          { return 0; }
HWFILE  CreateRealFileHandle( HANDLE )                                                                      { return 0; }
BOOLEAN CloseLibraryFile( INT16, UINT32 )                                                                   { return FALSE; }
BOOLEAN GetLibraryAndFileIDFromLibraryFileHandle( HWFILE, INT16 *, UINT32 * )                               { return FALSE; }
BOOLEAN LibraryFileSeek( INT16, UINT32, UINT32, UINT8 )                                                     { return FALSE; }
BOOLEAN CloseLibrary( INT16 )                                                                               { return FALSE; }
BOOLEAN OpenLibrary( INT16 )                                                                                { return FALSE; }
BOOLEAN IsLibraryOpened( INT16 )                                                                            { return FALSE; }
BOOLEAN GetLibraryFileTime( INT16, UINT32, SGP_FILETIME * )                                                 { return FALSE; }
