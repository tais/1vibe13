# STATIC: SDL3_mixer flips BUILD_SHARED_LIBS ON globally, which would
# otherwise make this a DLL/.so. On Windows a zlib DLL only exports
# its symbols with the right ZLIB_DLL/ZEXPORT decoration, so libpng's
# DLL link fails with undefined crc32/inflate/deflate. Static
# sidesteps the export/import dance on every platform (and these are
# tiny libs not worth shipping separately).
add_library(zlib STATIC
"gzread.c"
"gzlib.c"
"gzclose.c"
"crc32.c"
"zutil.c"
"uncompr.c"
"compress.c"
"inffast.c"
"inflate.c"
"adler32.c"
"infback.c"
"deflate.c"
"gzwrite.c"
"inftrees.c"
"trees.c"
)
target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Tell zlib it has <unistd.h> on POSIX (what zlib's own ./configure does).
# zconf.h then includes it so gzread.c's read()/close()/SEEK_* are declared;
# without it modern clang errors on the implicit declarations. Replaces the
# old in-source gzguts.h patch so future zlib bumps stay drop-in. Not on
# Windows (no unistd.h there; zlib uses <io.h>).
if(NOT WIN32)
  target_compile_definitions(zlib PUBLIC Z_HAVE_UNISTD_H)
endif()
