#include "gfx.hpp"
#include "gfx/buffer_vars.hpp"
#include <gfx/context.hpp>
#include <gfx/features/base_color.hpp>
#include <gfx/features/debug_color.hpp>
#include <gfx/features/transform.hpp>
#include <gfx/loop.hpp>
#include <gfx/math.hpp>
#include <gfx/mesh.hpp>
#include <gfx/renderer.hpp>
#include <gfx/view.hpp>
#include <gfx/window.hpp>
#include <linalg_shim.hpp>

using namespace chainblocks;

namespace gfx {
using chainblocks::Mat4;

void CBDrawable::updateVariables() {
	if (transformVar.isVariable()) {
		drawable->transform = chainblocks::Mat4(transformVar.get());
	}
}

struct DrawableBlock {
	static inline Type MeshVarType = Type::VariableOf(MeshType);
	static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);

	static inline Parameters params{
		{"Mesh", CBCCSTR("The mesh to draw"), {MeshVarType}},
		{"Transform", CBCCSTR("The transform to use"), {TransformVarType}},
	};

	static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
	static CBTypesInfo outputTypes() { return DrawableType; }
	static CBParametersInfo parameters() { return params; }

	ParamVar _meshVar{};
	ParamVar _transformVar{};

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_meshVar = value;
			break;
		case 1:
			_transformVar = value;
			break;
		default:
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return _meshVar;
		case 1:
			return _transformVar;
		default:
			return Var::Empty;
		}
	}

	void warmup(CBContext *context) {
		_meshVar.warmup(context);
		_transformVar.warmup(context);
	}

	void cleanup() {
		_meshVar.cleanup();
		_transformVar.cleanup();
	}

	CBVar activate(CBContext *cbContext, const CBVar &input) {
		MeshPtr *meshPtr = (MeshPtr *)_meshVar.get().payload.objectValue;
		if (!meshPtr)
			throw formatException("Mesh must be set");

		CBDrawable *cbDrawable = DrawableObjectVar.New();
		cbDrawable->drawable = std::make_shared<Drawable>(*meshPtr, chainblocks::Mat4(_transformVar.get()));
		if (_transformVar.isVariable()) {
			cbDrawable->transformVar = (CBVar &)_transformVar;
			cbDrawable->transformVar.warmup(cbContext);
		}

		return DrawableObjectVar.Get(cbDrawable);
	}
};

struct Draw : public BaseConsumer {
	static inline Type DrawableSeqType = Type::SeqOf(DrawableType);
	static inline Types DrawableTypes{DrawableType, DrawableSeqType};

	static CBTypesInfo inputTypes() { return DrawableTypes; }
	static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
	static CBParametersInfo parameters() { return CBParametersInfo(); }

	void warmup(CBContext *cbContext) { baseConsumerWarmup(cbContext); }
	void cleanup() { baseConsumerCleanup(); }
	CBTypeInfo compose(const CBInstanceData &data) {
		if (data.inputType.basicType == CBType::Seq) {
			OVERRIDE_ACTIVATE(data, activateSeq);
		} else {
			OVERRIDE_ACTIVATE(data, activateSingle);
		}
		return CoreInfo::NoneType;
	}

	void addDrawableToQueue(DrawablePtr drawable) {
		auto &dq = getMainWindowGlobals().drawQueue;
		dq.add(drawable);
	}

	void updateCBDrawable(CBDrawable *cbDrawable) {
		// Update transform if it's referencing a context variable
		if (cbDrawable->transformVar.isVariable()) {
			cbDrawable->drawable->transform = chainblocks::Mat4(cbDrawable->transformVar.get());
		}
	}

	CBVar activateSeq(CBContext *cbContext, const CBVar &input) {
		spdlog::debug("activate GFX.Draw [Seq]");

		auto &seq = input.payload.seqValue;
		for (size_t i = 0; i < seq.len; i++) {
			CBDrawable *cbDrawable = (CBDrawable *)seq.elements[i].payload.objectValue;
			assert(cbDrawable);
			cbDrawable->updateVariables();
			addDrawableToQueue(cbDrawable->drawable);
		}

		return CBVar{};
	}

