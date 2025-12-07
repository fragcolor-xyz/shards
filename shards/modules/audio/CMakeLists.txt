set(SOURCES
  dsp.cpp
  compressor.cpp
  codec.cpp
  synth.cpp
)

add_shards_module(audio SOURCES ${SOURCES}
  REGISTER_SHARDS audio dsp synth
  EXPERIMENTAL)

if(SHARDS_AUDIO_LINK_WHOLE)
  # Remove opus from normal linking
  target_link_libraries(shards-module-audio
    kissfft miniaudio shards-logging
  )
  
  # Add opus with whole-archive using plain signature
  target_link_libraries(shards-module-audio
    "-Wl,--whole-archive"
    opus
    "-Wl,--no-whole-archive"
  )
else()
  target_link_libraries(shards-module-audio
    kissfft miniaudio shards-logging opus
  )
endif()

# Need to compile specific files as objective-c
if(IOS OR VISIONOS)
  target_sources(shards-module-audio PRIVATE audio.mm)
else()
  target_sources(shards-module-audio PRIVATE audio.cpp)
endif()

# if(EMSCRIPTEN)
#   target_compile_definitions(shards-module-audio PRIVATE MA_ENABLE_AUDIO_WORKLETS=1)
#   target_link_options(shards-module-audio PUBLIC
#     "-sAUDIO_WORKLET=1"
#     "-sWASM_WORKERS=1" # Required for miniaudio
#   )
# endif()

# SIMD optimizations for synth.cpp
# Apple uses Accelerate framework, others use SLEEF if available, WASM uses scalar fallback
if(APPLE)
  target_link_libraries(shards-module-audio "-framework Accelerate")
elseif(NOT EMSCRIPTEN AND TARGET sleef)
  target_link_libraries(shards-module-audio sleef)
endif()
