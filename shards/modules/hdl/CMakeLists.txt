set(SOURCES
  hdl.cpp
  types.cpp
  blocks.cpp
  context.cpp
  registry.cpp
  verilog_emitter.cpp
  shards/hardware.cpp
  shards/io.cpp
  shards/math.cpp
)

add_shards_module(hdl SOURCES ${SOURCES}
  REGISTER_SHARDS hdl hdl_io
  EXPERIMENTAL)