	CBVar activateSingle(CBContext *cbContext, const CBVar &input) {
		spdlog::debug("activate GFX.Draw [Single]");

		CBDrawable *cbDrawable = (CBDrawable *)input.payload.objectValue;
		updateCBDrawable(cbDrawable);
		addDrawableToQueue(cbDrawable->drawable);

		return CBVar{};
	}

	CBVar activate(CBContext *cbContext, const CBVar &input) { throw ActivationError("GFX.Draw: Unsupported input type"); }
};

struct BuiltinFeature {
	enum class Id {
		Transform,
		BaseColor,
		VertexColorFromNormal,
	};

	static constexpr uint32_t IdTypeId = 'feid';
	static inline chainblocks::Type IdType = chainblocks::Type::Enum(VendorId, IdTypeId);
	static inline chainblocks::EnumInfo<Id> IdEnumInfo{"BuiltinFeatureId", VendorId, IdTypeId};

	static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
	static CBTypesInfo outputTypes() { return FeatureType; }

	static inline Parameters params{{"Id", CBCCSTR("Builtin feature id."), {IdType}}};
	static CBParametersInfo parameters() { return params; }

	Id _id{};
	FeaturePtr *_feature{};

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_id = Id(value.payload.enumValue);
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return Var::Enum(_id, VendorId, IdTypeId);
		default:
			return Var::Empty;
		}
	}

	void cleanup() {
		if (_feature) {
			FeatureObjectVar.Release(_feature);
			_feature = nullptr;
		}
	}

	void warmup(CBContext *context) {
		_feature = FeatureObjectVar.New();
		switch (_id) {
		case Id::Transform:
			*_feature = features::Transform::create();
			break;
		case Id::BaseColor:
			*_feature = features::BaseColor::create();
			break;
		case Id::VertexColorFromNormal:
			*_feature = features::DebugColor::create("normal", ProgrammableGraphicsStage::Vertex);
			break;
		}
	}

	CBVar activate(CBContext *context, const CBVar &input) { return FeatureObjectVar.Get(_feature); }
};

struct DrawablePass {
	static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
	static CBTypesInfo outputTypes() { return PipelineStepType; }

	static inline Type PipelineStepSeqType = Type::SeqOf(PipelineStepType);
	static inline Parameters params{
		{"Features", CBCCSTR("Features to use."), {Type::VariableOf(PipelineStepSeqType), PipelineStepSeqType}},
	};
	static CBParametersInfo parameters() { return params; }

	PipelineStepPtr *_step{};
	ParamVar _features{};

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_features = value;
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return _features;
		default:
			return Var::Empty;
		}
	}

	void cleanup() {
		if (_step) {
			PipelineStepObjectVar.Release(_step);
			_step = nullptr;
		}
		_features.cleanup();
	}

	void warmup(CBContext *context) {
		_step = PipelineStepObjectVar.New();
		_features.warmup(context);
	}

	std::vector<FeaturePtr> collectFeatures(const CBVar &input) {
		std::vector<FeaturePtr> result;
		std::vector<Var> featureVars(Var(_features.get()));
		for (auto &featureVar : featureVars) {
			if (featureVar.payload.objectValue) {
				result.push_back(*(FeaturePtr *)featureVar.payload.objectValue);
			}
		}
		return result;
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		*_step = makeDrawablePipelineStep(RenderDrawablesStep{
			.features = collectFeatures(_features),
		});
		return PipelineStepObjectVar.Get(_step);
	}
};

void CBView::updateVariables() {
	if (viewTransformVar.isVariable()) {
		view->view = chainblocks::Mat4(viewTransformVar.get());
	}
}

struct ViewBlock {
	static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
	static CBTypesInfo outputTypes() { return ViewType; }

	static inline Type PipelineStepSeqType = Type::SeqOf(PipelineStepType);
	static inline Parameters params{
		{"View", CBCCSTR("The view matrix."), {Type::VariableOf(CoreInfo::Float4x4Type), CoreInfo::Float4x4Type}},
	};
	static CBParametersInfo parameters() { return params; }

