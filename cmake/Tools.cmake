set(SHARDS_TOOLS_PATH ${SHARDS_DIR}/src/tools/build/bin)
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
  endforeach()

endfunction()
