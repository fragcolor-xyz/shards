// Stub definitions for llama.cpp common/ functions that are referenced from
// code paths we never exercise (fit-params, perf memory breakdown, speculative
// decoding type names). Linking the upstream definitions would drag in the
// full ngram-map / ngram-cache / speculative.cpp / fit.cpp chain that our
// inference flow doesn't use.
//
// Why this is needed even though the call sites are unreachable in our flow:
//   - common.cpp's common_init_result constructor references common_fit_params
//     and common_speculative_type_to_str. We don't construct common_init_result
//     ourselves, but the constructor still ends up in the object file and the
//     linker has to resolve the symbol references.
//   - common/sampling.cpp's common_perf_print references
//     common_memory_breakdown_print for the same reason.
//
// On macOS Release with dead-code elimination the unused functions get
// stripped before link resolution, so the build succeeds locally. Linux Debug
// (and any non-DCE build) keeps the references and fails to link without
// these stubs.
//
// If anything in shards ever actually calls these code paths, replace the
// stubs with proper compilation of fit.cpp / speculative.cpp (and their
// transitive ngram-* deps) in modules/llm/CMakeLists.txt.

#include "fit.h"
#include "speculative.h"

#include <cstddef>
#include <cstdint>
#include <string>

void common_memory_breakdown_print(const struct llama_context * /*ctx*/) {}

enum common_params_fit_status common_fit_params(const char * /*path_model*/, struct llama_model_params * /*mparams*/,
                                                struct llama_context_params * /*cparams*/, float * /*tensor_split*/,
                                                struct llama_model_tensor_buft_override * /*tensor_buft_overrides*/,
                                                size_t * /*margins*/, uint32_t /*n_ctx_min*/,
                                                enum ggml_log_level /*log_level*/) {
  return COMMON_PARAMS_FIT_STATUS_FAILURE;
}

std::string common_speculative_type_to_str(enum common_speculative_type /*type*/) { return {}; }
