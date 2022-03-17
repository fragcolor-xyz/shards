add_executable(bin2c main.cpp)
target_link_libraries(bin2c spdlog)
target_include_directories(bin2c PRIVATE include)
target_compile_features(bin2c PUBLIC cxx_std_17)