	ParamVar _viewTransform;
	CBView *_view;

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_viewTransform = value;
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return _viewTransform;
		default:
			return Var::Empty;
		}
	}

	void cleanup() {
		if (_view) {
			ViewObjectVar.Release(_view);
			_view = nullptr;
		}
		_viewTransform.cleanup();
	}

	void warmup(CBContext *context) {
		_view = ViewObjectVar.New();
		_viewTransform.warmup(context);
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		ViewPtr view = _view->view = std::make_shared<View>();
		if (_viewTransform->valueType != CBType::None) {
			view->view = chainblocks::Mat4(_viewTransform.get());
		}

		// TODO: Add projection/viewport override params
		view->proj = ViewPerspectiveProjection{};
		// view->viewport

		if (_viewTransform.isVariable()) {
			_view->viewTransformVar = (CBVar &)_viewTransform;
			_view->viewTransformVar.warmup(context);
		}
		return ViewObjectVar.Get(_view);
	}
};

struct Render : public BaseConsumer {
	static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
	static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

	static inline Type PipelineStepSeqType = Type::SeqOf(PipelineStepType);
	static inline Parameters params{
		{"Steps", CBCCSTR("Render steps to follow."), {Type::VariableOf(PipelineStepSeqType), PipelineStepSeqType}},
		{"View", CBCCSTR("The view to render into."), {Type::VariableOf(ViewType), ViewType}},
		{"Views", CBCCSTR("The views to render into."), {Type::VariableOf(ViewSeqType), ViewSeqType}},
	};
	static CBParametersInfo parameters() { return params; }

	ParamVar _steps{};
	ParamVar _view{};
	ParamVar _views{};
	ViewPtr defaultView;
	std::vector<ViewPtr> views;

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_steps = value;
			break;
		case 1:
			_view = value;
			break;
		case 2:
			_views = value;
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return _steps;
		case 1:
			return _view;
		case 2:
			return _views;
		default:
			return Var::Empty;
		}
	}

	void cleanup() {
		baseConsumerCleanup();
		_steps.cleanup();
		_view.cleanup();
		_views.cleanup();
		defaultView.reset();
	}

	void warmup(CBContext *context) {
		baseConsumerWarmup(context);
		_steps.warmup(context);
		_view.warmup(context);
		_views.warmup(context);
	}

	ViewPtr getDefaultView() {
		if (!defaultView)
			defaultView = std::make_shared<View>();
		return defaultView;
	}

	void validateViewParameters() {
		if (_view->valueType != CBType::None && _views->valueType != CBType::None) {
			throw ComposeError("GFX.Render :View and :Views can not be set at the same time");
		}
	}

	CBTypeInfo compose(CBInstanceData &data) {
		composeCheckMainThread(data);
		validateViewParameters();
		return CoreInfo::NoneType;
	}

	const std::vector<ViewPtr> &updateAndCollectViews() {
		views.clear();
		if (_view->valueType != CBType::None) {
			const CBVar &var = _view.get();
			CBView *cbView = (CBView *)var.payload.objectValue;
			cbView->updateVariables();
			views.push_back(cbView->view);
		} else if (_views->valueType != CBType::None) {
			SeqVar &seq = (SeqVar &)_views.get();
			for (size_t i = 0; i < seq.size(); i++) {
				const CBVar &viewVar = seq[i];
				CBView *cbView = (CBView *)viewVar.payload.objectValue;
				cbView->updateVariables();
				views.push_back(cbView->view);
			}
		} else {
			views.push_back(getDefaultView());
		}
		return views;
	}

	PipelineSteps collectPipelineSteps() {
		PipelineSteps steps;
		Var stepsCBVar(_steps.get());
		std::vector<Var> stepVars(stepsCBVar);
		for (const auto &stepVar : stepVars) {
			steps.push_back(*(PipelineStepPtr *)stepVar.payload.objectValue);
		}
		return steps;
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		MainWindowGlobals &globals = getMainWindowGlobals();
		globals.renderer->render(globals.drawQueue, updateAndCollectViews(), collectPipelineSteps());
		return CBVar{};
	}
};

extern void registerMainWindowBlocks();
extern void registerMeshBlocks();
void registerBlocks() {
	registerMainWindowBlocks();
	registerMeshBlocks();
	REGISTER_CBLOCK("GFX.Drawable", DrawableBlock);
	REGISTER_CBLOCK("GFX.Draw", Draw);
	REGISTER_CBLOCK("GFX.BuiltinFeature", BuiltinFeature);
	REGISTER_CBLOCK("GFX.DrawablePass", DrawablePass);
	REGISTER_CBLOCK("GFX.View", ViewBlock);
	REGISTER_CBLOCK("GFX.Render", Render);
}
} // namespace gfx