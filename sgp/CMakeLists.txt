set(sgpCommonSrc
"${CMAKE_CURRENT_SOURCE_DIR}/Button Sound Control.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Button System.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Compression.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Cursor Control.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/DEBUG.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/debug_util.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/debug_win_util.cpp"
# DirectDraw Calls.cpp / DirectX Common.cpp dropped: SDL3 replaces
# DirectDraw on every platform.
"${CMAKE_CURRENT_SOURCE_DIR}/English.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/FileMan.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Font.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/himage.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/impTGA.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/input.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/LibraryDataBase.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/line.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/mousesystem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/PCX.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/PngLoader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Random.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/render_palette_registry.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/SaveSerializer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/sdl_input.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/sdl_video.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/sdl_vsurface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/sgp_logger.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/sgp_portable_globals.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/sgp_portable_stubs.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/shading.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/soundman.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/STCI.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/stringicmp.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/timer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject_blitters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject_depth_queries.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject_mask_blitters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject_multiz_blitters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject_native_image.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject_native_pixel_blitters.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/vobject_native_pixel_cache.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/WinFont.cpp"
)

# MemMan's emitted Release object changes with the application definitions.
# Keep it in each campaign archive even though the current Debug objects happen
# to match. A source may enter the common list only when every supported
# application produces the same object in both Release and Debug/ASan builds.
set(sgpVariantSrc
"${CMAKE_CURRENT_SOURCE_DIR}/MemMan.cpp"
)

set(sgpSrc ${sgpCommonSrc} ${sgpVariantSrc})
set(sgpCommonSrc ${sgpCommonSrc} PARENT_SCOPE)
set(sgpVariantSrc ${sgpVariantSrc} PARENT_SCOPE)
set(sgpSrc ${sgpSrc} PARENT_SCOPE)
