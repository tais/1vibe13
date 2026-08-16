# Loopback and raw-wire tests link the production SDL3_net transport target.
add_executable(mp_sdl_net_transport_tests
  mp_sdl_net_transport_tests.cpp
)
target_link_libraries(mp_sdl_net_transport_tests PRIVATE SdlNetTransport)
add_test(NAME mp_sdl_net_transport COMMAND mp_sdl_net_transport_tests)

# Fixed-width co-op admission codecs and sender/epoch binding are independent
# from SDL, the game engine, and installed Data.
add_executable(coop_session_protocol_tests
  coop_session_protocol_tests.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopAdmission.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopSessionProtocol.cpp
)
target_include_directories(coop_session_protocol_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Multiplayer)
target_compile_features(coop_session_protocol_tests PRIVATE cxx_std_17)
add_test(NAME coop_session_protocol COMMAND coop_session_protocol_tests)

# The first authoritative co-op command boundary is deliberately independent
# from the game and transport implementations. It pins the exact intent bytes,
# sender-bound admission, command order, tactical context, and actor ACL before
# a future full-engine adapter is permitted to execute anything.
add_executable(coop_tactical_authority_tests
  coop_tactical_authority_tests.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopAdmission.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopSessionProtocol.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopTacticalAuthority.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopTacticalIntent.cpp
)
target_include_directories(coop_tactical_authority_tests PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/Multiplayer)
target_compile_features(coop_tactical_authority_tests PRIVATE cxx_std_17)
add_test(NAME coop_tactical_authority COMMAND coop_tactical_authority_tests)

# Compose admission, sender-derived intent authority, and an injected execution
# sink at the boundary intended for JA2 --dedicated. This remains data-free and
# does not register or reinterpret any legacy multiplayer RPC.
add_executable(full_engine_coop_ingress_tests
  full_engine_coop_ingress_tests.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopAdmission.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopSessionProtocol.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopTacticalAuthority.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/CoopTacticalIntent.cpp
  ${CMAKE_SOURCE_DIR}/Multiplayer/FullEngineCoopIngress.cpp
)
target_include_directories(full_engine_coop_ingress_tests PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/Multiplayer)
target_compile_features(full_engine_coop_ingress_tests PRIVATE cxx_std_17)
add_test(NAME full_engine_coop_ingress COMMAND full_engine_coop_ingress_tests)

# Drive the real standalone coordinator pump over loopback with two lightweight
# clients. The server translation unit is unchanged except for its test-only
# entry-point name; no game data or engine target is linked.
find_package(Threads REQUIRED)
add_library(mp_coordinator_server_for_tests OBJECT
  ${CMAKE_SOURCE_DIR}/Multiplayer/ja2server/ja2server.cpp)
target_compile_definitions(mp_coordinator_server_for_tests PRIVATE
  main=ja2server_test_main
  JA2SERVER_LOOPBACK_TEST=1)
target_include_directories(mp_coordinator_server_for_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/Multiplayer
  ${CMAKE_SOURCE_DIR}/Multiplayer/ja2server)
target_link_libraries(mp_coordinator_server_for_tests PRIVATE
  SdlNetTransport)

add_executable(mp_coordinator_session_tests
  mp_coordinator_session_tests.cpp
  $<TARGET_OBJECTS:mp_coordinator_server_for_tests>)
target_include_directories(mp_coordinator_session_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/Multiplayer
  ${CMAKE_SOURCE_DIR}/Multiplayer/ja2server)
target_link_libraries(mp_coordinator_session_tests PRIVATE
  SdlNetTransport Threads::Threads)
add_test(NAME mp_coordinator_session COMMAND mp_coordinator_session_tests)
set_tests_properties(mp_coordinator_session PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME mp_coordinator_coop_rejection
  COMMAND mp_coordinator_session_tests --coop-only)

# This executable deliberately links only the campaign/platform-independent
# target. It prevents accidental reliance on SDL, SGP, or JA2 globals from
# hiding behind the larger headless game harness.
add_executable(engine_core_tests engine_core_tests.cpp)
target_link_libraries(engine_core_tests PRIVATE JA2::EngineCore)
add_test(NAME engine_core COMMAND engine_core_tests)

# The legacy domain serializer still talks through HWFILE, but its byte schema
# is data-free. Compile the production serializer against a tiny in-memory
# FileMan surface and pin primitives plus representative production visitors to
# one canonical fixture on Windows, macOS, Linux, and the sanitizer job.
add_executable(save_serializer_golden_tests
  save_serializer_golden_tests.cpp
  ${CMAKE_SOURCE_DIR}/sgp/SaveSerializer.cpp)
target_include_directories(save_serializer_golden_tests PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/Ja2
  ${CMAKE_SOURCE_DIR}/Strategic
  ${CMAKE_SOURCE_DIR}/sgp)
add_test(NAME save_serializer_golden COMMAND save_serializer_golden_tests)

# Packed VK configuration and frame-owned legacy scheduling are dependency-free
# models. This target prevents either from regaining SDL, Win32, or game-global
# dependencies while exercising aliases, injected input, debt, and discontinuities.
add_executable(utils_runtime_control_model_tests
  utils_runtime_control_model_tests.cpp
  ${CMAKE_SOURCE_DIR}/Utils/KeyBindingModel.cpp
  ${CMAKE_SOURCE_DIR}/Utils/LegacyClockScheduler.cpp)
