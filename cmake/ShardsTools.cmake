# Tools that depend on running shards

if(CMAKE_CROSSCOMPILING OR EXTERNAL_SHARDS_COMMAND)
  if(DEFINED ENV{SHARDS_COMMAND})
    file(REAL_PATH $ENV{SHARDS_COMMAND} SHARDS_COMMAND_ABS)
    set(SHARDS_COMMAND ${SHARDS_COMMAND_ABS} CACHE STRING "The shards interpreter/compiler" FORCE)
  elseif(SHARDS_COMMAND STREQUAL "")
    # Find shards when cross compiling since it's needed to precompile scripts
    find_program(SHARDS_COMMAND REQUIRED
      NAMES shards
      DOC "The shards interpreter/compiler"
      PATHS 
        ${EXTERNAL_SHARDS_COMMAND}
        ${SHARDS_DIR}/build
    )
  endif()
else()
  set(SHARDS_COMMAND $<TARGET_FILE:shards>)
  set(SHARDS_COMMAND_DEPENDS shards)
endif()
message(STATUS "Using shards command line: ${SHARDS_COMMAND}")

function(shards_build INPUT OUTPUT)
    # Create a custom command to build the input file
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${SHARDS_COMMAND} build ${INPUT} -o ${OUTPUT}
        DEPENDS ${INPUT} ${SHARDS_COMMAND_DEPENDS} # Depend on the shards target
        COMMENT "Building ${INPUT} with Shards"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} # Set the working directory
    )

    # Create a custom target that depends on the custom command
    add_custom_target(${OUTPUT}_target ALL DEPENDS ${OUTPUT})
endfunction()
