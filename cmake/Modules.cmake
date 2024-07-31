option(SHARDS_WITH_EVERYTHING "Enables all modules, disabling this will only build common modules and exclude experimental ones" ON)
option(SHARDS_NO_RUST_UNION "Disables rust union build" OFF)

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
    REGISTER_SHARDS
    RUST_TARGETS # The rust crate targets that are required for this module
  )
  cmake_parse_arguments(MODULE "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})

  message(STATUS "add_shards_module(${MODULE_NAME})")
  message(VERBOSE "  SOURCES = ${MODULE_SOURCES}")

  set(MODULE_TARGET "shards-module-${MODULE_NAME}")
  set(MODULE_ID "${MODULE_NAME}")

  if(NOT MODULE_SOURCES)
    set(DUMMY_SOURCE_PATH ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp)

    if(NOT EXISTS ${DUMMY_SOURCE_PATH})
      file(WRITE ${DUMMY_SOURCE_PATH} "// Intentionally empty\n")
    endif()

    set(MODULE_SOURCES ${DUMMY_SOURCE_PATH})
  endif()

  add_library(${MODULE_TARGET} STATIC ${MODULE_SOURCES})

  # Generate registration function
  set_property(GLOBAL APPEND PROPERTY SHARDS_MODULE_TARGETS ${MODULE_TARGET})
  set_property(TARGET ${MODULE_TARGET} PROPERTY SHARDS_MODULE_ID ${MODULE_ID})

  # Store rust target
  set_property(TARGET ${MODULE_TARGET} PROPERTY SHARDS_RUST_TARGETS ${MODULE_RUST_TARGETS})

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
    message(VERBOSE "  INLINE_SHARDS = ${MODULE_INLINE_SHARDS}")

    foreach(INLINE_SHARD ${MODULE_INLINE_SHARDS})
      set_property(TARGET ${MODULE_TARGET} APPEND PROPERTY SHARDS_MODULE_INLINE_SHARDS ${INLINE_SHARD})
    endforeach()
  endif()

  if(MODULE_INLINE_SOURCES)
    message(VERBOSE "  INLINE_SOURCES = ${MODULE_INLINE_SOURCES}")

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
    message(VERBOSE "  REGISTER_SHARDS = ${MODULE_REGISTER_SHARDS}")

    foreach(REGISTER_FN ${MODULE_REGISTER_SHARDS})
      set_property(TARGET ${MODULE_TARGET} APPEND PROPERTY SHARDS_MODULE_REGISTER_FNS "${MODULE_ID}_${REGISTER_FN}")

      # message(STATUS "FN        " ${REGISTER_FN})
    endforeach()
  endif()
endfunction()

# function(shards_add_global_rust_target TARGET_NAME)
# set_property(GLOBAL APPEND PROPERTY SHARDS_GLOBAL_RUST_TARGETS ${TARGET_NAME})
# endfunction()
function(shards_generate_rust_union TARGET_NAME)
  message(STATUS "Generating rust union target ${TARGET_NAME}")

  set(OPTS)
  set(ARGS)
  set(MULTI_ARGS
    RUST_TARGETS # Extra dependencies to add
    ENABLED_MODULES # When set, can use to specify which modules to include
  )
  cmake_parse_arguments(UNION_EXTRA "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})

  get_property(SHARDS_MODULE_TARGETS GLOBAL PROPERTY SHARDS_MODULE_TARGETS)

  foreach(TARGET_NAME ${SHARDS_MODULE_TARGETS})
    get_property(MODULE_ID TARGET ${TARGET_NAME} PROPERTY SHARDS_MODULE_ID)

    if(UNION_EXTRA_ENABLED_MODULES)
      if(NOT "${MODULE_ID}" IN_LIST UNION_EXTRA_ENABLED_MODULES)
        message(DEBUG "Skipping module ${MODULE_ID}")
        continue()
      else()
        message(DEBUG "Including module ${MODULE_ID}")
      endif()
    else()
      is_module_enabled(MODULE_ENABLED ${MODULE_ID})

      if(NOT ${MODULE_ENABLED})
        continue()
      endif()
    endif()

    get_property(MODULE_RUST_TARGETS TARGET ${TARGET_NAME} PROPERTY SHARDS_RUST_TARGETS)

    if(MODULE_RUST_TARGETS)
      message(DEBUG "${TARGET_NAME}: Adding rust targets {${MODULE_RUST_TARGETS}} from module ${MODULE_ID}")
      list(APPEND RUST_TARGETS ${MODULE_RUST_TARGETS})
    endif()
  endforeach()

  list(APPEND RUST_TARGETS ${UNION_EXTRA_RUST_TARGETS})

  # Disable union
  if(SHARDS_NO_RUST_UNION)
    add_library(${TARGET_NAME} INTERFACE)

    foreach(RUST_TARGET ${RUST_TARGETS})
      message(STATUS "RUST TARGET: ${RUST_TARGET}")
      target_link_libraries(${TARGET_NAME} INTERFACE ${RUST_TARGET})
    endforeach()
  else()
    set(GEN_RUST_PATH ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME})
    set(GEN_RUST_SRC_PATH ${GEN_RUST_PATH}/src)
    set(CARGO_TOML ${GEN_RUST_PATH}/Cargo.toml)
    set(LIB_RS_TMP ${GEN_RUST_SRC_PATH}/lib.rs.tmp)
    set(LIB_RS ${GEN_RUST_SRC_PATH}/lib.rs)

    file(MAKE_DIRECTORY ${GEN_RUST_SRC_PATH})
    file(WRITE ${LIB_RS_TMP} "// This file is generated by CMake\n\n")

    # Add all the rust targets to the Cargo.toml
    foreach(RUST_TARGET ${RUST_TARGETS})
      get_property(RUST_PROJECT_PATH TARGET ${RUST_TARGET} PROPERTY RUST_PROJECT_PATH)
      get_property(RUST_NAME TARGET ${RUST_TARGET} PROPERTY RUST_NAME)

      get_property(RUST_FEATURES TARGET ${RUST_TARGET} PROPERTY RUST_FEATURES)
      unset(RUST_FEATURES_STRING)
      unset(RUST_FEATURES_STRING1)

      if(RUST_FEATURES)
        unset(RUST_FEATURES_QUOTED)

        foreach(FEATURE ${RUST_FEATURES})
          list(APPEND RUST_FEATURES_QUOTED \"${FEATURE}\")
        endforeach()

        list(JOIN RUST_FEATURES_QUOTED "," RUST_FEATURES_STRING)
        set(RUST_FEATURES_STRING1 ", features = [${RUST_FEATURES_STRING}]")
      endif()

      message(VERBOSE "${TARGET_NAME}: Adding rust target ${RUST_TARGET} (path: ${RUST_PROJECT_PATH}, name: ${RUST_NAME}, features: ${RUST_FEATURES_STRING})")

      file(RELATIVE_PATH RELATIVE_RUST_PROJECT_PATH ${GEN_RUST_PATH} ${RUST_PROJECT_PATH})
      string(APPEND CARGO_CRATE_DEPS "${RUST_NAME} = { path = \"${RELATIVE_RUST_PROJECT_PATH}\"${RUST_FEATURES_STRING1} }\n")

      string(REPLACE "-" "_" RUST_NAME_ID ${RUST_NAME})
      file(APPEND ${LIB_RS_TMP} "pub use ${RUST_NAME_ID};\n")

      get_property(TARGET_INTERFACE_LINK_LIBRARIES TARGET ${RUST_TARGET} PROPERTY INTERFACE_LINK_LIBRARIES)
      list(APPEND COMBINED_INTERFACE_LINK_LIBRARIES ${TARGET_INTERFACE_LINK_LIBRARIES})

      get_property(RUST_DEPENDS TARGET ${RUST_TARGET} PROPERTY RUST_DEPENDS)
      list(APPEND COMBINED_RUST_DEPENDS ${RUST_DEPENDS})

      get_property(RUST_ENVIRONMENT TARGET ${RUST_TARGET} PROPERTY RUST_ENVIRONMENT)
      list(APPEND COMBINED_RUST_ENVIRONMENT ${RUST_ENVIRONMENT})
    endforeach()

    # The generated create name
    set(CARGO_CRATE_NAME ${TARGET_NAME})

    file(COPY_FILE ${LIB_RS_TMP} ${LIB_RS} ONLY_IF_DIFFERENT)
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Cargo.toml.in" ${CARGO_TOML})

    # Add the rust library
    add_rust_library(NAME ${TARGET_NAME}
      PROJECT_PATH ${GEN_RUST_PATH}
      DEPENDS ${COMBINED_RUST_DEPENDS}
      ENVIRONMENT ${COMBINED_RUST_ENVIRONMENT})
    add_library(${TARGET_NAME} ALIAS ${TARGET_NAME}-rust)

    list(REMOVE_DUPLICATES COMBINED_INTERFACE_LINK_LIBRARIES)

    if(COMBINED_INTERFACE_LINK_LIBRARIES)
      set_property(TARGET ${TARGET_NAME}-rust APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${COMBINED_INTERFACE_LINK_LIBRARIES})
      message(VERBOSE "${TARGET_NAME}: Linking against ${COMBINED_INTERFACE_LINK_LIBRARIES}")
    endif()
  endif()
endfunction()

# The union libary is responsible for combining all required shards modules
# into a single library alongside the registration and indexing boilerplate
function(shards_generate_union UNION_TARGET_NAME)
  message(STATUS "Generating C++ union target ${UNION_TARGET_NAME}")

  set(OPTS)
  set(ARGS)
  set(MULTI_ARGS
    ENABLED_MODULES # When set, can use to specify which modules to include
  )
  cmake_parse_arguments(UNION_EXTRA "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})

  set(GENERATED_ROOT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
  set(DUMMY_FILE_PATH "${GENERATED_ROOT_DIR}/dummy.cpp")
  file(MAKE_DIRECTORY ${GENERATED_ROOT_DIR})

  if(NOT EXISTS ${DUMMY_FILE_PATH})
    file(WRITE ${DUMMY_FILE_PATH} "// Empty\n")
  endif()

  set(union_SOURCES ${DUMMY_FILE_PATH})

  add_library(${UNION_TARGET_NAME} ${union_SOURCES})
  target_link_libraries(${UNION_TARGET_NAME} shards-core)

  get_property(SHARDS_MODULE_TARGETS GLOBAL PROPERTY SHARDS_MODULE_TARGETS)

  if(NOT SHARDS_INLINE_EVERYTHING)
    # Compile normally inlined sources separately
    # NOTE: You should make sure the source files listed here are included inside core_inlined.cpp as well
    target_sources(${UNION_TARGET_NAME} PRIVATE
      ${SHARDS_DIR}/shards/core/runtime.cpp
    )
  endif()

  foreach(TARGET_NAME ${SHARDS_MODULE_TARGETS})
    get_property(MODULE_ID TARGET ${TARGET_NAME} PROPERTY SHARDS_MODULE_ID)

    if(UNION_EXTRA_ENABLED_MODULES)
      if(NOT "${MODULE_ID}" IN_LIST UNION_EXTRA_ENABLED_MODULES)
        message(DEBUG "Skipping module ${MODULE_ID}")
        continue()
      else()
        message(DEBUG "Including module ${MODULE_ID}")
      endif()
    else()
      is_module_enabled(MODULE_ENABLED ${MODULE_ID})

      if(NOT ${MODULE_ENABLED})
        continue()
      endif()
    endif()

    message(DEBUG "${UNION_TARGET_NAME}: Adding module ${TARGET_NAME} (id: ${MODULE_ID})")
    target_link_libraries(${UNION_TARGET_NAME} ${TARGET_NAME})
    list(APPEND ENABLED_MODULE_IDS ${MODULE_ID})
    list(APPEND ENABLED_MODULE_TARGETS ${TARGET_NAME})
  endforeach()

  # message(STATUS "ENABLED_MODULE_IDS ${ENABLED_MODULE_IDS}")
  # message(STATUS "ENABLED_MODULE_TARGETS ${ENABLED_MODULE_TARGETS}")
  set(GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shards")
  file(MAKE_DIRECTORY ${GENERATED_INCLUDE_DIR})

  # Generate headers
  set(GENERATED_INLINE_HEADER "${GENERATED_INCLUDE_DIR}/inlined.hpp")
  set(GENERATED_TEMP "${CMAKE_CURRENT_BINARY_DIR}/temp.cpp")

  # Add include path to shards core so it can inline the code that is generated here
  target_include_directories(shards-core PUBLIC ${GENERATED_ROOT_DIR})

  file(WRITE ${GENERATED_TEMP}
    "// This file is generated by CMake\n"
    "#ifndef SHARDS_INLINED_HPP\n"
    "#define SHARDS_INLINED_HPP\n"
    "\n"
    "#include <stdint.h>\n"
    "\n"
    "namespace shards {\n"
    "struct InlineShard {\n"
    "  enum Type : uint32_t {\n")

  foreach(MODULE_ID MODULE_TARGET IN ZIP_LISTS ENABLED_MODULE_IDS ENABLED_MODULE_TARGETS)
    get_property(INLINE_SHARDS TARGET ${MODULE_TARGET} PROPERTY SHARDS_MODULE_INLINE_SHARDS)

    if(INLINE_SHARDS)
      foreach(INLINE_SHARD ${INLINE_SHARDS})
        file(APPEND ${GENERATED_TEMP} "    "
          "${INLINE_SHARD},\n")
      endforeach()

      list(APPEND MODULES_WITH_INLINE_IDS "${MODULE_ID}")
    endif()

    get_property(INLINE_SOURCES TARGET ${MODULE_TARGET} PROPERTY SHARDS_MODULE_INLINE_SOURCES)

    if(INLINE_SOURCES)
      message(VERBOSE "INLINING ${INLINE_SOURCES}")

      foreach(INLINE_SOURCE ${INLINE_SOURCES})
        file(RELATIVE_PATH REL_INLINE_SRC_PATH "${GENERATED_INCLUDE_DIR}" "${INLINE_SOURCE}")
        list(APPEND RELATIVE_INLINE_SOURCES ${REL_INLINE_SRC_PATH})
        list(APPEND RELATIVE_INLINE_SOURCES_MODULE_IDS ${MODULE_ID})
      endforeach()
    endif()
  endforeach()

  file(APPEND ${GENERATED_TEMP}
    "  };\n"
    "};\n"
    "}\n"
    "#endif // SHARDS_INLINED_HPP\n")
  file(COPY_FILE ${GENERATED_TEMP} ${GENERATED_INLINE_HEADER} ONLY_IF_DIFFERENT)

  # Generate inline source bindings
  set(GENERATED_INLINE_SOURCE "${GENERATED_INCLUDE_DIR}/inlined.cpp")
  file(WRITE ${GENERATED_TEMP}
    "// This file is generated by CMake\n"
    "#include <shards/union/inlined_prelude.cpp>\n"
    "#include <shards/core/inline.hpp>\n")

  if(SHARDS_INLINE_EVERYTHING)
    # Include module-provided inline sources
    foreach(REL_INLINE_SRC MODULE_ID IN ZIP_LISTS RELATIVE_INLINE_SOURCES RELATIVE_INLINE_SOURCES_MODULE_IDS)
      file(APPEND ${GENERATED_TEMP}
        "#define SHARDS_THIS_MODULE_ID ${MODULE_ID}\n"
        "#include \"${REL_INLINE_SRC}\"\n"
        "#undef SHARDS_THIS_MODULE_ID\n"
      )
      message(STATUS "Shards: Inlining ${REL_INLINE_SRC}")
    endforeach()

    # Include the core inline source file here
    file(APPEND ${GENERATED_TEMP} "#include <shards/core/core_inlined.cpp>\n")
  endif()

  file(APPEND ${GENERATED_TEMP} "\n"
    "namespace shards {\n")

  # Set inline ID declaration (when not inlining)
  if(NOT SHARDS_INLINE_EVERYTHING)
    foreach(MODULE_ID ${MODULES_WITH_INLINE_IDS})
      file(APPEND ${GENERATED_TEMP}
        "ALWAYS_INLINE bool setInlineShardId_${MODULE_ID}(Shard*, std::string_view);\n"
      )
    endforeach()

    file(APPEND ${GENERATED_TEMP} "\n")
  endif()

  file(APPEND ${GENERATED_TEMP}
    "ALWAYS_INLINE void setInlineShardId(Shard *shard, std::string_view name) {\n")

  foreach(MODULE_ID ${MODULES_WITH_INLINE_IDS})
    file(APPEND ${GENERATED_TEMP}
      "  if (setInlineShardId_${MODULE_ID}(shard, name))\n"
      "    return;\n")
  endforeach()

  file(APPEND ${GENERATED_TEMP} "}\n\n")

  # Activation declaration (when not inlining)
  if(NOT SHARDS_INLINE_EVERYTHING)
    foreach(MODULE_ID ${MODULES_WITH_INLINE_IDS})
      file(APPEND ${GENERATED_TEMP}
        "ALWAYS_INLINE bool activateShardInline_${MODULE_ID}(Shard*, SHContext*, const SHVar&, SHVar&);\n"
      )
    endforeach()

    file(APPEND ${GENERATED_TEMP} "\n")
  endif()

  file(APPEND ${GENERATED_TEMP}
    "ALWAYS_INLINE bool activateShardInline(Shard *shard, SHContext *context, const SHVar &input, SHVar &output) {\n")

  foreach(MODULE_ID ${MODULES_WITH_INLINE_IDS})
    file(APPEND ${GENERATED_TEMP}
      "  if (activateShardInline_${MODULE_ID}(shard, context, input, output))\n"
      "    return true;\n")
  endforeach()

  file(APPEND ${GENERATED_TEMP}
    "  return false;\n"
    "}\n\n")

  file(APPEND ${GENERATED_TEMP} "}\n")
  file(COPY_FILE ${GENERATED_TEMP} ${GENERATED_INLINE_SOURCE} ONLY_IF_DIFFERENT)

  target_sources(${UNION_TARGET_NAME} PRIVATE ${GENERATED_INLINE_SOURCE})
  set_source_files_properties(${GENERATED_INLINE_SOURCE} PROPERTIES GENERATED TRUE)

  # Registry header
  set(GENERATED_REGISTRY_HEADER "${GENERATED_INCLUDE_DIR}/registry.hpp")
  file(WRITE ${GENERATED_TEMP}
    "// This file is generated by CMake\n"
    "#ifndef SHARDS_REGISTRY_HPP\n"
    "#define SHARDS_REGISTRY_HPP\n")

  foreach(MODULE_ID ${ENABLED_MODULE_IDS})
    string(TOUPPER ${MODULE_ID} MODULE_ID_UPPPER)
    file(APPEND ${GENERATED_TEMP} "#define SHARDS_WITH_${MODULE_ID_UPPPER} 1\n")
  endforeach()

  file(APPEND ${GENERATED_TEMP} "#endif // SHARDS_REGISTRY_HPP\n")
  file(COPY_FILE ${GENERATED_TEMP} ${GENERATED_REGISTRY_HEADER} ONLY_IF_DIFFERENT)

  # Shard registry
  set(GENERATED_REGISTRY_SOURCE "${GENERATED_INCLUDE_DIR}/registry.cpp")
  file(WRITE ${GENERATED_TEMP}
    "// This file is generated by CMake\n"
    "#include <shards/shards.hpp>\n"
    "\n")

  foreach(MODULE_TARGET ${ENABLED_MODULE_TARGETS})
    get_property(REGISTER_FNS TARGET ${MODULE_TARGET} PROPERTY SHARDS_MODULE_REGISTER_FNS)

    foreach(REGISTER_FN ${REGISTER_FNS})
      list(APPEND ALL_REGISTER_FNS ${REGISTER_FN})
    endforeach()

    list(APPEND MODULES_WITH_INLINE_IDS "${MODULE_ID}")
  endforeach()

  foreach(FN ${ALL_REGISTER_FNS})
    file(APPEND ${GENERATED_TEMP} "extern \"C\" void shardsRegister_${FN}(SHCore* core);\n")
  endforeach()

  file(APPEND ${GENERATED_TEMP} "\n"
    "namespace shards {\n"
    "void registerModuleShards(SHCore* core) {\n")

  foreach(FN ${ALL_REGISTER_FNS})
    file(APPEND ${GENERATED_TEMP} "  shardsRegister_${FN}(core);\n")
  endforeach()

  file(APPEND ${GENERATED_TEMP} "}\n")
  file(APPEND ${GENERATED_TEMP} "}\n")
  file(COPY_FILE ${GENERATED_TEMP} ${GENERATED_REGISTRY_SOURCE} ONLY_IF_DIFFERENT)

  target_sources(${UNION_TARGET_NAME} PRIVATE ${GENERATED_REGISTRY_SOURCE})
  set_source_files_properties(${GENERATED_REGISTRY_SOURCE} PROPERTIES GENERATED TRUE)
endfunction()
