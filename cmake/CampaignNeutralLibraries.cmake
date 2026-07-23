include_guard(GLOBAL)

# Validate a legacy library's explicit campaign-neutral/campaign-sensitive
# source partition before any targets consume it. Keeping both lists explicit
# makes every new translation unit choose a side; accidental duplication,
# missing files, and silent empty partitions fail during configuration.
function(ja2_validate_campaign_source_partition library)
  set(common_variable "${library}CommonSrc")
  set(variant_variable "${library}VariantSrc")

  if(NOT DEFINED ${common_variable})
    message(FATAL_ERROR
      "${library}: missing campaign-neutral source list ${common_variable}")
  endif()
  if(NOT DEFINED ${variant_variable})
    message(FATAL_ERROR
      "${library}: missing campaign-sensitive source list ${variant_variable}")
  endif()

  set(common_sources ${${common_variable}})
  set(variant_sources ${${variant_variable}})
  if(NOT common_sources)
    message(FATAL_ERROR "${library}: campaign-neutral source list is empty")
  endif()
  if(NOT variant_sources)
    message(FATAL_ERROR "${library}: campaign-sensitive source list is empty")
  endif()

  set(all_sources ${common_sources} ${variant_sources})
  set(unique_sources ${all_sources})
  list(LENGTH all_sources source_count)
  list(REMOVE_DUPLICATES unique_sources)
  list(LENGTH unique_sources unique_source_count)
  if(NOT source_count EQUAL unique_source_count)
    message(FATAL_ERROR
      "${library}: source partition contains a duplicate or overlapping entry")
  endif()

  foreach(source IN LISTS all_sources)
    if(NOT IS_ABSOLUTE "${source}")
      message(FATAL_ERROR
        "${library}: source partition entry must be absolute: ${source}")
    endif()
    if(NOT EXISTS "${source}")
      message(FATAL_ERROR
        "${library}: source partition entry does not exist: ${source}")
    endif()
  endforeach()

  # Obvious application identity checks belong in the variant partition. This
  # catches the common maintenance error immediately; the stronger admission
  # rule remains byte comparison across every application and build mode
  # because identity-dependent inline/header code is not visible in this file.
  foreach(source IN LISTS common_sources)
    file(STRINGS "${source}" application_conditionals
      REGEX "^[ \t]*#[ \t]*(if|ifdef|ifndef|elif).*(JA2UBMAPS|JA2UB|JA2EDITOR)")
    if(application_conditionals)
      message(FATAL_ERROR
        "${library}: campaign-neutral source tests an application identity "
        "macro and must move to ${variant_variable}: ${source}")
    endif()
  endforeach()

  list(LENGTH common_sources common_count)
  list(LENGTH variant_sources variant_count)
  if(variant_count EQUAL 1)
    set(variant_noun "source")
  else()
    set(variant_noun "sources")
  endif()
  message(STATUS
    "${library}: sharing ${common_count} campaign-neutral sources; "
    "compiling ${variant_count} ${variant_noun} per application")
endfunction()
