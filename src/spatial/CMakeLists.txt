add_library(spatial context.cpp panel.cpp)
target_include_directories(spatial PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(spatial SDL3 gfx)
target_compile_features(spatial PUBLIC cxx_std_20)
