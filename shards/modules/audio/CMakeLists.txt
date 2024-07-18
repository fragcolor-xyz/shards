set(SOURCES
  dsp.cpp
)

add_shards_module(audio SOURCES ${SOURCES}
  REGISTER_SHARDS audio dsp
  EXPERIMENTAL)

target_link_libraries(shards-module-audio
  kissfft miniaudio
)

# Need to compile specific files as objective-c
if(IOS OR VISIONOS)
  target_sources(shards-module-audio PRIVATE audio.mm)
else()
  target_sources(shards-module-audio PRIVATE audio.cpp)
endif()

if(EMSCRIPTEN)
  target_link_options(shards-module-audioPRIVATE
    "-sAUDIO_WORKLET=1"
    "-sWASM_WORKERS=1" # Required for miniaudio
  )
endif()