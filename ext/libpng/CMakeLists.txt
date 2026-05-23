# STATIC: see ext/zlib/CMakeLists.txt -- avoids the Windows DLL
# export/import problem (libpng.dll couldn't resolve zlib's symbols)
# and keeps these vendored helpers folded into the executable.
add_library(libpng STATIC
"png.c"
"pngerror.c"
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

# Disable libpng's SIMD optimization paths (NEON/SSE/MSA/VSX/LSX). They live in
# per-arch subdirs (arm/, intel/, ...) that we don't vendor/compile; without
# this, pngpriv.h auto-enables NEON on arm64 and the core ends up referencing
# png_init_filter_functions / png_do_expand_palette from those missing sources.
# Pure-C decode is plenty fast for JA2's images and stays portable across arches.
target_compile_definitions(libpng PRIVATE
  PNG_ARM_NEON_OPT=0
  PNG_INTEL_SSE_OPT=0
  PNG_MIPS_MSA_OPT=0
  PNG_POWERPC_VSX_OPT=0
  PNG_LOONGARCH_LSX_OPT=0
)
