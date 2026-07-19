if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(GLOB_RECURSE core_files
  "${SOURCE_ROOT}/Engine/Core/*.h"
  "${SOURCE_ROOT}/Engine/Core/*.hpp"
  "${SOURCE_ROOT}/Engine/Core/*.cpp")

foreach(core_file IN LISTS core_files)
  file(READ "${core_file}" contents)
  # Core may depend on the standard library, expressed with angle brackets,
  # but never on a project-local header. This keeps dependency direction
  # mechanically enforceable as the layer grows.
  if(contents MATCHES "#[ \t]*include[ \t]*\"")
    message(FATAL_ERROR "Engine/Core has a project-header dependency: ${core_file}")
  endif()
endforeach()

message(STATUS "Engine/Core boundary verified (${core_files})")
