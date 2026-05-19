# libsmacker -- Smacker (.SMK) video decoder.
#
# Vendored from libsmacker 1.2.0r43 (SourceForge release, 2021-06-06,
# MD5 9c8d1a521e89a0481f7c868ad2d23d7b). Upstream is at
# https://sourceforge.net/projects/libsmacker/ but SourceForge has had
# availability issues in the past and the project is barely maintained,
# so we keep a local copy rather than FetchContent.
#
# License is LGPL 2.1 (see COPYING). We link statically -- LGPL allows
# this provided users can relink the application against a modified
# libsmacker, which they can do trivially by replacing these source
# files. JA2 stracciatella's source is itself GPL-licensed, which is
# LGPL-compatible.
#
# Files:
#   smacker.c       -- the decoder
#   smacker.h       -- public API
#   smk_malloc.h    -- upstream's allocator wrapper (alloc/free macros)

add_library(libsmacker STATIC smacker.c)
target_include_directories(libsmacker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# Upstream is C89 with many implicit casts and signed/unsigned mixes;
# silence its warnings so our own build log stays clean. The library
# itself is well-tested and stable.
target_compile_options(libsmacker PRIVATE -w)
