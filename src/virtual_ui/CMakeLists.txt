add_library(virtual-ui context.cpp panel.cpp)
target_include_directories(virtual-ui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
target_link_libraries(virtual-ui SDL2 gfx)
target_compile_features(virtual-ui PUBLIC cxx_std_20)
