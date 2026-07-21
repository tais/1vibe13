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
  cstddef
  cstdint
  deque
  initializer_list
  iterator
  optional
  string
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

message(STATUS "Engine/Core boundary verified (${core_files})")
