
## 7-zip

# LZMA SDK 26.01 (upgraded from 9.22 beta). bfVFS only reads uncompressed
# (copy-method) 7z archives by direct offset, but 7z *headers* may be LZMA/PPMd
# compressed, so we vendor the decode-only source set (extract-only, single
# thread, PPMd enabled). All headers are vendored (interdependent); the .c set
# is the SDK's own 7zdec decoder objects (see C/Util/7z/makefile.gcc), minus
# OS file I/O (bfVFS supplies its own stream) and the asm LZMA fast path.
file(GLOB INCLUDE_7z "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

set(SOURCE_7z
  src/7zAlloc.c
  src/7zArcIn.c
  src/7zBuf.c
  src/7zBuf2.c
  src/7zCrc.c
  src/7zCrcOpt.c
  src/7zDec.c
  src/7zStream.c
  src/Bcj2.c
  src/Bra.c
  src/Bra86.c
  src/BraIA64.c
  src/CpuArch.c
  src/Delta.c
  src/Lzma2Dec.c
  src/LzmaDec.c
  src/Ppmd7.c
  src/Ppmd7Dec.c
)

source_group("" FILES ${INCLUDE_7z} ${SOURCE_7z})

####################################################

if(BUILD_BFVFS_7ZIP_SHARED)
  set(BUILD_SHARED_LIBS ON)
  add_definitions(-DSZIP_EXPORT)
  set(DEFINITIONS ${DEFINITIONS} -DSZIP_IMPORT)
else()
  set(BUILD_SHARED_LIBS OFF)
  add_definitions(-DSZIP_STATIC)
  set(DEFINITIONS ${DEFINITIONS} -DSZIP_STATIC)
endif()

# Single-thread (_7ZIP_ST still maps to Z7_ST in 26.01), decode-only build,
# with PPMd decoding compiled in (7z headers/streams may use PPMd).
add_definitions(-D_7ZIP_ST -DZ7_EXTRACT_ONLY -DZ7_PPMD_SUPPORT)

set(BFVFS_7ZIP_TARGET "7z" CACHE INTERNAL "")

add_library( ${BFVFS_7ZIP_TARGET} ${INCLUDE_7z} ${SOURCE_7z} )

set_property(
  TARGET ${BFVFS_7ZIP_TARGET}
  PROPERTY ARCHIVE_OUTPUT_DIRECTORY ${BFVFS_LIBRARY_DIRS}
)
set_property(
  TARGET ${BFVFS_7ZIP_TARGET}
  PROPERTY LIBRARY_OUTPUT_DIRECTORY ${BFVFS_LIBRARY_DIRS}
)
set_property(
  TARGET ${BFVFS_7ZIP_TARGET}
  PROPERTY RUNTIME_OUTPUT_DIRECTORY ${BFVFS_RUNTIME_DIRS}
)
####################################################

set(BFVFS_7ZIP_DEFINITIONS ${DEFINITIONS} CACHE INTERNAL "")
