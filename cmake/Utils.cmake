# Get all propreties that cmake supports
if(NOT CMAKE_PROPERTIES_TO_DUPLICATE)
  execute_process(COMMAND cmake --help-property-list OUTPUT_VARIABLE CMAKE_PROPERTIES_TO_DUPLICATE)
  
  # Convert command output into a CMake list
  string(REGEX REPLACE ";" "\\\\;" CMAKE_PROPERTIES_TO_DUPLICATE "${CMAKE_PROPERTIES_TO_DUPLICATE}")
  string(REGEX REPLACE "\n" ";" CMAKE_PROPERTIES_TO_DUPLICATE "${CMAKE_PROPERTIES_TO_DUPLICATE}")
  
  set(IGNORED_PROPERTIES "IMPORTED_GLOBAL" "NAME" "TYPE" "SOURCES")
  foreach(IGNORED_PROPERTY ${IGNORED_PROPERTIES})
    list(REMOVE_ITEM CMAKE_PROPERTIES_TO_DUPLICATE ${IGNORED_PROPERTY})
  endforeach()
endif()

function(duplicate_library_target TARGET TYPE NEW_TARGET)
  add_library(${NEW_TARGET} ${TYPE})
  
  foreach(PROPERTY ${CMAKE_PROPERTIES_TO_DUPLICATE})
    string(REPLACE "<CONFIG>" "${CMAKE_BUILD_TYPE}" PROPERTY ${PROPERTY})

    # Fix https://stackoverflow.com/questions/32197663/how-can-i-remove-the-the-location-PROPERTY-may-not-be-read-from-TARGET-error-i
    if(PROPERTY STREQUAL "LOCATION" OR PROPERTY MATCHES "^LOCATION_" OR PROPERTY MATCHES "_LOCATION$")
      continue()
    endif()

    get_property(IS_PROPERTY_SET TARGET ${TARGET} PROPERTY ${PROPERTY} SET)
    if(IS_PROPERTY_SET)
      get_target_property(VALUE ${TARGET} ${PROPERTY})
      set_target_properties(${NEW_TARGET} PROPERTIES ${PROPERTY} "${VALUE}")
    endif()
  endforeach()
    
  get_target_property(TARGET_SOURCE_DIR ${TARGET} SOURCE_DIR)
  get_target_property(TARGET_SOURCES ${TARGET} SOURCES)
  foreach(SOURCE ${TARGET_SOURCES})
    get_filename_component(SOURCE_ABS ${SOURCE} ABSOLUTE BASE_DIR ${TARGET_SOURCE_DIR})
    target_sources(${NEW_TARGET} PRIVATE ${SOURCE_ABS})
  endforeach()
endfunction()
