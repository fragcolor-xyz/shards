set(SHARDS_TOOLS_PATH ${SHARDS_DIR}/shards/tools/build/bin)
find_program(BIN2C_EXE NAMES "bin2c" PATHS ${SHARDS_TOOLS_PATH} REQUIRED NO_DEFAULT_PATH NO_SYSTEM_ENVIRONMENT_PATH)

# Used to add files
# This functions strips all the folder names and genrates include paths as follows:
#  file.ext => <PREFIX>file.ext.h
#  subdir/file2.ext => <PREFIX>subdir/file2.ext.h
#
# when RELATIVE_TO is set to 'subdir' for example, the following include paths are generated:
#  subdir/file.ext => <PREFIX>file.ext.h
#  subdir/a/file.ext => <PREFIX>a/file.ext.h
#
# The file is accessed through a variable based on the following pattern: bundled_<name>
# (although you should avoid loading files directly and instead use the bundle manifest)
#  file.ext => bundled_fileΘext
#  file-name.ext => bundled_fileΔnameΘext
#  a/b/file.ext => bundled_aΓbΓfileΘext
function(target_bundle_files BUNDLED_TARGET)
  set(OPTS)
  set(ARGS
    RELATIVE_TO # Specifies the base path that will be stripped from the input paths
    PREFIX # Specifies the base path that will be added onto the bundled paths
    VISIBILITY # Specifies the visibility of include paths (default: PRIVATE)
    NAMESPACE
  )
  set(MULTI_ARGS
    FILES # The list of file paths to bundle
  )
  cmake_parse_arguments(BUNDLED "${OPTS}" "${ARGS}" "${MULTI_ARGS}" ${ARGN})
  message(DEBUG "BUNDLED_RELATIVE_TO: ${BUNDLED_RELATIVE_TO}")
  message(DEBUG "BUNDLED_PREFIX: ${BUNDLED_PREFIX}")
  message(DEBUG "BUNDLED_VISIBILITY: ${BUNDLED_VISIBILITY}")
  message(DEBUG "BUNDLED_FILES: ${BUNDLED_FILES}")

  if(NOT DEFINED BUNDLED_VISIBILITY)
    set(BUNDLED_VISIBILITY PRIVATE)
  endif()

  set(GENERATED_HEADER_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_include_directories(${BUNDLED_TARGET} ${BUNDLED_VISIBILITY} ${GENERATED_HEADER_ROOT_DIR})

  set(GENERATED_HEADER_DIR "${GENERATED_HEADER_ROOT_DIR}/${BUNDLED_NAMESPACE}")
  file(MAKE_DIRECTORY ${GENERATED_HEADER_DIR})

  set(IN_PATHS)
  set(MAPPED_PATHS)
  set(OUT_PATHS)
  set(VAR_NAMES)

  foreach(IN_PATH ${BUNDLED_FILES})
    message(DEBUG "bundle in: ${IN_PATH}")

    if(DEFINED BUNDLED_RELATIVE_TO)
      file(RELATIVE_PATH IN_PATH_REL ${BUNDLED_RELATIVE_TO} ${IN_PATH})
      message(DEBUG " make rel: ${IN_PATH} => ${IN_PATH_REL}")
    else()
      get_filename_component(IN_PATH_REL ${IN_PATH} NAME)
      message(DEBUG " strip path: ${IN_PATH} => ${IN_PATH_REL}")
    endif()

    string(REPLACE "." "Θ" GENERATED_NAME ${IN_PATH_REL})
    string(REPLACE "-" "Δ" GENERATED_NAME ${GENERATED_NAME})
    string(REPLACE "/" "Γ" GENERATED_NAME ${GENERATED_NAME})
    string(REPLACE " " "_" GENERATED_NAME ${GENERATED_NAME})
    set(GENERATED_NAME "bundled_${GENERATED_NAME}")
    set(OUT_PATH "${GENERATED_HEADER_DIR}/${IN_PATH_REL}.h")

    list(APPEND IN_PATHS ${IN_PATH})
    list(APPEND MAPPED_PATHS ${BUNDLED_PREFIX}${IN_PATH_REL})
    list(APPEND OUT_PATHS ${OUT_PATH})
    list(APPEND VAR_NAMES ${GENERATED_NAME})
  endforeach()

  get_target_property(BUNDLED_FILE_NAMES ${BUNDLED_TARGET} "BUNDLED_FILE_NAMES")

  if(NOT BUNDLED_FILE_NAMES)
    set(BUNDLED_FILE_NAMES "")
  endif()

  get_target_property(BUNDLED_FILE_VARS ${BUNDLED_TARGET} "BUNDLED_FILE_VARS")

  if(NOT BUNDLED_FILE_VARS)
    set(BUNDLED_FILE_VARS "")
  endif()

  get_target_property(BUNDLED_FILE_MAPPED_NAMES ${BUNDLED_TARGET} "BUNDLED_FILE_MAPPED_NAMES")

  if(NOT BUNDLED_FILE_MAPPED_NAMES)
    set(BUNDLED_FILE_MAPPED_NAMES "")
  endif()

  foreach(IN_PATH OUT_PATH MAPPED_PATH VAR_NAME IN ZIP_LISTS IN_PATHS OUT_PATHS MAPPED_PATHS VAR_NAMES)
    get_filename_component(OUT_ABS_PATH ${OUT_PATH} ABSOLUTE)
    get_filename_component(IN_ABS_PATH ${IN_PATH} ABSOLUTE)

    set(GENERATED_C_FILE_PATH "${OUT_PATH}.c")
    get_filename_component(TARGET_FOLDER ${GENERATED_C_FILE_PATH} DIRECTORY)
    file(MAKE_DIRECTORY ${TARGET_FOLDER})
    target_sources(${BUNDLED_TARGET} PRIVATE ${GENERATED_C_FILE_PATH})
    set_source_files_properties(${GENERATED_C_FILE_PATH} PROPERTIES GENERATED TRUE)

    # Need to escape $ characters in command argument
    # string(REPLACE "$" "$$" VAR_NAME_ESCAPED ${VAR_NAME})
    message(DEBUG "bundle ${IN_ABS_PATH} => ${OUT_ABS_PATH} as ${VAR_NAME}")
    add_custom_command(OUTPUT ${OUT_PATH} ${GENERATED_C_FILE_PATH}
      MAIN_DEPENDENCY ${IN_ABS_PATH}
      COMMENT "Bundling binary file ${IN_PATH} => ${OUT_PATH}"
      COMMAND ${BIN2C_EXE} -in ${IN_ABS_PATH} -out ${OUT_ABS_PATH} -varName "${VAR_NAME}"
      USES_TERMINAL
    )

    file(RELATIVE_PATH REL_INCLUDE_PATH "${GENERATED_HEADER_ROOT_DIR}" "${OUT_PATH}")
    list(APPEND BUNDLED_FILE_NAMES "${REL_INCLUDE_PATH}")
    list(APPEND BUNDLED_FILE_VARS "${VAR_NAME}")
    list(APPEND BUNDLED_FILE_MAPPED_NAMES "${MAPPED_PATH}")
  endforeach()

  set_target_properties(${BUNDLED_TARGET} PROPERTIES BUNDLED_FILE_NAMES "${BUNDLED_FILE_NAMES}")
  set_target_properties(${BUNDLED_TARGET} PROPERTIES BUNDLED_FILE_VARS "${BUNDLED_FILE_VARS}")
  set_target_properties(${BUNDLED_TARGET} PROPERTIES BUNDLED_FILE_MAPPED_NAMES "${BUNDLED_FILE_MAPPED_NAMES}")
endfunction()

# function(target_bundle_files TARGET VISIBILITY NAMESPACE)

# endfunction()
function(target_generate_bundle_manifest TARGET HEADER_NAME MANIFEST_VAR_NAME)
  set(GENERATED_HEADER_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
  set(GENERATED_C_FILE_PATH "${GENERATED_HEADER_ROOT_DIR}/${HEADER_NAME}.h")
  file(WRITE ${GENERATED_C_FILE_PATH} "")
  target_sources(${TARGET} PRIVATE ${GENERATED_C_FILE_PATH})
  set_source_files_properties(${GENERATED_C_FILE_PATH} PROPERTIES GENERATED TRUE)

  get_target_property(BUNDLED_FILE_NAMES ${TARGET} "BUNDLED_FILE_NAMES")
  get_target_property(BUNDLED_FILE_VARS ${TARGET} "BUNDLED_FILE_VARS")
  get_target_property(BUNDLED_FILE_MAPPED_NAMES ${TARGET} "BUNDLED_FILE_MAPPED_NAMES")

  file(APPEND ${GENERATED_C_FILE_PATH} "#include <unordered_map>\n")

  foreach(FILE_NAME ${BUNDLED_FILE_NAMES})
    file(APPEND ${GENERATED_C_FILE_PATH} "#include <${FILE_NAME}>\n")
  endforeach()

  file(APPEND ${GENERATED_C_FILE_PATH} "static std::unordered_map<std::string, const void *> ${MANIFEST_VAR_NAME} = \{\n")

  foreach(VAR_NAME MAPPED_NAME IN ZIP_LISTS BUNDLED_FILE_VARS BUNDLED_FILE_MAPPED_NAMES)
    file(APPEND ${GENERATED_C_FILE_PATH} "\{\"${MAPPED_NAME}\", ${VAR_NAME}\},\n")
  endforeach()

  file(APPEND ${GENERATED_C_FILE_PATH} "\};\n")
  message(STATUS "Created bundled file manifest for target ${TARGET} at ${GENERATED_C_FILE_PATH}")
endfunction()
