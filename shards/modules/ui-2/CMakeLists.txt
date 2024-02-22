set(SOURCES
    ui2.cpp
)

add_shards_module(ui2 SOURCES ${SOURCES}
    REGISTER_SHARDS test)

target_link_libraries(shards-module-ui2
    spdlog magic_enum nameof linalg
    shards-logging shards-fast-string
    Boost::container
)