target_include_directories(utils_runtime_control_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_features(utils_runtime_control_model_tests PRIVATE cxx_std_17)
add_test(NAME utils_runtime_control_model
  COMMAND utils_runtime_control_model_tests)

# UTF decoding, bounded glyph geometry, deterministic alpha coverage, and
# legacy metric normalization stay independent from stb, SDL, VFS, and game
# globals. The production WinFont adapter consumes this same model.
add_executable(portable_font_model_tests
  portable_font_model_tests.cpp)
target_include_directories(portable_font_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_features(portable_font_model_tests PRIVATE cxx_std_17)
add_test(NAME portable_font_model COMMAND portable_font_model_tests)

# JA2's command vocabulary and replay schema are an adapter above Core. Keeping
# this as a separate executable makes both sides of that dependency testable.
add_executable(ja2_runtime_adapter_tests ja2_runtime_adapter_tests.cpp)
target_link_libraries(ja2_runtime_adapter_tests PRIVATE JA2::RuntimeAdapter)
add_test(NAME ja2_runtime_adapter COMMAND ja2_runtime_adapter_tests)

# Co-op join/reconnect requires a complete generation baseline before ordered
# tactical deltas can be applied. Keep the versioned baseline codec adapter-only
# and data-free so every host platform pins identical bytes and fail-closed
# decode behavior without installed game data.
add_executable(tactical_world_snapshot_codec_tests
  tactical_world_snapshot_codec_tests.cpp)
target_link_libraries(tactical_world_snapshot_codec_tests
  PRIVATE JA2::RuntimeAdapter)
add_test(NAME tactical_world_snapshot_codec
  COMMAND tactical_world_snapshot_codec_tests)

add_executable(tactical_interrupt_session_model_tests
  tactical_interrupt_session_model_tests.cpp
  ../Engine/Adapters/JA2/TacticalWorldSession.cpp)
target_include_directories(tactical_interrupt_session_model_tests PRIVATE ..)
add_test(NAME tactical_interrupt_session_model
  COMMAND tactical_interrupt_session_model_tests)

# The door action popup retains one exact actor/world/structure selection while
# UI callbacks are pending. Keep that lifecycle model adapter-only so raw JA2
# pointers, world storage, SDL, and presentation resources cannot leak into it.
add_executable(tactical_door_ui_session_tests
  tactical_door_ui_session_tests.cpp)
target_link_libraries(tactical_door_ui_session_tests PRIVATE JA2::RuntimeAdapter)
add_test(NAME tactical_door_ui_session COMMAND tactical_door_ui_session_tests)

add_executable(tactical_team_population_session_model_tests
  tactical_team_population_session_model_tests.cpp
  ../Engine/Adapters/JA2/TacticalWorldSession.cpp)
target_include_directories(tactical_team_population_session_model_tests
  PRIVATE ..)
add_test(NAME tactical_team_population_session_model
  COMMAND tactical_team_population_session_model_tests)

# Reload-all captures one bounded exact roster and its environment policy before
# touching legacy inventory state. This adapter-only target pins that value and
# replay codec without linking the game, SDL, or installed data.
add_executable(simulation_command_bulk_reload_model_tests
  simulation_command_bulk_reload_model_tests.cpp)
target_link_libraries(simulation_command_bulk_reload_model_tests
  PRIVATE JA2::RuntimeAdapter)
add_test(NAME simulation_command_bulk_reload_model
  COMMAND simulation_command_bulk_reload_model_tests)

# Equipment-driven mode/scope correction captures a complete value result and
# keeps presentation, continuation, and replication policy independent. This
# adapter-only target pins the structural contract, codec, and reference-model
# discard without linking the game, SDL, or installed data.
add_executable(simulation_command_weapon_configuration_model_tests
  simulation_command_weapon_configuration_model_tests.cpp)
target_link_libraries(simulation_command_weapon_configuration_model_tests
  PRIVATE JA2::RuntimeAdapter)
add_test(NAME simulation_command_weapon_configuration_model
  COMMAND simulation_command_weapon_configuration_model_tests)

# Retained AI and route traversal captures an exact actor/path precondition,
# while player intent remains a sentinel-only request. This adapter-only target
# pins the truth table, extended tag-11 codec, replay provenance, and explicit
# reference-model discard without linking the game or installed data.
add_executable(simulation_command_traversal_model_tests
  simulation_command_traversal_model_tests.cpp)
target_link_libraries(simulation_command_traversal_model_tests
  PRIVATE JA2::RuntimeAdapter)
add_test(NAME simulation_command_traversal_model
  COMMAND simulation_command_traversal_model_tests)

# Automatic AI, path, pending-action, and dialogue structure interactions use
# one retained value contract. This adapter-only target pins its truth table,
# tag-30 bytes, playback provenance, and explicit reference-model discard.
add_executable(simulation_command_world_object_model_tests
  simulation_command_world_object_model_tests.cpp)
target_link_libraries(simulation_command_world_object_model_tests
  PRIVATE JA2::RuntimeAdapter)
add_test(NAME simulation_command_world_object_model
  COMMAND simulation_command_world_object_model_tests)

# Multiplayer healing is a local-only authoritative vitals snapshot. This
# adapter-only target pins exact actor identity, the legacy signed-byte domain,
# appended tag-31 bytes, playback provenance, and reference-model behavior.
add_executable(simulation_command_actor_vitals_model_tests
  simulation_command_actor_vitals_model_tests.cpp)
target_link_libraries(simulation_command_actor_vitals_model_tests
  PRIVATE JA2::RuntimeAdapter)
add_test(NAME simulation_command_actor_vitals_model
  COMMAND simulation_command_actor_vitals_model_tests)

# The first typed dialogue effect coexists with the arbitrary legacy bitmask
# lane. This dependency-free model pins the tagged payload, full numeric
# domains, composite compatibility, and retained FIFO identity.
add_executable(dialogue_effect_payload_model_tests
  dialogue_effect_payload_model_tests.cpp)
target_include_directories(dialogue_effect_payload_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Tactical)
target_compile_features(dialogue_effect_payload_model_tests PRIVATE cxx_std_17)
add_test(NAME dialogue_effect_payload_model
  COMMAND dialogue_effect_payload_model_tests)

# Legacy map-temp action bytes overlap between campaign data sets. This tiny
# data-free test keeps their runtime codec independent from the full game host.
add_executable(campaign_map_change_code_tests
  campaign_map_change_code_tests.cpp)
target_include_directories(campaign_map_change_code_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_map_change_codes COMMAND campaign_map_change_code_tests)

# Application campaign choices are runtime policy above Engine/Core. This
# data-free target proves both built-in campaign paths without relying on the
# executable's JA2UB compile definition or legacy application globals.
add_executable(campaign_application_policy_tests
  campaign_application_policy_tests.cpp)
target_include_directories(campaign_application_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_application_policy COMMAND campaign_application_policy_tests)

# Tactical dialogue, animation, melee, and radar behavior consume the same live
# meanwhile capability. This data-free truth table also proves the UB path does
# not evaluate Arulco's legacy meanwhile-state probe.
add_executable(campaign_meanwhile_policy_tests
  campaign_meanwhile_policy_tests.cpp)
target_include_directories(campaign_meanwhile_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_meanwhile_policy COMMAND campaign_meanwhile_policy_tests)

# Door forcing and tunnel feedback are selected from the live campaign rather
# than from the executable's JA2UB compile definition.
add_executable(campaign_door_policy_tests
  campaign_door_policy_tests.cpp)
target_include_directories(campaign_door_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_door_policy COMMAND campaign_door_policy_tests)

# Power-generator, tunnel, fortified-door, and mine content is projected into
# typed sectors and grid sets. This data-free target pins both campaigns,
# distinct authored sentinels, and the live tunnel-enemy option decision.
add_executable(campaign_tactical_scenario_policy_tests
  campaign_tactical_scenario_policy_tests.cpp)
target_include_directories(campaign_tactical_scenario_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_tactical_scenario_policy
  COMMAND campaign_tactical_scenario_policy_tests)

# The contiguous UB strategic-sector script consumes one immutable typed
# snapshot. This data-free target pins the ordered quote/email/map/roof policy,
# including legacy coordinate aliases and short-circuit behavior.
add_executable(campaign_strategic_sector_script_policy_tests
  campaign_strategic_sector_script_policy_tests.cpp)
target_include_directories(campaign_strategic_sector_script_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_strategic_sector_script_policy
  COMMAND campaign_strategic_sector_script_policy_tests)

# The live JA25 scenario-origin byte is distinct from runtime campaign
# identity. This data-free target pins built-in, custom, and noncanonical save
# values plus the source-selection and short-circuit contract.
add_executable(campaign_strategic_ai_scenario_policy_tests
  campaign_strategic_ai_scenario_policy_tests.cpp)
target_include_directories(campaign_strategic_ai_scenario_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_strategic_ai_scenario_policy
  COMMAND campaign_strategic_ai_scenario_policy_tests)

# Strategic-event callback ownership is selected from the live campaign. This
# dependency-free target pins all fourteen routes plus the two live probes that
# must remain behind their campaign gates.
add_executable(campaign_strategic_event_policy_tests
  campaign_strategic_event_policy_tests.cpp)
target_include_directories(campaign_strategic_event_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_strategic_event_policy
  COMMAND campaign_strategic_event_policy_tests)

# First-battle loyalty, creature meanwhile state, Enrico mail, militia prompts,
# and Speck reactions remain in Strategic while campaign ownership crosses this
# dependency-free six-effect value boundary.
add_executable(campaign_strategic_content_policy_tests
  campaign_strategic_content_policy_tests.cpp)
target_include_directories(campaign_strategic_content_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_strategic_content_policy
  COMMAND campaign_strategic_content_policy_tests)

# Strategic-map guidance, custom-map processing, meanwhile scenes, and the UB
# strategic hooks are selected from the live campaign rather than JA2UB.
add_executable(campaign_map_screen_policy_tests
  campaign_map_screen_policy_tests.cpp)
target_include_directories(campaign_map_screen_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_map_screen_policy COMMAND campaign_map_screen_policy_tests)

# IMP activation passes and campaign text-resource fallback are selected from
# the live campaign rather than from the executable's JA2UB definition.
add_executable(campaign_imp_policy_tests
  campaign_imp_policy_tests.cpp)
target_include_directories(campaign_imp_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_imp_policy COMMAND campaign_imp_policy_tests)

# Campaign progress calculation and progress-triggered story events are
# selected from immutable runtime capabilities. This dependency-free target
# pins UB's signed legacy progress-key behavior and Arulco's threshold event.
add_executable(campaign_progress_policy_tests
  campaign_progress_policy_tests.cpp)
target_include_directories(campaign_progress_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_progress_policy COMMAND campaign_progress_policy_tests)

# Quest/fact availability, initial selection, completion rewards, laptop
# recovery, and POW support follow the live campaign. This dependency-free
# target also pins UB's leave-the-existing-fact-value-untouched behavior.
add_executable(campaign_quest_policy_tests
  campaign_quest_policy_tests.cpp)
target_include_directories(campaign_quest_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_quest_policy COMMAND campaign_quest_policy_tests)

# JA25 new-gun dialogue is selected from immutable runtime capabilities. This
# dependency-free target pins both call boundaries and their short-circuiting.
add_executable(campaign_gun_comment_policy_tests
  campaign_gun_comment_policy_tests.cpp)
target_include_directories(campaign_gun_comment_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_gun_comment_policy
  COMMAND campaign_gun_comment_policy_tests)

# NPC quote-file routing, old-save repair, Morris turn dialogue, and enemy
# surrender offers follow the live campaign rather than JA2UB.
add_executable(campaign_npc_policy_tests
  campaign_npc_policy_tests.cpp)
target_include_directories(campaign_npc_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_npc_policy COMMAND campaign_npc_policy_tests)

# Lua global names stay campaign-compatible while their initializer selects
# the live campaign rather than the JA2UB host definition.
add_executable(campaign_lua_global_policy_tests
  campaign_lua_global_policy_tests.cpp)
target_include_directories(campaign_lua_global_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_lua_global_policy
  COMMAND campaign_lua_global_policy_tests)

# LuaInitNPCs reads one live campaign policy per invocation and replays the
# exact legacy 40/2/28 callback groups only for Unfinished Business.
add_executable(campaign_lua_registration_policy_tests
  campaign_lua_registration_policy_tests.cpp)
target_include_directories(campaign_lua_registration_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_lua_registration_policy
  COMMAND campaign_lua_registration_policy_tests)

# Civilian quote catalogues, dedicated group records, surrender completion,
# and UB's unavailable-record sentinel follow the live campaign capability.
add_executable(campaign_civilian_quote_policy_tests
  campaign_civilian_quote_policy_tests.cpp)
target_include_directories(campaign_civilian_quote_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_civilian_quote_policy
  COMMAND campaign_civilian_quote_policy_tests)

# Dealer identities overlap in the legacy persisted slots. This data-free
# policy test verifies both complete rosters and their inverse mapping without
# selecting behavior through a host compile definition.
add_executable(campaign_dealer_policy_tests
  campaign_dealer_policy_tests.cpp)
target_include_directories(campaign_dealer_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_dealer_policy COMMAND campaign_dealer_policy_tests)

# Profile IDs and mercenary lifecycle behavior differ between campaigns while
# both implementations must remain available to every host.
add_executable(campaign_mercenary_policy_tests
  campaign_mercenary_policy_tests.cpp)
target_include_directories(campaign_mercenary_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_mercenary_policy COMMAND campaign_mercenary_policy_tests)

# Mercenary hiring and initial-sector setup consume one typed UB arrival
# snapshot per routed invocation. This dependency-free trace suite pins the two
# formerly eager argument probes, the complete content shape, and effect order.
add_executable(campaign_mercenary_arrival_content_tests
  campaign_mercenary_arrival_content_tests.cpp)
target_include_directories(campaign_mercenary_arrival_content_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_mercenary_arrival_content
  COMMAND campaign_mercenary_arrival_content_tests)

# Lua-authored arrival settings cross a typed, write-only application seam.
# This dependency-free target pins all seven slots plus Jerry's signed
# positive/negative/zero mutation contract without exposing GAME_UB_OPTIONS.
add_executable(campaign_mercenary_arrival_mutation_tests
  campaign_mercenary_arrival_mutation_tests.cpp)
target_include_directories(campaign_mercenary_arrival_mutation_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
target_compile_features(campaign_mercenary_arrival_mutation_tests
  PRIVATE cxx_std_17)
add_test(NAME campaign_mercenary_arrival_mutation
  COMMAND campaign_mercenary_arrival_mutation_tests)

# M.E.R.C. account, pricing, gear, availability, and Speck quote records are
# selected at runtime. This data-free target pins both complete policy paths
# without relying on the host executable's campaign compile definition.
add_executable(campaign_merc_site_policy_tests
  campaign_merc_site_policy_tests.cpp)
target_include_directories(campaign_merc_site_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_merc_site_policy COMMAND campaign_merc_site_policy_tests)

# A.I.M. links, salary presentation, contract pricing, and hiring controls are
# selected at runtime. This data-free target pins both complete policy paths.
add_executable(campaign_aim_site_policy_tests
  campaign_aim_site_policy_tests.cpp)
target_include_directories(campaign_aim_site_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME campaign_aim_site_policy COMMAND campaign_aim_site_policy_tests)

# Email catalogs, insurance/shipment notices, tactical death notices, and
# M.E.R.C. level-up IDs are selected at runtime. The same data-free target pins
# the boundary checks used by Laptop's legacy containers, dropdowns, and
# mail-order scrolling.
add_executable(laptop_communications_policy_tests
  laptop_communications_policy_tests.cpp)
target_include_directories(laptop_communications_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2 ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_communications_policy
  COMMAND laptop_communications_policy_tests)

# Insurance page navigation, purchase validation, coverage arithmetic, and
# payout counts must remain safe without the legacy UI or campaign globals.
add_executable(insurance_site_model_tests
  insurance_site_model_tests.cpp)
target_include_directories(insurance_site_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME insurance_site_model
  COMMAND insurance_site_model_tests)

# File-viewer briefings/artwork and history quest text are selected at runtime.
# This data-free target pins both campaigns and every optional-asset fallback,
# including paired quest start/completion record indices.
add_executable(laptop_campaign_content_policy_tests
  laptop_campaign_content_policy_tests.cpp)
target_include_directories(laptop_campaign_content_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Ja2)
add_test(NAME laptop_campaign_content_policy
  COMMAND laptop_campaign_content_policy_tests)

# Bobby Ray's configured cart size, fixed legacy save records, stock counters,
# and shipment-region arrays meet at old raw-storage boundaries. Keep the
# capacity/ownership model independently executable and data-free.
add_executable(bobby_ray_commerce_model_tests
  bobby_ray_commerce_model_tests.cpp)
target_include_directories(bobby_ray_commerce_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME bobby_ray_commerce_model
  COMMAND bobby_ray_commerce_model_tests)

# Catalogue page counts, navigation, visible item slots, and backing-data
# indexes must stay safe without loading the storefront UI or campaign data.
add_executable(bobby_ray_catalogue_model_tests
  bobby_ray_catalogue_model_tests.cpp)
target_include_directories(bobby_ray_catalogue_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME bobby_ray_catalogue_model
  COMMAND bobby_ray_catalogue_model_tests)

# Bobby Ray destination windows and shipment rows are sparse, mutable UI
# views. Keep empty, stale, exact-end, and visible-slot rules data-free.
add_executable(bobby_ray_fulfilment_model_tests
  bobby_ray_fulfilment_model_tests.cpp)
target_include_directories(bobby_ray_fulfilment_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME bobby_ray_fulfilment_model
  COMMAND bobby_ray_fulfilment_model_tests)

# Homepage signs, catalogue rows, order controls/drop-downs, and shipment rows
# must use identical geometry for drawing, input, scrolling, and invalidation.
add_executable(bobby_ray_layout_tests
  bobby_ray_layout_tests.cpp)
target_include_directories(bobby_ray_layout_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME bobby_ray_layout
  COMMAND bobby_ray_layout_tests)

# Finance, history, and email use persisted paginated record lists. Keep
# malformed layouts, stale saved pages, offsets, and exact-end views data-free.
add_executable(laptop_record_page_model_tests
  laptop_record_page_model_tests.cpp)
target_include_directories(laptop_record_page_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_record_page_model
  COMMAND laptop_record_page_model_tests)

# Email's mutable message/page lists, persisted indices, and localized buffers
# meet at a legacy raw-storage boundary. Keep empty, stale, exact-end,
# overflow, and truncation behavior independently executable and data-free.
add_executable(laptop_email_list_model_tests
  laptop_email_list_model_tests.cpp)
target_include_directories(laptop_email_list_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_email_list_model
  COMMAND laptop_email_list_model_tests)

# Personnel's live and departed rosters are sparse, mutable views backed by
# fixed legacy save arrays. Keep profile validation, duplicate repair,
# transactional category changes, page selection, and inventory windows
# independently executable and data-free.
add_executable(laptop_personnel_roster_model_tests
  laptop_personnel_roster_model_tests.cpp)
target_include_directories(laptop_personnel_roster_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_personnel_roster_model
  COMMAND laptop_personnel_roster_model_tests)

# Campaign History combines persisted incident tails with mutable summary,
# detail, and news navigation. Keep stale/empty page rules, report retention,
# saturating totals, exact transfers, picture bounds, direction composition,
# and bounded fixed-buffer copying independently executable and data-free.
add_executable(laptop_campaign_history_model_tests
  laptop_campaign_history_model_tests.cpp)
target_include_directories(laptop_campaign_history_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_campaign_history_model
  COMMAND laptop_campaign_history_model_tests)

# The final Laptop audit shares one dependency-free contract for the remaining
# mutable pages: signed/exact-end selections, stale pagination, bounded text,
# and exact binary transfers.
add_executable(laptop_ui_state_model_tests
  laptop_ui_state_model_tests.cpp)
target_include_directories(laptop_ui_state_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_ui_state_model
  COMMAND laptop_ui_state_model_tests)

# A.I.M.'s profile page has two fixed-pixel artwork variants. Keep drawing,
# mouse hitboxes, stat rows, equipment cells, and navigation coordinates on one
# independently executable geometry contract.
add_executable(aim_member_profile_layout_tests
  aim_member_profile_layout_tests.cpp)
target_include_directories(aim_member_profile_layout_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME aim_member_profile_layout
  COMMAND aim_member_profile_layout_tests)

# The remaining A.I.M. site geometry is shared by video rendering/control
# creation, facial-index drawing/hitboxes/navigation, and policy title
# variants. Keep those contracts executable without the legacy UI runtime.
add_executable(aim_website_layout_tests
  aim_website_layout_tests.cpp)
target_include_directories(aim_website_layout_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME aim_website_layout
  COMMAND aim_website_layout_tests)

# M.E.R.C.'s inventory artwork, tooltip regions, item placement, and loadout
# controls must consume the same dependency-free grid geometry.
add_executable(merc_files_layout_tests
  merc_files_layout_tests.cpp)
target_include_directories(merc_files_layout_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME merc_files_layout
  COMMAND merc_files_layout_tests)

# Shared legacy UI helpers expose signed handles, bounded callback mappings,
# and slider geometry to every application host. Keep exact-end, zero-extent,
# duplicate-ID, stale-ID, and full-capacity behavior independently executable.
add_executable(utils_ui_state_model_tests
  utils_ui_state_model_tests.cpp)
target_include_directories(utils_ui_state_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME utils_ui_state_model
  COMMAND utils_ui_state_model_tests)

# Cursor catalogue selection, animation, flashing, mouse levels, and chance-bar
# geometry must remain safe without loading SDL, SGP, tactical state, or assets.
add_executable(utils_cursor_state_model_tests
  utils_cursor_state_model_tests.cpp)
target_include_directories(utils_cursor_state_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME utils_cursor_state_model
  COMMAND utils_cursor_state_model_tests)

# Shared message, localization, and fixed-buffer text paths need the same
# negative/exact-end, capacity, serialization, and lazy-load rules in every
# host. Keep those rules independently executable and data-free.
add_executable(utils_text_state_model_tests
  utils_text_state_model_tests.cpp)
target_include_directories(utils_text_state_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME utils_text_state_model
  COMMAND utils_text_state_model_tests)

# INI/property/XML persistence shares strict numeric parsing, transactional
# publication, bounded destinations, escaping, and exact transfer rules.
# Keep malformed-input and zero-capacity behavior data-free.
add_executable(utils_data_boundary_model_tests
  utils_data_boundary_model_tests.cpp)
target_include_directories(utils_data_boundary_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME utils_data_boundary_model
  COMMAND utils_data_boundary_model_tests)

# Legacy filename joins and wide-text wrapping sit below rendering and FileMan.
# Keep their exact-capacity and no-space behavior independently testable.
add_executable(utils_legacy_utilities_model_tests
  utils_legacy_utilities_model_tests.cpp)
target_include_directories(utils_legacy_utilities_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME utils_legacy_utilities_model
  COMMAND utils_legacy_utilities_model_tests)

# Indexed localization XML overlays are sparse and may repeat records, but no
# record may reach the fixed legacy tables until every index and text payload
# has parsed successfully. Keep those rules executable without Expat or JA2.
add_executable(utils_indexed_xml_model_tests
  utils_indexed_xml_model_tests.cpp)
target_include_directories(utils_indexed_xml_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME utils_indexed_xml_model
  COMMAND utils_indexed_xml_model_tests)

# Shared cinematic, music, and sound adapters own fixed slots, opaque callback
# epochs, clipped frame geometry, PCM queue limits, and positional attenuation.
# Keep their reusable boundary rules independently executable and data-free.
add_executable(utils_media_lifecycle_model_tests
  utils_media_lifecycle_model_tests.cpp)
target_include_directories(utils_media_lifecycle_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME utils_media_lifecycle_model
  COMMAND utils_media_lifecycle_model_tests)

# The tactical Event Pump still exposes raw legacy packets, but its replacement
# queue semantics are independently testable: exact schemas, owned payloads,
# FIFO/delayed ordering, bounded failure, and complete teardown.
add_executable(tactical_event_queue_model_tests
  tactical_event_queue_model_tests.cpp)
target_include_directories(tactical_event_queue_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME tactical_event_queue_model
  COMMAND tactical_event_queue_model_tests)

# Offline image conversion has legacy signed dimensions and 32-bit on-disk
# lengths. Keep its checked geometry, staging, and palette rules executable
# without the editor, renderer, VFS, or filesystem.
add_executable(utils_image_utility_model_tests
  utils_image_utility_model_tests.cpp)
target_include_directories(utils_image_utility_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME utils_image_utility_model
  COMMAND utils_image_utility_model_tests)

# Language identity, resource paths, and layout policy are runtime data even
# while the legacy global text catalogs remain one-per-archive. Keep the full
# supported catalog and its persisted identifiers independently executable.
add_executable(i18n_language_catalog_tests
  i18n_language_catalog_tests.cpp)
add_test(NAME i18n_language_catalog
  COMMAND i18n_language_catalog_tests)

# The first seven immutable TextCatalog domains own all eight scalar labels,
# the indexed game-time and AIM Sort tables, and the 85-entry exporter-only
# cohort while a tiny compiled seam preserves today's g_lang-selected default.
# This target links no game, SDL, VFS, archive, or installed localization data.
add_executable(i18n_text_catalog_tests
  i18n_text_catalog_tests.cpp
  ${CMAKE_SOURCE_DIR}/i18n/TextCatalog.cpp
  ${CMAKE_SOURCE_DIR}/i18n/language.cpp)
target_compile_definitions(i18n_text_catalog_tests PRIVATE ENGLISH)
target_include_directories(i18n_text_catalog_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/i18n
  ${CMAKE_SOURCE_DIR}/i18n/include)
target_compile_features(i18n_text_catalog_tests PRIVATE cxx_std_17)
add_test(NAME i18n_text_catalog COMMAND i18n_text_catalog_tests)

# The selected-catalog adapter borrows all legacy storage shapes but the
# injected sink must copy immediately. Link the English catalog directly so
# empty suppression, absolute indices, TextPack agreement, and later snapshots
# of mutable globals stay executable without VFS or the game application.
add_executable(selected_catalog_export_tests
  selected_catalog_export_tests.cpp
  ${CMAKE_SOURCE_DIR}/i18n/SelectedCatalogExport.cpp
  ${CMAKE_SOURCE_DIR}/i18n/TextCatalog.cpp
  ${CMAKE_SOURCE_DIR}/i18n/language.cpp
  ${CMAKE_SOURCE_DIR}/i18n/_EnglishText.cpp)
target_compile_definitions(selected_catalog_export_tests PRIVATE ENGLISH)
target_include_directories(selected_catalog_export_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/i18n
  ${CMAKE_SOURCE_DIR}/i18n/include)
target_compile_features(selected_catalog_export_tests PRIVATE cxx_std_17)
add_test(NAME selected_catalog_export COMMAND selected_catalog_export_tests)

# Campaign and build-conditioned translations are language data selected by an
# explicit policy. This data-free model pins the key schema and all four policy
# quadrants without linking a legacy text catalog or game application.
add_executable(i18n_conditional_text_policy_tests
  i18n_conditional_text_policy_tests.cpp)
target_include_directories(i18n_conditional_text_policy_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/i18n/include)
add_test(NAME i18n_conditional_text_policy
  COMMAND i18n_conditional_text_policy_tests)

# Compile the narrow legacy publication seam in all four combinations. This
# also proves that its selected literals still initialize fixed CHAR16 rows.
function(add_i18n_compiled_conditional_text_test target expect_ub expect_beta)
  add_executable(${target} i18n_compiled_conditional_text_tests.cpp)
  target_include_directories(${target} PRIVATE
    ${CMAKE_SOURCE_DIR}/i18n
    ${CMAKE_SOURCE_DIR}/i18n/include)
  target_compile_definitions(${target} PRIVATE
    I18N_EXPECT_CAMPAIGN_UB=${expect_ub}
    I18N_EXPECT_BUILD_BETA=${expect_beta}
    ${ARGN})
  add_test(NAME ${target} COMMAND ${target})
endfunction()

add_i18n_compiled_conditional_text_test(
  i18n_compiled_conditional_text_ja2_release_tests 0 0)
add_i18n_compiled_conditional_text_test(
  i18n_compiled_conditional_text_ja2_beta_tests 0 1 JA2BETAVERSION)
add_i18n_compiled_conditional_text_test(
  i18n_compiled_conditional_text_ja2ub_release_tests 1 0 JA2UB)
add_i18n_compiled_conditional_text_test(
  i18n_compiled_conditional_text_ja2ub_beta_tests
  1 1 JA2UB JA2BETAVERSION)

# Items.xml combines a cleared base item table with persistent auxiliary
# arrays and optional, field-presence-aware localization overlays. Keep its
# index, narrowing, inheritance, rollback, and publish rules data-free before
# the legacy Expat callback is moved onto the transaction in later slices.
add_executable(item_data_staging_model_tests
  item_data_staging_model_tests.cpp)
target_include_directories(item_data_staging_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Utils)
add_test(NAME item_data_staging_model
  COMMAND item_data_staging_model_tests)

# Encyclopedia is disabled in the current product configuration, but it is
# still maintained source rather than dead text. Compile its complete path in
# every host so dormant bounds and ownership code cannot silently decay.
add_library(laptop_encyclopedia_compile_check OBJECT
  "${CMAKE_SOURCE_DIR}/Laptop/Encyclopedia_new.cpp"
  "${CMAKE_SOURCE_DIR}/Laptop/Encyclopedia_Data_new.cpp"
  "${CMAKE_SOURCE_DIR}/Laptop/BriefingRoom_Data.cpp")
target_compile_definitions(laptop_encyclopedia_compile_check
  PRIVATE ENCYCLOPEDIA_WORKS)

# XML callbacks receive text in arbitrary chunks. Keep accumulation, strict
# integer narrowing, explicit legacy-byte sentinels, index validation, and
# fixed-buffer copying data-free.
add_executable(laptop_localization_input_model_tests
  laptop_localization_input_model_tests.cpp)
target_include_directories(laptop_localization_input_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_localization_input_model
  COMMAND laptop_localization_input_model_tests)

# IMP page transitions, generated-profile indices, and XML-backed portrait and
# voice cycling share one dependency-free lifecycle contract.
add_executable(laptop_imp_creation_state_model_tests
  laptop_imp_creation_state_model_tests.cpp)
target_include_directories(laptop_imp_creation_state_model_tests
  PRIVATE ${CMAKE_SOURCE_DIR}/Laptop)
add_test(NAME laptop_imp_creation_state_model
  COMMAND laptop_imp_creation_state_model_tests)

# A dedicated process replaces global allocation hooks while measuring the
# observer hot path. Keeping it separate avoids coupling allocation accounting
# to unrelated adapter tests or their setup work.
add_executable(tactical_observer_allocation_tests
  tactical_observer_allocation_tests.cpp)
target_link_libraries(tactical_observer_allocation_tests PRIVATE JA2::RuntimeAdapter)
add_test(NAME tactical_observer_allocations COMMAND tactical_observer_allocation_tests)

# Exercise the production legacy tile-cache implementation with data-free
# surface/structure stubs. Keeping the real translation unit in this target
# makes cache-capacity and lifecycle regressions available to ASan without a
# complete JA2 data installation.
add_executable(tile_cache_tests
  tile_cache_tests.cpp
  "${CMAKE_SOURCE_DIR}/TileEngine/Tile Cache.cpp"
)
add_test(NAME tile_cache COMMAND tile_cache_tests)

# Compile the TacticalActor aggregate through its focused header only. This
# prevents the declaration from depending on the legacy Soldier Control facade.
add_executable(tactical_actor_header_tests tactical_actor_header_tests.cpp)
add_test(NAME tactical_actor_header COMMAND tactical_actor_header_tests)

# Compile legacy application API headers that expose actor references without
# importing the broad Soldier Control compatibility facade.
add_executable(tactical_actor_api_headers_tests
  tactical_actor_api_headers_tests.cpp)
add_test(NAME tactical_actor_api_headers
  COMMAND tactical_actor_api_headers_tests)

# Compile each remaining actor-facing application header in its own translation
# unit and prove that none imports the complete TacticalActor aggregate. Separate
# translation units prevent one legacy header's include order from masking a
# missing dependency in another.
set(tactical_actor_service_api_headers
  "Strategic/Assignments.h"
  "Strategic/Map Screen Interface.h"
  "Strategic/Merc Contract.h"
  "Strategic/Quest Debug System.h"
  "Strategic/Rebel Command.h"
  "Strategic/Strategic Town Loyalty.h"
  "Strategic/mapscreen.h"
  "Strategic/strategic.h"
  "Strategic/strategicmap.h"
  "Tactical/Animation Control.h"
  "Tactical/Animation Data.h"
  "Tactical/Bullets.h"
  "Tactical/Civ Quotes.h"
  "Tactical/Drugs And Alcohol.h"
  "Tactical/Food.h"
  "Tactical/Handle Items.h"
  "Tactical/Handle UI.h"
  "Tactical/Items.h"
  "Tactical/LOS.h"
  "Tactical/LogicalBodyTypes/Filter.h"
  "Tactical/Overhead.h"
  "Tactical/Soldier Add.h"
  "Tactical/Soldier Create.h"
  "Tactical/Soldier Profile.h"
  "Tactical/Vehicles.h"
  "Tactical/Weapons.h"
  "Tactical/faces.h"
  "Tactical/soldier profile type.h"
  "TacticalAI/ai.h"
  "Tactical/DynamicDialogue.h")
set(tactical_actor_service_api_header_sources)
foreach(tactical_actor_service_api_header IN LISTS
    tactical_actor_service_api_headers)
  string(MAKE_C_IDENTIFIER "${tactical_actor_service_api_header}"
    tactical_actor_service_api_header_id)
  set(tactical_actor_service_api_header_source
    "${CMAKE_CURRENT_BINARY_DIR}/tactical_actor_service_api_${tactical_actor_service_api_header_id}.cpp")
  file(GENERATE OUTPUT "${tactical_actor_service_api_header_source}" CONTENT
"#include \"${tactical_actor_service_api_header}\"
#include <type_traits>
template<class T, class = void> struct IsComplete : std::false_type {};
template<class T> struct IsComplete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};
static_assert(!IsComplete<TacticalActor>::value);
")
  list(APPEND tactical_actor_service_api_header_sources
    "${tactical_actor_service_api_header_source}")
endforeach()

# Compile each focused compatibility/schema contract by itself as well. These
# headers intentionally range from actor-facing declarations to value-only
# persistence constants, so this check is independent of the incomplete-actor
# assertion above.
set(tactical_actor_contract_headers
  "Strategic/Strategic Path Types.h"
  "Tactical/Grid Direction.h"
  "Tactical/Soldier Background Types.h"
  "Tactical/Soldier Drug Types.h"
  "Tactical/Soldier Palette.h"
  "Tactical/Soldier Patrol Types.h"
  "Tactical/Soldier Profile Constants.h"
  "Tactical/Soldier Stat Types.h"
  "Tactical/Taunt Types.h"
  "Tactical/Render Palette Effects.h"
  "Tactical/TacticalActorAnimationGeometry.h"
  "Tactical/TacticalActorAnimationSelection.h"
  "Tactical/TacticalActorAnimationState.h"
  "Tactical/TacticalActorAnimationTiming.h"
  "Tactical/TacticalActorBattleSounds.h"
  "Tactical/TacticalActorBleeding.h"
  "Tactical/TacticalActorBloodState.h"
  "Tactical/TacticalActorConsumables.h"
  "Tactical/TacticalActorCrowBehavior.h"
  "Tactical/TacticalActorDamageFeedback.h"
  "Tactical/TacticalActorDamageResolution.h"
  "Tactical/TacticalActorDebug.h"
  "Tactical/TacticalActorEmploymentTypes.h"
  "Tactical/TacticalActorEvents.h"
  "Tactical/TacticalActorInterrupts.h"
  "Tactical/TacticalActorLocomotion.h"
  "Tactical/TacticalActorLongActions.h"
  "Tactical/TacticalActorMovementState.h"
  "Tactical/TacticalActorMovementAudio.h"
  "Tactical/TacticalActorPendingActionTypes.h"
  "Tactical/TacticalActorPredicates.h"
  "Tactical/TacticalActorQuoteFlags.h"
  "Tactical/TacticalActorSkills.h"
  "Tactical/TacticalActorStateFlags.h"
  "Tactical/TacticalDestinationTypes.h")
set(tactical_actor_contract_header_sources)
foreach(tactical_actor_contract_header IN LISTS tactical_actor_contract_headers)
  string(MAKE_C_IDENTIFIER "${tactical_actor_contract_header}"
    tactical_actor_contract_header_id)
  set(tactical_actor_contract_header_source
    "${CMAKE_CURRENT_BINARY_DIR}/tactical_actor_contract_${tactical_actor_contract_header_id}.cpp")
  file(GENERATE OUTPUT "${tactical_actor_contract_header_source}" CONTENT
"#include \"${tactical_actor_contract_header}\"
")
  list(APPEND tactical_actor_contract_header_sources
    "${tactical_actor_contract_header_source}")
endforeach()
add_executable(tactical_actor_service_api_headers_tests
  tactical_actor_service_api_headers_tests.cpp
  ${tactical_actor_service_api_header_sources}
  ${tactical_actor_contract_header_sources})
add_test(NAME tactical_actor_service_api_headers
  COMMAND tactical_actor_service_api_headers_tests)

# Install the SDK, then configure, compile, and run a genuinely separate CMake
# project against find_package(JA2Engine). This catches missing public headers,
# non-exported symbols, and accidental source-tree include dependencies.
string(REPLACE ";" "|" engine_sdk_osx_architectures
  "${CMAKE_OSX_ARCHITECTURES}")
string(REPLACE ";" "|" engine_sdk_cxx_compiler
  "${CMAKE_CXX_COMPILER}")
add_test(
  NAME engine_sdk_consumer
  COMMAND ${CMAKE_COMMAND}
    "-DMAIN_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "-DMAIN_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DSDK_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/sdk_consumer"
    "-DSDK_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/sdk-consumer-test/$<CONFIG>"
    "-DSDK_INSTALL_LIBDIR=${CMAKE_INSTALL_LIBDIR}"
    "-DSDK_INSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}"
    "-DGENERATOR=${CMAKE_GENERATOR}"
    "-DGENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DGENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
    "-DGENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
    "-DCXX_COMPILER_ENCODED=${engine_sdk_cxx_compiler}"
    "-DCXX_COMPILER_TARGET=${CMAKE_CXX_COMPILER_TARGET}"
    "-DTOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
    "-DOSX_ARCHITECTURES_ENCODED=${engine_sdk_osx_architectures}"
    "-DOSX_SYSROOT=${CMAKE_OSX_SYSROOT}"
    "-DOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}"
    "-DCONFIGURATION=$<CONFIG>"
    "-DSYSTEM_NAME=${CMAKE_SYSTEM_NAME}"
    "-DSANITIZER_ENABLED=${ADDRESS_SANITIZER}"
    -P ${CMAKE_SOURCE_DIR}/cmake/TestEngineSdk.cmake)

# Fast source-level ratchet for the productized SDK surface. The deeper
# engine_sdk_consumer test below the install boundary remains authoritative for
# compile/link/runtime behavior; this catches missing release assets, examples,
# compatibility metadata, and an accidentally broad pre-1.0 version rule.
add_test(
  NAME engine_sdk_product_contract
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
    -DBINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}/sdk-product-contract/$<CONFIG>
    -P ${CMAKE_SOURCE_DIR}/cmake/CheckEngineSdkProduct.cmake)

add_test(
  NAME architecture_boundaries
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
    -P ${CMAKE_SOURCE_DIR}/cmake/CheckArchitectureBoundaries.cmake)

# Keep the explicitly inventoried legacy campaign-preprocessor tail from
# growing between larger runtime-selection migrations. The dedicated lint
# workflow always runs this; expose it through CTest as well when the local
# build environment has Python.
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  # Build-free shape, exact conditioned-value, linked GameStrings adapter,
  # ordered output-byte snapshots, source-derived limits, and exhaustive
  # raw-range/pointer-entry gates for all compiled catalogs. These remain
  # mandatory even while the legacy foreign executable matrix is allowed to
  # fail, and give developers one named no-build target.
  add_custom_target(i18n_text_schema_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/check_i18n_text_schema.py
    COMMAND ${Python3_EXECUTABLE} -m unittest
      tools/test_check_i18n_text_schema.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/check_i18n_conditional_text.py
    COMMAND ${Python3_EXECUTABLE} -m unittest
      tools/test_check_i18n_conditional_text.py
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/check_i18n_export_schema.py
    COMMAND ${Python3_EXECUTABLE} -m unittest
      tools/test_check_i18n_export_schema.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Validating compiled text ABI, conditioned values, and exporter schema")
  add_test(
    NAME i18n_text_schema
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/check_i18n_text_schema.py)
  add_test(
    NAME i18n_text_schema_tool
    COMMAND ${Python3_EXECUTABLE} -m unittest
      tools/test_check_i18n_text_schema.py)
  set_tests_properties(i18n_text_schema_tool PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME i18n_conditional_text_schema
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/check_i18n_conditional_text.py)
  add_test(
    NAME i18n_conditional_text_schema_tool
    COMMAND ${Python3_EXECUTABLE} -m unittest
      tools/test_check_i18n_conditional_text.py)
  set_tests_properties(i18n_conditional_text_schema_tool PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME i18n_export_schema
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/check_i18n_export_schema.py)
  add_test(
    NAME i18n_export_schema_tool
    COMMAND ${Python3_EXECUTABLE} -m unittest
      tools/test_check_i18n_export_schema.py)
  set_tests_properties(i18n_export_schema_tool PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(
    NAME campaign_compile_guards
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/lint_campaign_compile_guards.py)
  add_test(
    NAME world_tile_access
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}/tools/lint_world_tile_access.py)
endif()

# Standalone multiplayer coordinator/relay server (no game engine).
# Uses the project-native SDL3_net TCP transport. Protocol
# admission and coordinator session behavior are covered above by the real-pump
# loopback test; a complete in-engine playthrough remains experimental. Built
# only when explicitly requested.
option(BUILD_JA2SERVER "Build the standalone ja2server MP coordinator" OFF)
if(BUILD_JA2SERVER)
  add_executable(ja2server
    ${CMAKE_SOURCE_DIR}/Multiplayer/ja2server/ja2server.cpp
  )
  target_link_libraries(ja2server PRIVATE SdlNetTransport)
  if(APPLE)
    # The linker's ad-hoc signature on a bare arm64 executable can intermittently
    # fail dyld's page validation at launch (SIGKILL "Code Signature Invalid",
    # crash in dyld before main). Re-sign cleanly after every build to avoid it.
    add_custom_command(TARGET ja2server POST_BUILD
      COMMAND codesign --force --sign - $<TARGET_FILE:ja2server>
      VERBATIM)
  endif()
endif()
