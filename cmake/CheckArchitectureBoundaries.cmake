if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(GLOB_RECURSE core_files
  "${SOURCE_ROOT}/Engine/Core/*.h"
  "${SOURCE_ROOT}/Engine/Core/*.hpp"
  "${SOURCE_ROOT}/Engine/Core/*.cpp")

# Keep this explicit. A permissive "angle brackets mean standard library"
# rule also admits SDL3, platform SDKs, and upper engine/game layers.
set(core_standard_headers
  algorithm
  array
  atomic
  charconv
  chrono
  cmath
  cstddef
  cstdint
  cstdio
  cstring
  deque
  exception
  functional
  initializer_list
  iterator
  limits
  mutex
  new
  optional
  string
  string_view
  stdexcept
  system_error
  type_traits
  unordered_map
  unordered_set
  utility
  variant
  vector)

foreach(core_file IN LISTS core_files)
  file(READ "${core_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/Core/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    if(standard_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Core has a forbidden dependency '${header}' in ${core_file}")
    endif()
  endforeach()
endforeach()

file(GLOB_RECURSE legacy_adapter_files
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.hpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.cpp")

# This is the only engine layer allowed to bridge into the existing runtime.
# Keep the compatibility surface named and deliberately small so migration of
# a service cannot quietly pull unrelated game systems into Engine.
set(legacy_compatibility_headers
  FileMan.h
  expat.h
  soundman.h
  video.h
  vsurface.h
  SDL3/SDL_log.h)

foreach(adapter_file IN LISTS legacy_adapter_files)
  file(READ "${adapter_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/(Core|Adapters/Legacy)/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    list(FIND legacy_compatibility_headers "${header}" compatibility_header_index)
    if(standard_header_index EQUAL -1 AND compatibility_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Adapters/Legacy has a forbidden dependency '${header}' in ${adapter_file}")
    endif()
  endforeach()
endforeach()

# Keep the low-level SDL video surface narrower than the Legacy adapter layer:
# presentation and invalidation implementations may consume it, gateways may
# not. This makes a new raw frame-output dependency an explicit architecture
# change instead of an incidental include.
set(platform_video_adapter_consumers
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformDepthBufferBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoObjectBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoSurfaceBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformFramePresenter.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformFrameInvalidator.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformRenderCommands.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.cpp")
foreach(adapter_file IN LISTS legacy_adapter_files)
  list(FIND platform_video_adapter_consumers
    "${adapter_file}" platform_video_consumer_index)
  if(NOT platform_video_consumer_index EQUAL -1)
    continue()
  endif()
  file(READ "${adapter_file}" contents)
  string(REGEX MATCH
    "Platform(DepthBuffer|Video(Surface|Object)?)Backend\\.h|(^|[^A-Za-z0-9_])Platform(DepthBuffer(Describe|Map|Unmap)|Video(Present|Invalidate|MarkFrameChanged|Surface|ObjectDraw|ObjectOutline)[A-Za-z0-9_]*)[ \t\r\n]*\\("
    direct_platform_video_access "${contents}")
  if(direct_platform_video_access)
    message(FATAL_ERROR
      "Legacy gateway bypasses the engine frame contracts in ${adapter_file}; keep raw SDL access in the platform frame adapters")
  endif()
endforeach()

file(GLOB_RECURSE ja2_adapter_files
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.h"
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.hpp"
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.cpp")

foreach(adapter_file IN LISTS ja2_adapter_files)
  file(READ "${adapter_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/(Core|Adapters/JA2)/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    if(standard_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Adapters/JA2 has a forbidden dependency '${header}' in ${adapter_file}")
    endif()
  endforeach()
endforeach()

# Application package layers reach process-lifetime work through separate host
# ports, not one another's C++ interface. Campaigns may require rules through
# their manifest; a source include would recreate compile-time identity behind
# the runtime-selected package graph.
set(rules_package_files
  "${SOURCE_ROOT}/Ja2/RulesPackage.h"
  "${SOURCE_ROOT}/Ja2/RulesPackage.cpp")
foreach(package_file IN LISTS rules_package_files)
  file(READ "${package_file}" contents)
  string(REGEX MATCH
    "#[ \t]*include[ \t]*[<\"](CampaignPackage|CampaignRuntimeBootstrap)\\.h[>\"]"
    rules_campaign_dependency "${contents}")
  if(rules_campaign_dependency)
    message(FATAL_ERROR
      "The 1.13 rules package depends on campaign C++ identity in ${package_file}; use only RulesContentBootstrapHost")
  endif()
endforeach()

set(campaign_package_files
  "${SOURCE_ROOT}/Ja2/CampaignPackage.h"
  "${SOURCE_ROOT}/Ja2/CampaignPackage.cpp")
foreach(package_file IN LISTS campaign_package_files)
  file(READ "${package_file}" contents)
  string(REGEX MATCH
    "#[ \t]*include[ \t]*[<\"](RulesPackage|RulesContentBootstrap)\\.h[>\"]"
    campaign_rules_dependency "${contents}")
  if(campaign_rules_dependency)
    message(FATAL_ERROR
      "A campaign package depends on 1.13 rules C++ identity in ${package_file}; use only CampaignRuntimeBootstrapHost")
  endif()
endforeach()

foreach(retired_gameplay_runtime_file IN ITEMS
  "${SOURCE_ROOT}/Ja2/LegacyGameplayRuntime.h"
  "${SOURCE_ROOT}/Ja2/LegacyGameplayRuntime.cpp")
  if(EXISTS "${retired_gameplay_runtime_file}")
    message(FATAL_ERROR
      "Retired shared package runtime returned at ${retired_gameplay_runtime_file}; keep rules and campaign ownership separate")
  endif()
endforeach()

# Production callers must use the structured TryDispatch... result.  The
# sequence-returning wrappers remain part of the compatibility surface, but a
# zero sequence cannot distinguish rejection from a valid first command.
file(GLOB tactical_cpp_files "${SOURCE_ROOT}/Tactical/*.cpp")
set(legacy_tactical_dispatch_wrappers
  DispatchEndTurnCommandNow
  DispatchChangeStanceCommandNow
  DispatchBeginFireWeaponCommandNow
  DispatchMoveToGridCommandNow)

foreach(tactical_file IN LISTS tactical_cpp_files)
  if(tactical_file STREQUAL "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp")
    continue()
  endif()

  file(READ "${tactical_file}" contents)
  foreach(wrapper IN LISTS legacy_tactical_dispatch_wrappers)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${wrapper}[ \t\r\n]*\\("
      legacy_dispatch_call "${contents}")
    if(legacy_dispatch_call)
      message(FATAL_ERROR
        "Production code uses ambiguous compatibility wrapper '${wrapper}' in ${tactical_file}; use Try${wrapper} and inspect its structured result")
    endif()
  endforeach()
endforeach()

# Tactical world identity is owned by EngineRuntime's TacticalWorldSession.
# Exact legacy globals remain readable compatibility mirrors, but a second
# production writer would silently split world and turn identity again.
set(world_state_source_directories
  Editor
  Ja2
  Laptop
  ModularizedTacticalAI
  Multiplayer
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
  sgp)
set(world_state_files)
foreach(source_directory IN LISTS world_state_source_directories)
  file(GLOB_RECURSE source_files
    "${SOURCE_ROOT}/${source_directory}/*.cpp")
  list(APPEND world_state_files ${source_files})
endforeach()

set(world_state_owner "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp")
set(world_state_mirrors
  gWorldSectorX
  gWorldSectorY
  gbWorldSectorZ
  gfWorldLoaded)
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${world_state_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS world_state_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${mirror}[ \t\r\n]*(\\+\\+|--|[+*/%-]?=[^=])|(^|[^A-Za-z0-9_])(\\+\\+|--)[ \t\r\n]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_write "${contents}")
    if(world_state_write)
      message(FATAL_ERROR
        "Production code writes tactical-world compatibility mirror '${mirror}' in ${source_file}; route the transition through TacticalWorldAdapter")
    endif()
    string(REGEX MATCH
      "(^|[^&])&[ \t\r\n]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_address_escape "${contents}")
    if(world_state_address_escape)
      message(FATAL_ERROR
        "Production code passes tactical-world compatibility mirror '${mirror}' by address in ${source_file}; stage a local value and route writes through TacticalWorldAdapter")
    endif()
    string(REGEX MATCH
      "GetSectorFromFileName[ \t\r\n]*\\([^\\)]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_reference_escape "${contents}")
    if(world_state_reference_escape)
      message(FATAL_ERROR
        "Editor filename parsing passes tactical-world compatibility mirror '${mirror}' by mutable reference in ${source_file}; parse locals and route writes through TacticalWorldAdapter")
    endif()
  endforeach()
endforeach()

# World-load generation has no legacy storage requirement. It is read directly
# from TacticalWorldSession and must not return as a duplicate scalar owner.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])guiWorldLoadGeneration([^A-Za-z0-9_]|$)"
    retired_world_generation_mirror "${contents}")
  if(retired_world_generation_mirror)
    message(FATAL_ERROR
      "Retired world-generation mirror returned in ${source_file}; read CaptureJa2TacticalWorld().worldGeneration")
  endif()
endforeach()

# Loaded-world turn identity is owned by TacticalWorldSession. The old
# gTacticalStatus fields remain readable, but mode/team writers must publish
# through TacticalWorldAdapter so package snapshots cannot observe split state.
set(tactical_turn_owner "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${tactical_turn_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*ubCurrentTeam[ \t\r\n]*(\\+\\+|--|[+*/%&|^-]?=[^=])|(^|[^A-Za-z0-9_])(\\+\\+|--)[ \t\r\n]*gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*ubCurrentTeam([^A-Za-z0-9_]|$)"
    tactical_team_write "${contents}")
  if(tactical_team_write)
    message(FATAL_ERROR
      "Production code writes the tactical current-team mirror in ${source_file}; route the transition through TacticalWorldAdapter")
  endif()
  string(REGEX MATCH
    "(^|[^&])&[ \t\r\n]*gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*ubCurrentTeam([^A-Za-z0-9_]|$)"
    tactical_team_address_escape "${contents}")
  if(tactical_team_address_escape)
    message(FATAL_ERROR
      "Production code passes the tactical current-team mirror by address in ${source_file}; stage a value and route writes through TacticalWorldAdapter")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*uiFlags[ \t\r\n]*(=[^=]|[|&^]=[^;\r\n]*(TURNBASED|INCOMBAT))"
    tactical_mode_write "${contents}")
  if(tactical_mode_write)
    message(FATAL_ERROR
      "Production code writes the tactical turn/combat-mode mirror in ${source_file}; route the transition through TacticalWorldAdapter")
  endif()
endforeach()

# Campaign time identity is owned solely by EngineRuntime's
# CampaignClockSession. These former writable scalars have been retired; the
# established game reads value accessors backed by the session instead.
set(retired_campaign_clock_mirrors
  guiGameClock
  guiPreviousGameClock
  guiDay
  guiHour
  guiMin)
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS retired_campaign_clock_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_\"])${mirror}([^A-Za-z0-9_]|$)"
      retired_campaign_clock_use "${contents}")
    if(retired_campaign_clock_use)
      message(FATAL_ERROR
        "Retired campaign-clock scalar '${mirror}' returned in ${source_file}; read CampaignClockSession through CaptureJa2CampaignClock or GetWorld*")
    endif()
  endforeach()
endforeach()

# Campaign pacing is part of the fixed-step runtime. The strategic clock may
# retain presentation compatibility, but it must not quietly return to
# sampling a platform timer from the render loop.
set(campaign_clock_source "${SOURCE_ROOT}/Strategic/Game Clock.cpp")
file(READ "${campaign_clock_source}" campaign_clock_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])GetJA2Clock[ \t\r\n]*\\("
  campaign_clock_wall_time_sampling "${campaign_clock_contents}")
if(campaign_clock_wall_time_sampling)
  message(FATAL_ERROR
    "Strategic campaign time samples GetJA2Clock in ${campaign_clock_source}; pace it through CampaignClockScheduler")
endif()

set(game_loop_source "${SOURCE_ROOT}/Ja2/gameloop.cpp")
file(READ "${game_loop_source}" game_loop_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])UpdateClock[ \t\r\n]*\\("
  render_paced_campaign_clock "${game_loop_contents}")
if(render_paced_campaign_clock)
  message(FATAL_ERROR
    "The render loop calls legacy UpdateClock in ${game_loop_source}; campaign time belongs on the fixed-step sink")
endif()

# Strategic-event nodes and ordering are owned by EngineRuntime's
# CampaignEventQueue. The former gpEventList mirror has been retired; callers
# must query the live runtime-owned head when they begin a traversal.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])gpEventList([^A-Za-z0-9_]|$)"
    retired_campaign_event_head "${contents}")
  if(retired_campaign_event_head)
    message(FATAL_ERROR
      "Retired strategic-event head mirror returned in ${source_file}; begin traversal with GetStrategicEventListHead")
  endif()
endforeach()

# TacticalEntityDirectory owns the incarnation sequence directly. The former
# exported counter was unused outside the host and must not return as a second
# authority.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])guiCurrentUniqueSoldierId([^A-Za-z0-9_]|$)"
    retired_entity_sequence_mirror "${contents}")
  if(retired_entity_sequence_mirror)
    message(FATAL_ERROR
      "Retired tactical-entity incarnation mirror returned in ${source_file}; use TacticalEntityHost sequence gateways")
  endif()
