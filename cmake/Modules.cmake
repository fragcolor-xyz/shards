option(SHARDS_WITH_EVERYTHING "Enables all modules, disabling this will only build common modules and exclude experimental ones" ON)

function(is_module_enabled OUTPUT_VARIABLE MODULE_ID)
  if(${SHARDS_WITH_EVERYTHING})
    set(${OUTPUT_VARIABLE} TRUE PARENT_SCOPE)
  else()
    string(TOUPPER "${MODULE_ID}" MODULE_ID_UPPER)
    set(${OUTPUT_VARIABLE} ${SHARDS_WITH_${MODULE_ID_UPPER}} PARENT_SCOPE)
  endif()
endfunction()

function(add_shards_module MODULE_NAME)
  set(OPTS
    EXPERIMENTAL # This disables the module by default when not building with SHARDS_WITH_EVERYTHING=ON
  )
  set(ARGS)
  set(MULTI_ARGS
    SOURCES # The source files to add to this module
    INLINE_SHARDS # The names of inline shard id's to register
    INLINE_SOURCES # The source files to inline during full optimization builds
    REGISTER_SHARDS)
  cmake_parse_arguments(MODULE "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})

  message(STATUS "add_shards_module(${MODULE_NAME})")
  message(VERBOSE "  SOURCES = ${MODULE_SOURCES}")

  set(MODULE_TARGET "shards-module-${MODULE_NAME}")
  set(MODULE_ID "${MODULE_NAME}")

  if(NOT MODULE_SOURCES)
    set(DUMMY_SOURCE_PATH ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp)
    file(WRITE ${DUMMY_SOURCE_PATH} "// Intentionally empty\n")
    set(MODULE_SOURCES ${DUMMY_SOURCE_PATH})
  endif()

  add_library(${MODULE_TARGET} STATIC ${MODULE_SOURCES})

  # Generate registration function
  set_property(GLOBAL APPEND PROPERTY SHARDS_MODULE_TARGETS ${MODULE_TARGET})
  set_property(TARGET ${MODULE_TARGET} PROPERTY SHARDS_MODULE_ID ${MODULE_ID})
  
  set(MODULE_ENABLED TRUE)
  if(${MODULE_EXPERIMENTAL})
    set(MODULE_ENABLED FALSE)
  endif()
    
  string(TOUPPER "${MODULE_ID}" MODULE_ID_UPPER)
  set("SHARDS_WITH_${MODULE_ID_UPPER}" ${MODULE_ENABLED} CACHE BOOL "Enables the ${MODULE_ID} module")

  is_module_enabled(MODULE_ENABLED ${MODULE_ID})
  if(NOT ${MODULE_ENABLED})
    message(STATUS "  ENABLED = ${MODULE_ENABLED}")
  endif()

  target_compile_definitions(${MODULE_TARGET} PRIVATE SHARDS_THIS_MODULE_ID=${MODULE_ID})
  target_link_libraries(${MODULE_TARGET} shards-core)

  if(MODULE_INLINE_SHARDS)
    message(STATUS "  INLINE_SHARDS = ${MODULE_INLINE_SHARDS}")

    foreach(INLINE_SHARD ${MODULE_INLINE_SHARDS})
      set_property(TARGET ${MODULE_TARGET} APPEND PROPERTY SHARDS_MODULE_INLINE_SHARDS ${INLINE_SHARD})
    endforeach()
  endif()

  if(MODULE_INLINE_SOURCES)
    message(STATUS "  INLINE_SOURCES = ${MODULE_INLINE_SOURCES}")

    foreach(INLINE_SOURCE ${MODULE_INLINE_SOURCES})
      get_filename_component(ABS_SOURCE "${INLINE_SOURCE}" ABSOLUTE)
      set_property(TARGET ${MODULE_TARGET} APPEND PROPERTY SHARDS_MODULE_INLINE_SOURCES ${ABS_SOURCE})
    endforeach()

    # If not inlined, inlude these source files as separate units
    if(NOT SHARDS_INLINE_EVERYTHING)
      target_sources(${MODULE_TARGET} PRIVATE ${MODULE_INLINE_SOURCES})
    endif()
  endif()

  if(MODULE_INLINE_SHARDS OR MODULE_INLINE_SOURCES)
    set_property(TARGET ${MODULE_TARGET} APPEND PROPERTY SHARDS_MODULE_INLINE_MODULES ${MODULE_ID})
  endif()

  if(MODULE_REGISTER_SHARDS)
    message(STATUS "  REGISTER_SHARDS = ${MODULE_REGISTER_SHARDS}")

    foreach(REGISTER_FN ${MODULE_REGISTER_SHARDS})
      set_property(TARGET ${MODULE_TARGET} APPEND PROPERTY SHARDS_MODULE_REGISTER_FNS "${MODULE_ID}_${REGISTER_FN}")
      # message(STATUS "FN        " ${REGISTER_FN})
    endforeach()
  endif()
endfunction()

# function(add_shards_rust_module MODULE_NAME RUST_PROJECT_FOLDER)
  # add_rust_library(PROJECT_PATH RUST_PROJECT_FOLDER ${ARGN})
# endfunction()