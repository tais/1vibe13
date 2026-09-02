cmake_minimum_required(VERSION 3.20)
project(ja2_engine_sdk_consumer LANGUAGES CXX)

find_package(JA2Engine 0.2 CONFIG REQUIRED COMPONENTS RuntimeAdapter)

if(NOT JA2Engine_RuntimeAdapter_FOUND OR NOT JA2Engine_EngineCore_FOUND OR
    NOT TARGET JA2::RuntimeAdapter OR NOT TARGET JA2::EngineCore)
  message(FATAL_ERROR "JA2Engine RuntimeAdapter component is incomplete")
endif()

function(assert_no_sdk_path_leak target_name)
  set(target_properties
    INTERFACE_COMPILE_DEFINITIONS
    INTERFACE_COMPILE_OPTIONS
    INTERFACE_INCLUDE_DIRECTORIES
    INTERFACE_LINK_DIRECTORIES
    INTERFACE_LINK_OPTIONS
    INTERFACE_SOURCES
    IMPORTED_LOCATION)
  get_target_property(imported_configurations ${target_name} IMPORTED_CONFIGURATIONS)
  foreach(configuration IN LISTS imported_configurations)
    list(APPEND target_properties "IMPORTED_LOCATION_${configuration}")
  endforeach()

  foreach(property_name IN LISTS target_properties)
    get_target_property(property_value ${target_name} ${property_name})
    if(NOT property_value OR property_value MATCHES "-NOTFOUND$")
      continue()
    endif()
    set(inspected_property_value "${property_value}")
    foreach(install_prefix IN ITEMS "${JA2_SDK_EXPECTED_INSTALL_PREFIX}")
      if(NOT install_prefix)
        continue()
      endif()
      file(TO_CMAKE_PATH "${install_prefix}" normalized_install_prefix)
      string(REPLACE "${install_prefix}" "<JA2_SDK_INSTALL_PREFIX>"
        inspected_property_value "${inspected_property_value}")
      string(REPLACE "${normalized_install_prefix}" "<JA2_SDK_INSTALL_PREFIX>"
        inspected_property_value "${inspected_property_value}")
    endforeach()
    foreach(forbidden_path IN ITEMS
        "${JA2_SDK_FORBIDDEN_SOURCE_DIR}" "${JA2_SDK_FORBIDDEN_BUILD_DIR}")
      if(NOT forbidden_path)
        continue()
      endif()
      file(TO_CMAKE_PATH "${forbidden_path}" normalized_forbidden_path)
      string(FIND "${inspected_property_value}" "${forbidden_path}" native_path_offset)
      string(FIND "${inspected_property_value}" "${normalized_forbidden_path}" cmake_path_offset)
      if(NOT native_path_offset EQUAL -1 OR NOT cmake_path_offset EQUAL -1)
        message(FATAL_ERROR
          "${target_name} ${property_name} leaks '${forbidden_path}': ${property_value}")
      endif()
    endforeach()
  endforeach()
endfunction()

assert_no_sdk_path_leak(JA2::EngineCore)
assert_no_sdk_path_leak(JA2::RuntimeAdapter)

add_executable(ja2_engine_sdk_consumer main.cpp)
# RuntimeAdapter is the only requested SDK target. Its public EngineCore
# dependency keeps the established Core surface available transitively.
target_link_libraries(ja2_engine_sdk_consumer PRIVATE JA2::RuntimeAdapter)
target_compile_features(ja2_engine_sdk_consumer PRIVATE cxx_std_17)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  set_property(TARGET ja2_engine_sdk_consumer PROPERTY
    MSVC_RUNTIME_LIBRARY "${JA2Engine_MSVC_RUNTIME_LIBRARY}")
endif()

# Let the parent SDK test execute the consumer through the generator instead
# of guessing single- versus multi-config output directories.
add_custom_target(run_ja2_engine_sdk_consumer
  COMMAND $<TARGET_FILE:ja2_engine_sdk_consumer>
  DEPENDS ja2_engine_sdk_consumer
  VERBATIM)