endforeach()

# Whole SOLDIERTYPE record relocation changes which incarnation occupies a
# legacy pool slot. Keep those rare mutations in the entity host so the
# runtime directory is rebuilt atomically with the compatibility pool.
set(entity_pool_owner "${SOURCE_ROOT}/Ja2/TacticalEntityHost.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${entity_pool_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "Menptr[ \t\r\n]*\\[[^\\]]+\\][ \t\r\n]*=[^=]"
    entity_pool_record_write "${contents}")
  if(entity_pool_record_write)
    message(FATAL_ERROR
      "Production code relocates a complete tactical-entity pool record in ${source_file}; route the swap through TacticalEntityHost")
  endif()
endforeach()

# Expat parser allocation and release are an engine-adapter responsibility.
# Production loaders may borrow a parser during setup and keep their existing
# callbacks, but direct ownership would bypass bounded AssetSource reads,
# structured diagnostics, and the common lifetime guard.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "XML_(ExternalEntity)?ParserCreate[ \t\r\n]*\\("
    direct_xml_parser_create "${contents}")
  if(direct_xml_parser_create)
    message(FATAL_ERROR
      "Production code directly creates an Expat parser in ${source_file}; use LegacyXmlDocument")
  endif()
endforeach()

# Raw mixer handles belong to the platform adapter. The sound manager provides
# that backend, but every production caller must use the stable Sound* gateway
# so engine-owned and legacy playback cannot alias each other's identifiers.
set(platform_audio_backend_owner "${SOURCE_ROOT}/sgp/soundman.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${platform_audio_backend_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])PlatformSound[A-Za-z0-9_]*[ \t\r\n]*\\("
    direct_platform_audio_call "${contents}")
  if(direct_platform_audio_call)
    message(FATAL_ERROR
      "Production code bypasses the engine audio gateway in ${source_file}; use the public Sound* compatibility API")
  endif()
