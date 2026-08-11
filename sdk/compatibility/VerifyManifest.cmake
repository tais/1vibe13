cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED JA2_ENGINE_SDK_MANIFEST OR
    NOT EXISTS "${JA2_ENGINE_SDK_MANIFEST}")
  message(FATAL_ERROR
    "Pass an installed JA2EngineCompatibility.json as JA2_ENGINE_SDK_MANIFEST")
endif()

file(READ "${JA2_ENGINE_SDK_MANIFEST}" manifest_contents)

string(JSON manifest_root_type TYPE "${manifest_contents}")
string(JSON manifest_schema_type TYPE "${manifest_contents}" schemaVersion)
string(JSON manifest_sdk_type TYPE "${manifest_contents}" sdk)
string(JSON manifest_name_type TYPE "${manifest_contents}" sdk name)
string(JSON manifest_version_type TYPE "${manifest_contents}" sdk version)
string(JSON manifest_line_type TYPE "${manifest_contents}" sdk compatibilityLine)
string(JSON manifest_stability_type TYPE "${manifest_contents}" sdk stability)
string(JSON manifest_cxx_standard_type TYPE "${manifest_contents}" sdk cxxStandard)
string(JSON manifest_compatibility_type TYPE "${manifest_contents}" compatibility)
string(JSON manifest_source_policy_type TYPE
  "${manifest_contents}" compatibility source)
string(JSON manifest_binary_policy_type TYPE
  "${manifest_contents}" compatibility binary)
string(JSON manifest_components_type TYPE "${manifest_contents}" components)
if(NOT manifest_root_type STREQUAL "OBJECT" OR
    NOT manifest_schema_type STREQUAL "NUMBER" OR
    NOT manifest_sdk_type STREQUAL "OBJECT" OR
    NOT manifest_name_type STREQUAL "STRING" OR
    NOT manifest_version_type STREQUAL "STRING" OR
    NOT manifest_line_type STREQUAL "STRING" OR
    NOT manifest_stability_type STREQUAL "STRING" OR
    NOT manifest_cxx_standard_type STREQUAL "NUMBER" OR
    NOT manifest_compatibility_type STREQUAL "OBJECT" OR
    NOT manifest_source_policy_type STREQUAL "STRING" OR
    NOT manifest_binary_policy_type STREQUAL "STRING" OR
    NOT manifest_components_type STREQUAL "ARRAY")
  message(FATAL_ERROR
    "JA2Engine compatibility manifest has an unsupported schema")
endif()

string(JSON manifest_schema GET "${manifest_contents}" schemaVersion)
string(JSON manifest_name GET "${manifest_contents}" sdk name)
string(JSON manifest_version GET "${manifest_contents}" sdk version)
string(JSON manifest_line GET "${manifest_contents}" sdk compatibilityLine)
string(JSON manifest_stability GET "${manifest_contents}" sdk stability)
string(JSON manifest_cxx_standard GET "${manifest_contents}" sdk cxxStandard)
string(JSON manifest_source_policy GET
  "${manifest_contents}" compatibility source)
string(JSON manifest_binary_policy GET
  "${manifest_contents}" compatibility binary)
string(REGEX REPLACE "^([0-9]+[.][0-9]+)[.][0-9]+$" "\\1"
  manifest_version_line "${manifest_version}")

if(NOT manifest_schema EQUAL 1 OR
    NOT manifest_name STREQUAL "JA2Engine" OR
    NOT manifest_version MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$" OR
    NOT manifest_line MATCHES "^[0-9]+[.][0-9]+$" OR
    NOT manifest_version_line STREQUAL manifest_line OR
    NOT manifest_stability STREQUAL "experimental" OR
    NOT manifest_cxx_standard EQUAL 17 OR
    NOT manifest_source_policy STREQUAL "same-0.x-minor-line" OR
    NOT manifest_binary_policy STREQUAL
      "same-toolchain-and-build-configuration-only")
  message(FATAL_ERROR
    "JA2Engine compatibility manifest has an unsupported identity or policy")
endif()

string(JSON component_count LENGTH "${manifest_contents}" components)
if(NOT component_count EQUAL 2)
  message(FATAL_ERROR "JA2Engine compatibility manifest must name two components")
endif()

set(expected_component_names EngineCore RuntimeAdapter)
set(expected_component_targets JA2::EngineCore JA2::RuntimeAdapter)
foreach(component_index RANGE 0 1)
  list(GET expected_component_names ${component_index} expected_name)
  list(GET expected_component_targets ${component_index} expected_target)
  string(JSON component_name GET
    "${manifest_contents}" components ${component_index} name)
  string(JSON component_target GET
    "${manifest_contents}" components ${component_index} target)
  string(JSON component_type TYPE
    "${manifest_contents}" components ${component_index})
  string(JSON component_name_type TYPE
    "${manifest_contents}" components ${component_index} name)
  string(JSON component_target_type TYPE
    "${manifest_contents}" components ${component_index} target)
  if(NOT component_type STREQUAL "OBJECT" OR
      NOT component_name_type STREQUAL "STRING" OR
      NOT component_target_type STREQUAL "STRING" OR
      NOT component_name STREQUAL expected_name OR
      NOT component_target STREQUAL expected_target)
    message(FATAL_ERROR
      "JA2Engine compatibility manifest component ${component_index} changed")
  endif()
endforeach()

string(JSON runtime_adapter_requirement_type TYPE
  "${manifest_contents}" components 1 requires)
string(JSON runtime_adapter_requirement GET
  "${manifest_contents}" components 1 requires)
if(NOT runtime_adapter_requirement_type STREQUAL "STRING" OR
    NOT runtime_adapter_requirement STREQUAL "EngineCore")
  message(FATAL_ERROR
    "JA2Engine RuntimeAdapter must require EngineCore")
endif()

if(DEFINED JA2_ENGINE_INSTALLED_VERSION AND
    NOT JA2_ENGINE_INSTALLED_VERSION STREQUAL manifest_version)
  message(FATAL_ERROR
    "Installed JA2Engine version ${JA2_ENGINE_INSTALLED_VERSION} does not match manifest ${manifest_version}")
endif()
if(DEFINED JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE AND
    NOT JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE STREQUAL manifest_line)
  message(FATAL_ERROR
    "JA2Engine ${manifest_line} does not satisfy required line ${JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE}")
endif()

message(STATUS
  "JA2Engine ${manifest_version} compatibility manifest is valid (${manifest_stability})")
