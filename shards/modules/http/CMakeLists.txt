set(HTTP_SUPPORTED ON)

# emscripten implementation depends on fetch support
if(EMSCRIPTEN)
  if(NODEJS)
    message(STATUS "Http module disabled for node.js due to missing Fetch support")
    set(HTTP_SUPPORTED OFF)
  endif()
endif()

if(HTTP_SUPPORTED)
  set(SOURCES
    http.cpp
  )
  set(REGISTER_SHARDS_ARG http)

  # The rust shards use reqwest, which can not run on emscripten
  if(NOT EMSCRIPTEN)
    add_rust_library(NAME shards-http
      PROJECT_PATH ${CMAKE_CURRENT_LIST_DIR})
    set(RUST_TARGETS_ARG RUST_TARGETS shards-http-rust)
    list(APPEND REGISTER_SHARDS_ARG rust)
  endif()

  add_shards_module(http SOURCES ${SOURCES}
    ${RUST_TARGETS_ARG}
    REGISTER_SHARDS ${REGISTER_SHARDS_ARG})

  target_include_directories(shards-module-http PRIVATE ../core)

  if(NOT EMSCRIPTEN)
    target_link_libraries(shards-module-http Boost::beast Boost::asio Boost::context)
    target_include_directories(shards-module-http PRIVATE $<TARGET_PROPERTY:OpenSSL,INTERFACE_INCLUDE_DIRECTORIES>)
  else()
    target_include_directories(shards-module-http PUBLIC $<TARGET_PROPERTY:Boost::asio,INTERFACE_INCLUDE_DIRECTORIES>)
    target_include_directories(shards-module-http PUBLIC $<TARGET_PROPERTY:Boost::beast,INTERFACE_INCLUDE_DIRECTORIES>)
  endif()

  if(WIN32)
    # Indirect rust windows dependency
    target_link_libraries(shards-http-rust INTERFACE RuntimeObject)
  endif()

  if(EMSCRIPTEN)
    em_link_js_library(shards-module-http http.js)
    target_link_options(shards-module-http PUBLIC
      "SHELL:-s FETCH=1")
  endif()
endif()