endforeach()

# Raw SDL frame output belongs to the platform adapters. The video manager
# implements that backend, while normal FrameDriver work and established
# RefreshScreen/PresentNow/Invalidate*/ColorFillVideoSurfaceArea callers cross
# engine-owned contracts.
set(platform_video_backend_owners
  "${SOURCE_ROOT}/sgp/sdl_video.cpp"
  "${SOURCE_ROOT}/sgp/sdl_vsurface.cpp"
  "${SOURCE_ROOT}/sgp/vobject.cpp"
  "${SOURCE_ROOT}/sgp/vobject_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_depth_queries.cpp"
  "${SOURCE_ROOT}/sgp/vobject_mask_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_multiz_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_image.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_cache.cpp")
set(multiz_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_multiz_blitters.cpp")
set(depth_query_owner
  "${SOURCE_ROOT}/sgp/vobject_depth_queries.cpp")
set(mask_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_mask_blitters.cpp")
set(native_pixel_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_blitters.cpp")
set(native_pixel_cache_owner
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_cache.cpp")
set(native_image_import_owner
  "${SOURCE_ROOT}/sgp/vobject_native_image.cpp")
file(READ "${SOURCE_ROOT}/sgp/vobject.cpp" video_object_owner_contents)
string(REGEX MATCH
  "gVideoObjectHandles[ \\t\\r\\n]*\\.[ \\t\\r\\n]*find[ \\t\\r\\n]*\\("
  manager_only_render_identity "${video_object_owner_contents}")
if(manager_only_render_identity)
  message(FATAL_ERROR
    "Video-object drawing derives render identity from legacy manager membership; every CreateVideoObject lifetime must use its stable render-image identity")
endif()
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ColorFillVideoSurfaceArea[ \t\r\n]*\\("
    direct_surface_fill_implementation "${contents}")
  if(direct_surface_fill_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy surface-fill entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+BltVideoSurface[ \t\r\n]*\\("
    direct_surface_copy_implementation "${contents}")
  if(direct_surface_copy_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy numeric surface-copy entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+BltStretchVideoSurface[ \t\r\n]*\\("
    direct_surface_stretch_implementation "${contents}")
  if(direct_surface_stretch_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy numeric surface-stretch entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ShadowVideoSurfaceRect[ \t\r\n]*\\("
    direct_surface_shade_implementation "${contents}")
  if(direct_surface_shade_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy surface-shade entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ShadowVideoSurfaceRectUsingLowPercentTable[ \t\r\n]*\\("
    direct_surface_low_shade_implementation "${contents}")
  if(direct_surface_low_shade_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy low-intensity surface-shade entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])memset[ \t\r\n]*\\([ \t\r\n]*gpZBuffer"
    direct_depth_buffer_clear "${contents}")
  if(direct_depth_buffer_clear)
    message(FATAL_ERROR
      "Production code clears gpZBuffer directly in ${source_file}; use RenderDepthFillCommand through the renderer gateway")
  endif()
  if(NOT "${source_file}" STREQUAL "${SOURCE_ROOT}/sgp/vobject.cpp")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+BltVideoObject[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_image_draw_implementation "${contents}")
    if(direct_image_draw_implementation)
      message(FATAL_ERROR
        "Production code implements a legacy video-object draw entry point in ${source_file}; keep image command translation in the video-object compatibility owner")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${multiz_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+Blt8BPPDataTo16BPPBufferTransZ(Inc|TransShadowInc)[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_multiz_blitter_implementation "${contents}")
    if(direct_multiz_blitter_implementation)
      message(FATAL_ERROR
        "Production code implements a raw multi-Z blitter in ${source_file}; keep strip-depth rasterization in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${mask_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+Zero8BPPDataTo16BPPBufferTransparent[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_mask_clear_implementation "${contents}")
    if(direct_mask_clear_implementation)
      message(FATAL_ERROR
        "Production code implements a raw image-mask clear in ${source_file}; keep footprint clearing in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${depth_query_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(Blt8BPPDataTo16BPPBufferTransInvZ|Query8BPPDataToDepthBufferOcclusion|IsTileRedundent)[ \t\r\n]*\\("
      direct_depth_query_implementation "${contents}")
    if(direct_depth_query_implementation)
      message(FATAL_ERROR
        "Production code implements a raw inverse-depth draw or visibility query in ${source_file}; keep depth reads in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_pixel_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(BltNativePixelDataToBufferTransparentClip|Blt16BPPDataTo16BPPBufferTransparentClip|BltNativePixelImageToBufferClip)[ \t\r\n]*\\("
      direct_native_pixel_blitter_implementation "${contents}")
    if(direct_native_pixel_blitter_implementation)
      message(FATAL_ERROR
        "Production code implements native-pixel rasterization in ${source_file}; keep it in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_pixel_cache_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(CacheVObjectRegionNativePixels|FindCachedVObjectNativePixelRegion|ConvertVObjectRegionTo16BPP|CheckFor16BPPRegion)[ \t\r\n]*\\("
      direct_native_pixel_cache_implementation "${contents}")
    if(direct_native_pixel_cache_implementation)
      message(FATAL_ERROR
        "Production code implements native-pixel sprite-cache ownership in ${source_file}; keep it in the dedicated SGP cache backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_image_import_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+ImportNativeVideoObjectImage[ \t\r\n]*\\("
      direct_native_image_import_implementation "${contents}")
    if(direct_native_image_import_implementation)
      message(FATAL_ERROR
        "Production code implements native true-colour image import in ${source_file}; keep byte-order and RGB565 conversion at the dedicated HIMAGE boundary")
    endif()
  endif()

  list(FIND platform_video_backend_owners
    "${source_file}" platform_video_backend_owner_index)
  if(NOT platform_video_backend_owner_index EQUAL -1)
    continue()
  endif()
  string(REGEX MATCH
    "Platform(DepthBuffer|Video(Surface|Object)?)Backend\\.h|(^|[^A-Za-z0-9_])Platform(DepthBuffer(Describe|Map|Unmap)|Video(Present|Invalidate|MarkFrameChanged|Surface|Object(DepthDraw|DepthVisibility|Draw|Outline))[A-Za-z0-9_]*)[ \t\r\n]*\\("
    direct_platform_video_access "${contents}")
  if(direct_platform_video_access)
    message(FATAL_ERROR
      "Production code bypasses an engine renderer gateway in ${source_file}; use FramePresenter, FrameInvalidator, RenderSurfaceAccess, RenderCommandSink, or the public compatibility API")
  endif()
endforeach()

message(STATUS
  "Engine boundaries verified (Core: ${core_files}; Legacy adapter: ${legacy_adapter_files}; JA2 adapter: ${ja2_adapter_files})")
