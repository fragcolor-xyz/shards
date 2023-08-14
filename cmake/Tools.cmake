set(SHARDS_TOOLS_PATH ${SHARDS_DIR}/shards/tools/build/bin)
find_program(BIN2C_EXE NAMES "bin2c" PATHS ${SHARDS_TOOLS_PATH} REQUIRED NO_DEFAULT_PATH NO_SYSTEM_ENVIRONMENT_PATH)

# Used to add files
# This functions strips all the folder names and genrates include paths as follows:
# file.ext => NAMESPACE/file.ext.h
# subdir/file2.ext => NAMESPACE/file2.ext.h
#
# The file is accessed through a variable based on the following pattern:
# bundled_<name>
# file.ext => bundled_file_ext
function(target_bundle_files TARGET VISIBILITY NAMESPACE)
  set(GENERATED_HEADER_ROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
  target_include_directories(${TARGET} ${VISIBILITY} ${GENERATED_HEADER_ROOT_DIR})

  set(GENERATED_HEADER_DIR "${GENERATED_HEADER_ROOT_DIR}/${NAMESPACE}")
  file(MAKE_DIRECTORY ${GENERATED_HEADER_DIR})

  set(IN_PATHS)
  set(OUT_PATHS)
  set(VAR_NAMES)

  foreach(IN_PATH ${ARGN})
    get_filename_component(FILENAME ${IN_PATH} NAME)
    string(REPLACE "." "_" GENERATED_NAME ${FILENAME})
    set(GENERATED_NAME "bundled_${GENERATED_NAME}")
    set(OUT_PATH "${GENERATED_HEADER_DIR}/${FILENAME}.h")

    list(APPEND IN_PATHS ${IN_PATH})
    list(APPEND OUT_PATHS ${OUT_PATH})
    list(APPEND VAR_NAMES ${GENERATED_NAME})
  endforeach()

  get_target_property(BUNDLED_FILE_NAMES ${TARGET} "BUNDLED_FILE_NAMES")

  if(NOT BUNDLED_FILE_NAMES)
    set(BUNDLED_FILE_NAMES "")
  endif()

  get_target_property(BUNDLED_FILE_VARS ${TARGET} "BUNDLED_FILE_VARS")

  if(NOT BUNDLED_FILE_VARS)
    set(BUNDLED_FILE_VARS "")
  endif()

  get_target_property(BUNDLED_FILE_MAPPED_NAMES ${TARGET} "BUNDLED_FILE_MAPPED_NAMES")

  if(NOT BUNDLED_FILE_MAPPED_NAMES)
    set(BUNDLED_FILE_MAPPED_NAMES "")
  endif()

  foreach(IN_PATH OUT_PATH VAR_NAME IN ZIP_LISTS IN_PATHS OUT_PATHS VAR_NAMES)
    get_filename_component(TARGET_DIR_PATH ${OUT_PATH} DIRECTORY)
    get_filename_component(IN_ABS_PATH ${IN_PATH} ABSOLUTE)

    set(GENERATED_C_FILE_PATH "${OUT_PATH}.c")
    file(TOUCH ${GENERATED_C_FILE_PATH})
    target_sources(${TARGET} PRIVATE ${GENERATED_C_FILE_PATH})
    set_source_files_properties(${GENERATED_C_FILE_PATH} PROPERTIES GENERATED TRUE)

    add_custom_command(OUTPUT ${OUT_PATH} ${GENERATED_C_FILE_PATH}
      MAIN_DEPENDENCY ${IN_PATH}
      WORKING_DIRECTORY ${TARGET_DIR_PATH}
      COMMENT "Bundling binary file ${IN_PATH} => ${OUT_PATH}"
      COMMAND ${BIN2C_EXE} -in ${IN_ABS_PATH} -out ${OUT_PATH} -varName ${VAR_NAME}
      USES_TERMINAL
    )

    file(RELATIVE_PATH REL_INCLUDE_PATH "${GENERATED_HEADER_ROOT_DIR}" "${OUT_PATH}")
    list(APPEND BUNDLED_FILE_NAMES "${REL_INCLUDE_PATH}")
    list(APPEND BUNDLED_FILE_VARS "${VAR_NAME}")
    list(APPEND BUNDLED_FILE_MAPPED_NAMES "${IN_PATH}")
  endforeach()

  set_target_properties(${TARGET} PROPERTIES BUNDLED_FILE_NAMES "${BUNDLED_FILE_NAMES}")
  set_target_properties(${TARGET} PROPERTIES BUNDLED_FILE_VARS "${BUNDLED_FILE_VARS}")
  set_target_properties(${TARGET} PROPERTIES BUNDLED_FILE_MAPPED_NAMES "${BUNDLED_FILE_MAPPED_NAMES}")
endfunction()

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

macro(shards_build INPUT OUTPUT)
    # Define the full path to the shards command
    set(SHARDS_COMMAND ${CMAKE_BINARY_DIR}/shards)

    # Create a custom command to build the input file
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${SHARDS_COMMAND} build ${INPUT} -o ${OUTPUT}
        DEPENDS ${INPUT} shards # Depend on the shards target
        COMMENT "Building ${INPUT} with Shards"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} # Set the working directory
    )

    # Create a custom target that depends on the custom command
    add_custom_target(${OUTPUT}_target ALL DEPENDS ${OUTPUT})
endmacro()
