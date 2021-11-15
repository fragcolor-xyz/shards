#pragma once

#include "../context.hpp"
#include "../enums.hpp"
#include "../feature.hpp"
#include "../fields.hpp"
#include "../frame.hpp"
#include "gfx/context.hpp"
#include "gfx/feature.hpp"
#include <memory>

namespace gfx {
namespace features {

struct Time {
	static inline FeaturePtr create() {
		auto feature = std::make_shared<Feature>();
		feature->shaderParams.emplace_back("time");
		auto drawData = feature->sharedDrawData.emplace_back([](const FeatureCallbackContext &callbackContext, IDrawDataCollector &drawDataCollector) {
			Context &context = callbackContext.context;
			auto frame = context.getFrameRenderer();

			float3 time = float3(frame->inputs.deltaTime, frame->inputs.time, float(frame->inputs.frameNumber));
			drawDataCollector.set_vec3("time", time);
		});

		return feature;
	}
};

} // namespace features
} // namespace gfx
