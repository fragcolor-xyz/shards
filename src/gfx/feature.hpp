#pragma once
#include "enums.hpp"
#include "fields.hpp"
#include "linalg.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace gfx {

struct BGFXStencilState {
	uint32_t front = 0;
	uint32_t back = 0;
};

struct BGFXPipelineState {
	uint64_t state = 0;
	std::optional<short4> scissor;
	std::optional<BGFXStencilState> stencil;
};

struct FeaturePipelineState {
#define PIPELINE_STATE(_name, _type)                              \
private:                                                          \
	std::optional<_type> _name;                                   \
                                                                  \
public:                                                           \
	void set_##_name(const _type &_name) { this->_name = _name; } \
	void clear_##_name(const _type &_name) { this->_name.reset(); }
#include "pipeline_states.def"

public:
	FeaturePipelineState combine(const FeaturePipelineState &other) const {
		FeaturePipelineState result = *this;
#define PIPELINE_STATE(_name, _type) \
	if (other._name)                 \
		result._name = other._name;
#include "pipeline_states.def"
		return result;
	}

	bool operator==(const FeaturePipelineState &other) const {
#define PIPELINE_STATE(_name, _type) \
	if (_name != other._name)        \
		return false;
#include "pipeline_states.def"
		return true;
	}

	bool operator!=(const FeaturePipelineState &other) const { return !(*this == other); }

	size_t getHash() const {
		size_t hash = 0;
#define PIPELINE_STATE(_name, _type) hash = (hash * 3) ^ std::hash<decltype(_name)>{}(_name);
#include "pipeline_states.def"
		return hash;
	}

	BGFXPipelineState toBGFXState(WindingOrder frontFace) const;

	template <typename T> void hashStatic(T &hasher) const {
#define PIPELINE_STATE(_name, _type) hasher(_name);
#include "pipeline_states.def"
	}
};

struct Drawable;
struct View;
struct Context;

typedef std::shared_ptr<View> ViewPtr;

struct FeatureCallbackContext {
	Context &context;
	View *view = nullptr;
	Drawable *drawable = nullptr;
};

typedef std::function<bool(const FeatureCallbackContext &)> FeatureFilterCallback;

enum class FeatureFilterType { Inclusion, Exclusion };
struct FeatureFilter {
	FeatureFilterCallback callback;
	FeatureFilterType type = FeatureFilterType::Inclusion;

	FeatureFilter(FeatureFilterCallback callback, FeatureFilterType type = FeatureFilterType::Inclusion) : callback(callback), type(type) {}
};

struct IDrawDataCollector;
typedef std::function<void(const FeatureCallbackContext &, IDrawDataCollector &)> FeatureDrawDataFunction;

struct FeaturePrecomputeCallbackContext {
	Context &context;
	const std::vector<ViewPtr> views;
};
typedef std::function<void(const FeaturePrecomputeCallbackContext &)> FeaturePrecomputeFunction;

enum class DependencyType { Before, After };
struct NamedDependency {
	std::string name;
	DependencyType type = DependencyType::After;

	NamedDependency() = default;
	NamedDependency(std::string name, DependencyType type = DependencyType::After) : name(name), type(type) {}

	template <typename T> void hashStatic(T &hasher) const {
		hasher(name);
		hasher(type);
	}
};

struct FeatureShaderCode {
	ProgrammableGraphicsStage stage;
	std::string name;
	std::string code;
	std::vector<NamedDependency> dependencies;

	FeatureShaderCode() = default;
	FeatureShaderCode(const std::string &name, ProgrammableGraphicsStage stage = ProgrammableGraphicsStage::Fragment) : stage(stage), name(name) {}

	template <typename T> void hashStatic(T &hasher) const {
		hasher(stage);
		hasher(name);
		hasher(code);
		hasher(dependencies);
	}
};

struct FeatureShaderGlobal {
	FieldType type = FieldType::Float4;
	std::string name;
	FieldVariant defaultValue;

	template <typename T> void hashStatic(T &hasher) const {
		hasher(type);
		hasher(name);
	}
};

struct FeatureShaderVarying {
	FieldType type = FieldType::Float4;
	std::string name;

	template <typename T> void hashStatic(T &hasher) const {
		hasher(type);
		hasher(name);
	}
};

struct FeatureShaderField {
	FieldType type = FieldType::Float4;
	std::string name;
	FieldVariant defaultValue;

	FeatureShaderField() = default;
	FeatureShaderField(std::string name, FieldType type = FieldType::Float4, FieldVariant defaultValue = FieldVariant());
	FeatureShaderField(std::string name, FieldVariant defaultValue);

	template <typename T> void hashStatic(T &hasher) const {
		hasher(type);
		hasher(name);
	}
};

struct SceneRenderer;
struct MaterialUsageContext;
struct Feature {
	std::vector<FeatureFilter> filters;
	std::vector<FeatureDrawDataFunction> sharedDrawData;
	std::vector<FeatureDrawDataFunction> drawData;
	std::vector<FeaturePrecomputeFunction> precompute;
	FeaturePipelineState state;
	std::vector<FeatureShaderField> shaderParams;
	std::vector<FeatureShaderField> shaderGlobals;
	std::vector<FeatureShaderField> shaderVaryings;
	std::vector<FeatureShaderCode> shaderCode;

	virtual ~Feature() = default;

	template <typename T> void hashStatic(T &hasher) const {
		hasher(state);
		hasher(shaderParams);
		hasher(shaderGlobals);
		hasher(shaderVaryings);
		hasher(shaderCode);
	}
};
typedef std::shared_ptr<Feature> FeaturePtr;

struct FeatureProcessor {};

} // namespace gfx
