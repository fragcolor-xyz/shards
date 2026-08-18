# LLM module - llama.cpp/whisper.cpp based shards (LLM.* and Whisper.*)
# See also shards/modules/candle for AI.* shards (mistral.rs/candle)
is_module_enabled(LLM_ENABLED llm)
if(LLM_ENABLED AND NOT EMSCRIPTEN AND NOT WATCHOS)
  # Collect all mtmd model sources via glob
  file(GLOB MTMD_MODEL_SOURCES ${llama.cpp_SOURCE_DIR}/tools/mtmd/models/*.cpp)

  set(SOURCES
    embedd.cpp
    chat.cpp
    sh_whisper.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/clip.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd-audio.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd-image.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd-helper.cpp
    ${llama.cpp_SOURCE_DIR}/tools/mtmd/mtmd-helper-gen.cpp
    ${MTMD_MODEL_SOURCES}
    ${whisper.cpp_SOURCE_DIR}/src/whisper.cpp
  )

  add_shards_module(llm SOURCES ${SOURCES}
    REGISTER_SHARDS llm llm_chat whisper)

  target_include_directories(shards-module-llm PRIVATE ${whisper.cpp_SOURCE_DIR}/include)
  # llama-common provides common_batch_*, common_tokenize, common_sampler_*,
  # build-info, etc. — everything chat.cpp/embedd.cpp need from upstream's
  # common/ helper layer. Linking it (instead of compiling sources directly)
  # lets upstream's CMake handle generated files and the internal dependency
  # graph between common.cpp / sampling.cpp / fit.cpp / speculative.cpp /
  # build-info.cpp.
  target_link_libraries(shards-module-llm llama llama-common)
  target_compile_definitions(shards-module-llm PRIVATE WHISPER_VERSION="shards-whisper-0.1")

  # Make sure clip.h, llama, mtmd, and vendor headers are in the include path
  target_include_directories(shards-module-llm PRIVATE
    ${llama.cpp_SOURCE_DIR}
    ${llama.cpp_SOURCE_DIR}/common
    ${llama.cpp_SOURCE_DIR}/tools/mtmd
    ${llama.cpp_SOURCE_DIR}/vendor)
  target_compile_definitions(shards-module-llm PRIVATE SKIP_STB_IMAGE_IMPLEMENTATION LLAVA_LOG_OFF)
endif()
