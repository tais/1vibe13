# STATIC: see ext/zlib/CMakeLists.txt -- avoids the Windows DLL
# export/import problem (libpng.dll couldn't resolve zlib's symbols)
# and keeps these vendored helpers folded into the executable.
add_library(libpng STATIC
"png.c"
"pngerror.c"
"pnggccrd.c"
"pngget.c"
"pngmem.c"
"pngpread.c"
"pngread.c"
"pngrio.c"
"pngrtran.c"
"pngrutil.c"
"pngset.c"
"pngtrans.c"
"pngwio.c"
"pngwrite.c"
"pngwtran.c"
"pngwutil.c"
)
target_include_directories(libpng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libpng PUBLIC zlib)
