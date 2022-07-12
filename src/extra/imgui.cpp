/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "imgui.hpp"
#include "shards/shared.hpp"
#include "gfx.hpp"
#include "gfx/shards_types.hpp"
#include "runtime.hpp"
#include <implot.h>

namespace shards {
namespace ImGui {
using namespace shards;

struct Base {
  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(gfx::Base::requiredInfo); }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is not used and will pass through."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The output of this shard will be its input."); }
};

struct IDContext {
  IDContext(void *ptr) { ::ImGui::PushID(ptr); }
  ~IDContext() { ::ImGui::PopID(); }
};

struct Style : public Base {
  enum GuiStyle {
    Alpha,
    WindowPadding,
    WindowRounding,
    WindowBorderSize,
    WindowMinSize,
    WindowTitleAlign,
    WindowMenuButtonPosition,
    ChildRounding,
    ChildBorderSize,
    PopupRounding,
    PopupBorderSize,
    FramePadding,
    FrameRounding,
    FrameBorderSize,
    ItemSpacing,
    ItemInnerSpacing,
    TouchExtraPadding,
    IndentSpacing,
    ColumnsMinSpacing,
    ScrollbarSize,
    ScrollbarRounding,
    GrabMinSize,
    GrabRounding,
    TabRounding,
    TabBorderSize,
    ColorButtonPosition,
    ButtonTextAlign,
    SelectableTextAlign,
    DisplayWindowPadding,
    DisplaySafeAreaPadding,
    MouseCursorScale,
    AntiAliasedLines,
    AntiAliasedFill,
    CurveTessellationTol,
    TextColor,
    TextDisabledColor,
    WindowBgColor,
    ChildBgColor,
    PopupBgColor,
    BorderColor,
    BorderShadowColor,
    FrameBgColor,
    FrameBgHoveredColor,
    FrameBgActiveColor,
    TitleBgColor,
    TitleBgActiveColor,
    TitleBgCollapsedColor,
    MenuBarBgColor,
    ScrollbarBgColor,
    ScrollbarGrabColor,
    ScrollbarGrabHoveredColor,
    ScrollbarGrabActiveColor,
    CheckMarkColor,
    SliderGrabColor,
    SliderGrabActiveColor,
    ButtonColor,
    ButtonHoveredColor,
    ButtonActiveColor,
    HeaderColor,
    HeaderHoveredColor,
    HeaderActiveColor,
    SeparatorColor,
    SeparatorHoveredColor,
    SeparatorActiveColor,
    ResizeGripColor,
    ResizeGripHoveredColor,
    ResizeGripActiveColor,
    TabColor,
    TabHoveredColor,
    TabActiveColor,
    TabUnfocusedColor,
    TabUnfocusedActiveColor,
    PlotLinesColor,
    PlotLinesHoveredColor,
    PlotHistogramColor,
    PlotHistogramHoveredColor,
    TextSelectedBgColor,
    DragDropTargetColor,
    NavHighlightColor,
    NavWindowingHighlightColor,
    NavWindowingDimBgColor,
    ModalWindowDimBgColor,
  };
  REGISTER_ENUM(GuiStyle, 'guiS'); // FourCC = 0x67756953

  GuiStyle _key{};

  static inline ParamsInfo paramsInfo = ParamsInfo(ParamsInfo::Param("Style", SHCCSTR("A style key to set."), GuiStyleType));

  static SHParametersInfo parameters() { return SHParametersInfo(paramsInfo); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _key = GuiStyle(value.payload.enumValue);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var::Enum(_key, CoreCC, 'guiS');
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    switch (_key) {
    case WindowRounding:
    case WindowBorderSize:
    case WindowMenuButtonPosition:
    case ChildRounding:
    case ChildBorderSize:
    case PopupRounding:
    case PopupBorderSize:
    case FrameRounding:
    case FrameBorderSize:
    case IndentSpacing:
    case ColumnsMinSpacing:
    case ScrollbarSize:
    case ScrollbarRounding:
    case GrabMinSize:
    case GrabRounding:
    case TabRounding:
    case TabBorderSize:
    case ColorButtonPosition:
    case MouseCursorScale:
    case AntiAliasedLines:
    case AntiAliasedFill:
    case CurveTessellationTol:
    case Alpha:
      if (data.inputType.basicType != Float) {
        throw SHException("this GUI Style shard expected a Float variable as input!");
      }
      break;
    case WindowPadding:
    case WindowMinSize:
    case WindowTitleAlign:
    case FramePadding:
    case ItemSpacing:
    case ItemInnerSpacing:
    case TouchExtraPadding:
    case ButtonTextAlign:
    case SelectableTextAlign:
    case DisplayWindowPadding:
    case DisplaySafeAreaPadding:
      if (data.inputType.basicType != Float2) {
        throw SHException("this GUI Style shard expected a Float2 variable as input!");
      }
      break;
    case TextColor:
    case TextDisabledColor:
    case WindowBgColor:
    case ChildBgColor:
    case PopupBgColor:
    case BorderColor:
    case BorderShadowColor:
    case FrameBgColor:
    case FrameBgHoveredColor:
    case FrameBgActiveColor:
    case TitleBgColor:
    case TitleBgActiveColor:
    case TitleBgCollapsedColor:
    case MenuBarBgColor:
    case ScrollbarBgColor:
    case ScrollbarGrabColor:
    case ScrollbarGrabHoveredColor:
    case ScrollbarGrabActiveColor:
    case CheckMarkColor:
    case SliderGrabColor:
    case SliderGrabActiveColor:
    case ButtonColor:
    case ButtonHoveredColor:
    case ButtonActiveColor:
    case HeaderColor:
    case HeaderHoveredColor:
    case HeaderActiveColor:
    case SeparatorColor:
    case SeparatorHoveredColor:
    case SeparatorActiveColor:
    case ResizeGripColor:
    case ResizeGripHoveredColor:
    case ResizeGripActiveColor:
    case TabColor:
    case TabHoveredColor:
    case TabActiveColor:
    case TabUnfocusedColor:
    case TabUnfocusedActiveColor:
    case PlotLinesColor:
    case PlotLinesHoveredColor:
    case PlotHistogramColor:
    case PlotHistogramHoveredColor:
    case TextSelectedBgColor:
    case DragDropTargetColor:
    case NavHighlightColor:
    case NavWindowingHighlightColor:
    case NavWindowingDimBgColor:
    case ModalWindowDimBgColor:
      if (data.inputType.basicType != Color) {
        throw SHException("this GUI Style shard expected a Color variable as input!");
      }
      break;
    }
    return data.inputType;
  }

  static ImVec2 var2Vec2(const SHVar &input) {
    ImVec2 res;
    res.x = input.payload.float2Value[0];
    res.y = input.payload.float2Value[1];
    return res;
  }

  static ImVec4 color2Vec4(const SHColor &color) {
    ImVec4 res;
    res.x = color.r / 255.0f;
    res.y = color.g / 255.0f;
    res.z = color.b / 255.0f;
    res.w = color.a / 255.0f;
    return res;
  }

  static ImVec4 color2Vec4(const SHVar &input) { return color2Vec4(input.payload.colorValue); }

  static SHColor vec42Color(const ImVec4 &color) {
    SHColor res;
    res.r = roundf(color.x * 255.0f);
    res.g = roundf(color.y * 255.0f);
    res.b = roundf(color.z * 255.0f);
    res.a = roundf(color.w * 255.0f);
    return res;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &style = ::ImGui::GetStyle();

    switch (_key) {
    case Alpha:
      style.Alpha = input.payload.floatValue;
      break;
    case WindowPadding:
      style.WindowPadding = var2Vec2(input);
      break;
    case WindowRounding:
      style.WindowRounding = input.payload.floatValue;
      break;
    case WindowBorderSize:
      style.WindowBorderSize = input.payload.floatValue;
      break;
    case WindowMinSize:
      style.WindowMinSize = var2Vec2(input);
      break;
    case WindowTitleAlign:
      style.WindowTitleAlign = var2Vec2(input);
      break;
    case WindowMenuButtonPosition:
      style.WindowMenuButtonPosition = input.payload.floatValue;
      break;
    case ChildRounding:
      style.ChildRounding = input.payload.floatValue;
      break;
    case ChildBorderSize:
      style.ChildBorderSize = input.payload.floatValue;
      break;
    case PopupRounding:
      style.PopupRounding = input.payload.floatValue;
      break;
    case PopupBorderSize:
      style.PopupBorderSize = input.payload.floatValue;
      break;
    case FramePadding:
      style.FramePadding = var2Vec2(input);
      break;
    case FrameRounding:
      style.FrameRounding = input.payload.floatValue;
      break;
    case FrameBorderSize:
      style.FrameBorderSize = input.payload.floatValue;
      break;
    case ItemSpacing:
      style.ItemSpacing = var2Vec2(input);
      break;
    case ItemInnerSpacing:
      style.ItemInnerSpacing = var2Vec2(input);
      break;
    case TouchExtraPadding:
      style.TouchExtraPadding = var2Vec2(input);
      break;
    case IndentSpacing:
      style.IndentSpacing = input.payload.floatValue;
      break;
    case ColumnsMinSpacing:
      style.ColumnsMinSpacing = input.payload.floatValue;
      break;
    case ScrollbarSize:
      style.ScrollbarSize = input.payload.floatValue;
      break;
    case ScrollbarRounding:
      style.ScrollbarRounding = input.payload.floatValue;
      break;
    case GrabMinSize:
      style.GrabMinSize = input.payload.floatValue;
      break;
    case GrabRounding:
      style.GrabRounding = input.payload.floatValue;
      break;
    case TabRounding:
      style.TabRounding = input.payload.floatValue;
      break;
    case TabBorderSize:
      style.TabBorderSize = input.payload.floatValue;
      break;
    case ColorButtonPosition:
      style.ColorButtonPosition = input.payload.floatValue;
      break;
    case ButtonTextAlign:
      style.ButtonTextAlign = var2Vec2(input);
      break;
    case SelectableTextAlign:
      style.SelectableTextAlign = var2Vec2(input);
      break;
    case DisplayWindowPadding:
      style.DisplayWindowPadding = var2Vec2(input);
      break;
    case DisplaySafeAreaPadding:
      style.DisplaySafeAreaPadding = var2Vec2(input);
      break;
    case MouseCursorScale:
      style.MouseCursorScale = input.payload.floatValue;
      break;
    case AntiAliasedLines:
      style.AntiAliasedLines = input.payload.floatValue;
      break;
    case AntiAliasedFill:
      style.AntiAliasedFill = input.payload.floatValue;
      break;
    case CurveTessellationTol:
      style.CurveTessellationTol = input.payload.floatValue;
      break;
    case TextColor:
      style.Colors[ImGuiCol_Text] = color2Vec4(input);
      break;
    case TextDisabledColor:
      style.Colors[ImGuiCol_TextDisabled] = color2Vec4(input);
      break;
    case WindowBgColor:
      style.Colors[ImGuiCol_WindowBg] = color2Vec4(input);
      break;
    case ChildBgColor:
      style.Colors[ImGuiCol_ChildBg] = color2Vec4(input);
      break;
    case PopupBgColor:
      style.Colors[ImGuiCol_PopupBg] = color2Vec4(input);
      break;
    case BorderColor:
      style.Colors[ImGuiCol_Border] = color2Vec4(input);
      break;
    case BorderShadowColor:
      style.Colors[ImGuiCol_BorderShadow] = color2Vec4(input);
      break;
    case FrameBgColor:
      style.Colors[ImGuiCol_FrameBg] = color2Vec4(input);
      break;
    case FrameBgHoveredColor:
      style.Colors[ImGuiCol_FrameBgHovered] = color2Vec4(input);
      break;
    case FrameBgActiveColor:
      style.Colors[ImGuiCol_FrameBgActive] = color2Vec4(input);
      break;
    case TitleBgColor:
      style.Colors[ImGuiCol_TitleBg] = color2Vec4(input);
      break;
    case TitleBgActiveColor:
      style.Colors[ImGuiCol_TitleBgActive] = color2Vec4(input);
      break;
    case TitleBgCollapsedColor:
      style.Colors[ImGuiCol_TitleBgCollapsed] = color2Vec4(input);
      break;
    case MenuBarBgColor:
      style.Colors[ImGuiCol_MenuBarBg] = color2Vec4(input);
      break;
    case ScrollbarBgColor:
      style.Colors[ImGuiCol_ScrollbarBg] = color2Vec4(input);
      break;
    case ScrollbarGrabColor:
      style.Colors[ImGuiCol_ScrollbarGrab] = color2Vec4(input);
      break;
    case ScrollbarGrabHoveredColor:
      style.Colors[ImGuiCol_ScrollbarGrabHovered] = color2Vec4(input);
      break;
    case ScrollbarGrabActiveColor:
      style.Colors[ImGuiCol_ScrollbarGrabActive] = color2Vec4(input);
      break;
    case CheckMarkColor:
      style.Colors[ImGuiCol_CheckMark] = color2Vec4(input);
      break;
    case SliderGrabColor:
      style.Colors[ImGuiCol_SliderGrab] = color2Vec4(input);
      break;
    case SliderGrabActiveColor:
      style.Colors[ImGuiCol_SliderGrabActive] = color2Vec4(input);
      break;
    case ButtonColor:
      style.Colors[ImGuiCol_Button] = color2Vec4(input);
      break;
    case ButtonHoveredColor:
      style.Colors[ImGuiCol_ButtonHovered] = color2Vec4(input);
      break;
    case ButtonActiveColor:
      style.Colors[ImGuiCol_ButtonActive] = color2Vec4(input);
      break;
    case HeaderColor:
      style.Colors[ImGuiCol_Header] = color2Vec4(input);
      break;
    case HeaderHoveredColor:
      style.Colors[ImGuiCol_HeaderHovered] = color2Vec4(input);
      break;
    case HeaderActiveColor:
      style.Colors[ImGuiCol_HeaderActive] = color2Vec4(input);
      break;
    case SeparatorColor:
      style.Colors[ImGuiCol_Separator] = color2Vec4(input);
      break;
    case SeparatorHoveredColor:
      style.Colors[ImGuiCol_SeparatorHovered] = color2Vec4(input);
      break;
    case SeparatorActiveColor:
      style.Colors[ImGuiCol_SeparatorActive] = color2Vec4(input);
      break;
    case ResizeGripColor:
      style.Colors[ImGuiCol_ResizeGrip] = color2Vec4(input);
      break;
    case ResizeGripHoveredColor:
      style.Colors[ImGuiCol_ResizeGripHovered] = color2Vec4(input);
      break;
    case ResizeGripActiveColor:
      style.Colors[ImGuiCol_ResizeGripActive] = color2Vec4(input);
      break;
    case TabColor:
      style.Colors[ImGuiCol_Tab] = color2Vec4(input);
      break;
    case TabHoveredColor:
      style.Colors[ImGuiCol_TabHovered] = color2Vec4(input);
      break;
    case TabActiveColor:
      style.Colors[ImGuiCol_TabActive] = color2Vec4(input);
      break;
    case TabUnfocusedColor:
      style.Colors[ImGuiCol_TabUnfocused] = color2Vec4(input);
      break;
    case TabUnfocusedActiveColor:
      style.Colors[ImGuiCol_TabUnfocusedActive] = color2Vec4(input);
      break;
    case PlotLinesColor:
      style.Colors[ImGuiCol_PlotLines] = color2Vec4(input);
      break;
    case PlotLinesHoveredColor:
      style.Colors[ImGuiCol_PlotLinesHovered] = color2Vec4(input);
      break;
    case PlotHistogramColor:
      style.Colors[ImGuiCol_PlotHistogram] = color2Vec4(input);
      break;
    case PlotHistogramHoveredColor:
      style.Colors[ImGuiCol_PlotHistogramHovered] = color2Vec4(input);
      break;
    case TextSelectedBgColor:
      style.Colors[ImGuiCol_TextSelectedBg] = color2Vec4(input);
      break;
    case DragDropTargetColor:
      style.Colors[ImGuiCol_DragDropTarget] = color2Vec4(input);
      break;
    case NavHighlightColor:
      style.Colors[ImGuiCol_NavHighlight] = color2Vec4(input);
      break;
    case NavWindowingHighlightColor:
      style.Colors[ImGuiCol_NavWindowingHighlight] = color2Vec4(input);
      break;
    case NavWindowingDimBgColor:
      style.Colors[ImGuiCol_NavWindowingDimBg] = color2Vec4(input);
      break;
    case ModalWindowDimBgColor:
      style.Colors[ImGuiCol_ModalWindowDimBg] = color2Vec4(input);
      break;
    }

    return input;
  }
};

struct Window : public Base {
  shards::ShardsVar _blks{};
  std::string _title;
  bool firstActivation{true};
  Var _pos{}, _width{}, _height{};
  ParamVar _flags{Var::Enum(
      (int(Enums::GuiWindowFlags::NoResize) | int(Enums::GuiWindowFlags::NoMove) | int(Enums::GuiWindowFlags::NoCollapse)),
      CoreCC, Enums::GuiWindowFlagsCC)};
  ParamVar _notClosed{Var::True};
  std::array<SHExposedTypeInfo, 2> _required;

  static SHOptionalString help() { return SHCCSTR("Renders a GUI window within the main container window `GFX.MainWindow`."); }
  static SHOptionalString inputHelp() {
    return SHCCSTR("The value that will be passed to the Contents shards of the rendered window.");
  }

  static inline Parameters _params{
      {"Title", SHCCSTR("The title to be displayed on the title-bar of the rendered window."), {CoreInfo::StringType}},
      {"Pos",
       SHCCSTR("The (x,y) position of the rendered window as pixels (type Int2) or as a fraction of container's position (type "
               "Float2)."),
       {CoreInfo::Int2Type, CoreInfo::Float2Type, CoreInfo::NoneType}},
      {"Width",
       SHCCSTR("The width of the rendered window as pixels (type Int) or as a fraction of container's width (type Float)."),
       {CoreInfo::IntType, CoreInfo::FloatType, CoreInfo::NoneType}},
      {"Height",
       SHCCSTR("The height of the rendered window as pixels (type Int) or as a fraction of container's height (type Float)."),
       {CoreInfo::IntType, CoreInfo::FloatType, CoreInfo::NoneType}},
      {"Contents", SHCCSTR("Code to generate and control the UI elements that will be displayed in the rendered window."),
       CoreInfo::ShardsOrNone},
      {"Flags",
       SHCCSTR("Flags to control the rendered window attributes like menu-bar, title-bar, resize, move, collapse etc. Defaults "
               "are show and allow."),
       {Enums::GuiWindowFlagsType, Enums::GuiWindowFlagsVarType, Enums::GuiWindowFlagsSeqType, Enums::GuiWindowFlagsVarSeqType,
        CoreInfo::NoneType}},
      {"OnClose",
       SHCCSTR("Flag that shows a close [x] button on the rendered window based on an input boolean variable (show if boolean "
               "`true`). Rendered window is hidden and boolean set to `false` if the close button is clicked."),
       {CoreInfo::BoolVarType}},
  };

  static SHParametersInfo parameters() { return _params; }

  SHTypeInfo compose(SHInstanceData &data) {
    _blks.compose(data);
    return data.inputType;
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      break;
    case 1:
      _pos = Var(value);
      break;
    case 2:
      _width = Var(value);
      break;
    case 3:
      _height = Var(value);
      break;
    case 4:
      _blks = value;
      break;
    case 5:
      _flags = value;
      break;
    case 6:
      _notClosed = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_title);
    case 1:
      return _pos;
    case 2:
      return _width;
    case 3:
      return _height;
    case 4:
      return _blks;
    case 5:
      return _flags;
    case 6:
      return _notClosed;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    _blks.cleanup();
    _flags.cleanup();
    _notClosed.cleanup();
  }

  void warmup(SHContext *context) {
    _blks.warmup(context);
    _flags.warmup(context);
    _notClosed.warmup(context);
    firstActivation = true;
  }

  SHExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = gfx::Base::mainWindowGlobalsInfo;
    idx++;

    if (_notClosed.isVariable()) {
      _required[idx].name = _notClosed.variableName();
      _required[idx].help = SHCCSTR("The required OnClose.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    if (!_blks)
      return input;

    auto flags = ::ImGuiWindowFlags_NoSavedSettings | ::ImGuiWindowFlags(shards::getFlags<Enums::GuiWindowFlags>(_flags));

    if (firstActivation) {
      const ImGuiIO &io = ::ImGui::GetIO();

      if (_pos.valueType == Int2) {
        ImVec2 pos = {float(_pos.payload.int2Value[0]), float(_pos.payload.int2Value[1])};
        ::ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
      } else if (_pos.valueType == Float2) {
        const auto x = double(io.DisplaySize.x) * _pos.payload.float2Value[0];
        const auto y = double(io.DisplaySize.y) * _pos.payload.float2Value[1];
        ImVec2 pos = {float(x), float(y)};
        ::ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
      }

      ImVec2 size;
      if (_width.valueType == Int) {
        size.x = float(_width.payload.intValue);
      } else if (_width.valueType == Float) {
        size.x = io.DisplaySize.x * float(_width.payload.floatValue);
      } else {
        size.x = 0.f;
      }
      if (_height.valueType == Int) {
        size.y = float(_height.payload.intValue);
      } else if (_height.valueType == Float) {
        size.y = io.DisplaySize.y * float(_height.payload.floatValue);
      } else {
        size.y = 0.f;
      }
      ::ImGui::SetNextWindowSize(size, ImGuiCond_Always);

      firstActivation = false;
    }

    auto active = ::ImGui::Begin(_title.c_str(), _notClosed.isVariable() ? &_notClosed.get().payload.boolValue : nullptr, flags);
    DEFER(::ImGui::End());
    if (active) {
      SHVar output{};
      _blks.activate(context, input, output);
    }
    return input;
  }
};

struct ChildWindow : public Base {
  shards::ShardsVar _blks{};
  SHVar _width{}, _height{};
  bool _border = false;
  static inline ImGuiID windowIds{0};
  ImGuiID _wndId = ++windowIds;

  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Width", SHCCSTR("The width of the child window to create"), CoreInfo::IntOrNone),
      ParamsInfo::Param("Height", SHCCSTR("The height of the child window to create."), CoreInfo::IntOrNone),
      ParamsInfo::Param("Border", SHCCSTR("If we want to draw a border frame around the child window."), CoreInfo::BoolType),
      ParamsInfo::Param("Contents", SHCCSTR("The inner contents shards."), CoreInfo::ShardsOrNone));

  static SHParametersInfo parameters() { return SHParametersInfo(paramsInfo); }

  SHTypeInfo compose(SHInstanceData &data) {
    _blks.compose(data);
    return data.inputType;
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _width = value;
      break;
    case 1:
      _height = value;
      break;
    case 2:
      _border = value.payload.boolValue;
      break;
    case 3:
      _blks = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _width;
    case 1:
      return _height;
    case 2:
      return Var(_border);
    case 3:
      return _blks;
    default:
      return Var::Empty;
    }
  }

  void cleanup() { _blks.cleanup(); }

  void warmup(SHContext *context) { _blks.warmup(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    if (!_blks)
      return input;

    ImVec2 size{0, 0};
    if (_width.valueType == Int) {
      size.x = float(_width.payload.intValue);
    }

    if (_height.valueType == Int) {
      size.y = float(_height.payload.intValue);
    }

    auto visible = ::ImGui::BeginChild(_wndId, size, _border);
    DEFER(::ImGui::EndChild());
    if (visible) {
      SHVar output{};
      _blks.activate(context, input, output);
    }
    return input;
  }
};

Parameters &VariableParamsInfo() {
  static Parameters params{
      {"Label", SHCCSTR("The label for this widget."), CoreInfo::StringOrNone},
      {"Variable", SHCCSTR("The variable that holds the input value."), {CoreInfo::AnyVarType, CoreInfo::NoneType}},
  };
  return params;
}

template <SHType CT> struct Variable : public Base {
  static inline Type varType{{CT}};

  std::string _label;
  ParamVar _variable{};
  ExposedInfo _expInfo{};
  bool _exposing = false;

  void cleanup() { _variable.cleanup(); }

  void warmup(SHContext *context) { _variable.warmup(context); }

  SHTypeInfo compose(SHInstanceData &data) {
    if (_variable.isVariable()) {
      _exposing = true; // assume we expose a new variable
      // search for a possible existing variable and ensure it's the right type
      for (auto &var : data.shared) {
        if (strcmp(var.name, _variable.variableName()) == 0) {
          // we found a variable, make sure it's the right type and mark
          // exposing off
          _exposing = false;
          if (CT != SHType::Any && var.exposedType.basicType != CT) {
            throw SHException("GUI - Variable: Existing variable type not "
                              "matching the input.");
          }
          // also make sure it's mutable!
          if (!var.isMutable) {
            throw SHException("GUI - Variable: Existing variable is not mutable.");
          }
          break;
        }
      }
    }
    return varType;
  }

  SHExposedTypesInfo requiredVariables() {
    if (_variable.isVariable() && !_exposing) {
      _expInfo = ExposedInfo(
          gfx::Base::mainWindowGlobalsInfo,
          ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The required input variable."), SHTypeInfo(varType)));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  SHExposedTypesInfo exposedVariables() {
    if (_variable.isVariable() > 0 && _exposing) {
      _expInfo = ExposedInfo(
          gfx::Base::mainWindowGlobalsInfo,
          ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The exposed input variable."), SHTypeInfo(varType), true));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  static SHParametersInfo parameters() {
    static SHParametersInfo info = VariableParamsInfo();
    return info;
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1:
      _variable = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _label.size() == 0 ? Var::Empty : Var(_label);
    case 1:
      return _variable;
    default:
      return Var::Empty;
    }
  }
};

template <SHType CT1, SHType CT2> struct Variable2 : public Base {
  static inline Type varType1{{CT1}};
  static inline Type varType2{{CT2}};

  std::string _label;
  ParamVar _variable{};
  ExposedInfo _expInfo{};
  bool _exposing = false;

  void cleanup() { _variable.cleanup(); }

  void warmup(SHContext *context) { _variable.warmup(context); }

  SHTypeInfo compose(SHInstanceData &data) {
    if (_variable.isVariable()) {
      _exposing = true; // assume we expose a new variable
      // search for a possible existing variable and ensure it's the right type
      for (auto &var : data.shared) {
        if (strcmp(var.name, _variable.variableName()) == 0) {
          // we found a variable, make sure it's the right type and mark
          // exposing off
          _exposing = false;
          if (var.exposedType.basicType != CT1 && var.exposedType.basicType != CT2) {
            throw SHException("GUI - Variable: Existing variable type not "
                              "matching the input.");
          }
          // also make sure it's mutable!
          if (!var.isMutable) {
            throw SHException("GUI - Variable: Existing variable is not mutable.");
          }
          break;
        }
      }
    }
    return CoreInfo::AnyType;
  }

  SHExposedTypesInfo requiredVariables() {
    if (_variable.isVariable() && !_exposing) {
      _expInfo = ExposedInfo(
          gfx::Base::mainWindowGlobalsInfo,
          ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The required input variable."), SHTypeInfo(varType1)),
          ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The required input variable."), SHTypeInfo(varType2)));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  SHExposedTypesInfo exposedVariables() {
    if (_variable.isVariable() && _exposing) {
      _expInfo = ExposedInfo(
          gfx::Base::mainWindowGlobalsInfo,
          ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The exposed input variable."), SHTypeInfo(varType1), true),
          ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The exposed input variable."), SHTypeInfo(varType2), true));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  static SHParametersInfo parameters() {
    static SHParametersInfo info = VariableParamsInfo();
    return info;
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1:
      _variable = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _label.size() == 0 ? Var::Empty : Var(_label);
    case 1:
      return _variable;
    default:
      return Var::Empty;
    }
  }
};

struct Checkbox : public Variable<SHType::Bool> {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("A boolean indicating whether the checkbox changed state "
                   "during that frame.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    auto result = Var::False;
    if (_variable.isVariable()) {
      _variable.get().valueType = SHType::Bool;
      if (::ImGui::Checkbox(_label.c_str(), &_variable.get().payload.boolValue)) {
        result = Var::True;
      }
    } else {
      // HACK kinda... we recycle _exposing since we are not using it in this
      // branch
      if (::ImGui::Checkbox(_label.c_str(), &_exposing)) {
        result = Var::True;
      }
    }
    return result;
  }
};

struct CheckboxFlags : public Variable2<SHType::Int, SHType::Enum> {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("A boolean indicating whether the checkbox changed state "
                   "during that frame.");
  }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    if (index < 2)
      Variable2<SHType::Int, SHType::Enum>::setParam(index, value);
    else
      _value = value;
  }

  SHVar getParam(int index) {
    if (index < 2)
      return Variable2<SHType::Int, SHType::Enum>::getParam(index);
    else
      return _value;
  }

  SHTypeInfo compose(SHInstanceData &data) {
    Variable2<SHType::Int, SHType::Enum>::compose(data);

    // ideally here we should check that the type of _value is the same as
    // _variable.

    return CoreInfo::BoolType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    auto result = Var::False;
    switch (_value.valueType) {
    case SHType::Int: {
      int *flags;
      if (_variable.isVariable()) {
        _variable.get().valueType = SHType::Int;
        flags = reinterpret_cast<int *>(&_variable.get().payload.intValue);
      } else {
        flags = reinterpret_cast<int *>(&_tmp.payload.intValue);
      }
      if (::ImGui::CheckboxFlags(_label.c_str(), flags, int(_value.payload.intValue))) {
        result = Var::True;
      }
    } break;
    case SHType::Enum: {
      int *flags;
      if (_variable.isVariable()) {
        _variable.get().valueType = SHType::Enum;
        flags = reinterpret_cast<int *>(&_variable.get().payload.enumValue);
      } else {
        flags = reinterpret_cast<int *>(&_tmp.payload.enumValue);
      }
      if (::ImGui::CheckboxFlags(_label.c_str(), flags, int(_value.payload.enumValue))) {
        result = Var::True;
      }
    } break;
    default:
      break;
    }

    return result;
  }

private:
  static inline Parameters _params{
      VariableParamsInfo(), {{"Value", SHCCSTR("The flag value to set or unset."), {CoreInfo::IntType, CoreInfo::AnyEnumType}}}};

  SHVar _value{};
  SHVar _tmp{};
};

struct RadioButton : public Variable<SHType::Any> {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("A boolean indicating whether the radio button changed "
                   "state during that frame.");
  }

  static SHParametersInfo parameters() { return paramsInfo; }

  void setParam(int index, const SHVar &value) {
    if (index < 2)
      Variable<SHType::Any>::setParam(index, value);
    else
      _value = value;
  }

  SHVar getParam(int index) {
    if (index < 2)
      return Variable<SHType::Any>::getParam(index);
    else
      return _value;
  }

  SHExposedTypesInfo requiredVariables() {
    if (_variable.isVariable() && !_exposing) {
      _expInfo = ExposedInfo(gfx::Base::mainWindowGlobalsInfo,
                             ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The required input variable."),
                                                   SHTypeInfo({_value.valueType})));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  SHExposedTypesInfo exposedVariables() {
    if (_variable.isVariable() > 0 && _exposing) {
      _expInfo = ExposedInfo(gfx::Base::mainWindowGlobalsInfo,
                             ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The exposed input variable."),
                                                   SHTypeInfo({_value.valueType}), true));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    auto result = Var::False;
    if (_variable.isVariable()) {
      auto &var = _variable.get();
      if (::ImGui::RadioButton(_label.c_str(), var == _value)) {
        shards::cloneVar(var, _value);
        result = Var::True;
      }
    } else {
      // HACK kinda... we recycle _exposing since we are not using it in this
      // branch
      if (::ImGui::RadioButton(_label.c_str(), _exposing)) {
        result = Var::True;
      }
    }

    return result;
  }

private:
  static inline Parameters paramsInfo{VariableParamsInfo(),
                                      {{"Value", SHCCSTR("The value to compare with."), {CoreInfo::AnyType}}}};

  SHVar _value{};
};

struct Text : public Base {
  static SHOptionalString help() { return SHCCSTR("Displays the input string as GUI text."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The text/ string to display."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1:
      _color = value;
      break;
    case 2: {
      if (value.valueType == None) {
        _format.clear();
      } else {
        _format = value.payload.stringValue;
      }
    } break;
    case 3:
      _wrap = value.payload.boolValue;
      break;
    case 4:
      _bullet = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _label.size() == 0 ? Var::Empty : Var(_label);
    case 1:
      return _color;
    case 2:
      return _format.size() == 0 ? Var::Empty : Var(_format);
    case 3:
      return Var(_wrap);
    case 4:
      return Var(_bullet);
    default:
      return Var::Empty;
    }
  }

  VarStringStream _text;
  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    _text.write(input);

    if (_color.valueType == Color)
      ::ImGui::PushStyleColor(ImGuiCol_Text, Style::color2Vec4(_color));

    auto format = "%s";
    if (_format.size() > 0) {
      auto pos = _format.find("{}");
      // while (pos != std::string::npos) {
      if (pos != std::string::npos) {
        _format.replace(pos, 2, format);
        // pos = _format.find("{}", pos + 2);
        // TODO support multiple args
      }
      format = _format.c_str();
    }

    if (_wrap)
      ::ImGui::PushTextWrapPos(0.0f);

    if (_bullet)
      ::ImGui::Bullet();

    if (_label.size() > 0) {
      ::ImGui::LabelText(_label.c_str(), format, _text.str());
    } else {
      ::ImGui::Text(format, _text.str());
    }

    if (_wrap)
      ::ImGui::PopTextWrapPos();

    if (_color.valueType == Color)
      ::ImGui::PopStyleColor();

    return input;
  }

private:
  static inline Parameters _params = {
      {"Label", SHCCSTR("Optional label for the displayed string. Prints to the screen."), {CoreInfo::StringOrNone}},
      {"Color", SHCCSTR("Optional color of the displayed text. Default is white."), {CoreInfo::ColorOrNone}},
      {"Format",
       SHCCSTR("Optional string containing a placeholder `{}` for the input string text. The output is a composite string to "
               "display."),
       {CoreInfo::StringOrNone}},
      {"Wrap",
       SHCCSTR("Either wraps the text into the next line (if set to `true`) or truncates it (if set to `false`) when the text "
               "doesn't fit horizontally."),
       {CoreInfo::BoolType}},
      {"Bullet",
       SHCCSTR("Prints the text as a bullet-point (i.e. displays a small circle before the text), if set to `true`."),
       {CoreInfo::BoolType}},
  };

  std::string _label;
  SHVar _color{};
  std::string _format;
  bool _wrap{false};
  bool _bullet{false};
};

struct Bullet : public Base {
  static SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::Bullet();
    return input;
  }
};

struct Button : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("A boolean indicating whether the button was clicked during "
                   "that frame.");
  }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _shards = value;
      break;
    case 2:
      _type = Enums::GuiButton(value.payload.enumValue);
      break;
    case 3:
      _size.x = value.payload.float2Value[0];
      _size.y = value.payload.float2Value[1];
      break;
    case 4:
      _repeat = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _shards;
    case 2:
      return Var::Enum(_type, CoreCC, Enums::GuiButtonCC);
    case 3:
      return Var(_size.x, _size.x);
    case 4:
      return Var(_repeat);
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _shards.compose(data);
    return CoreInfo::BoolType;
  }

  void cleanup() { _shards.cleanup(); }

  void warmup(SHContext *ctx) { _shards.warmup(ctx); }

#define IMBTN_RUN_ACTION                      \
  {                                           \
    SHVar output = Var::Empty;                \
    _shards.activate(context, input, output); \
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    ::ImGui::PushButtonRepeat(_repeat);
    DEFER(::ImGui::PopButtonRepeat());

    auto result = Var::False;
    ImVec2 size;
    switch (_type) {
    case Enums::GuiButton::Normal:
      if (::ImGui::Button(_label.c_str(), _size)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButton::Small:
      if (::ImGui::SmallButton(_label.c_str())) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButton::Invisible:
      if (::ImGui::InvisibleButton(_label.c_str(), _size)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButton::ArrowLeft:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Left)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButton::ArrowRight:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Right)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButton::ArrowUp:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Up)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButton::ArrowDown:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Down)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    }
    return result;
  }

private:
  static inline Parameters _params = {
      {"Label", SHCCSTR("The text label of this button."), {CoreInfo::StringType}},
      {"Action", SHCCSTR("The shards to execute when the button is pressed."), CoreInfo::ShardsOrNone},
      {"Type", SHCCSTR("The button type."), {Enums::GuiButtonType}},
      {"Size", SHCCSTR("The optional size override."), {CoreInfo::Float2Type}},
      {"Repeat", SHCCSTR("Whether to repeat the action while the button is pressed."), {CoreInfo::BoolType}},
  };

  std::string _label;
  ShardsVar _shards{};
  Enums::GuiButton _type{};
  ImVec2 _size = {0, 0};
  bool _repeat{false};
};

struct HexViewer : public Base {
  // TODO use a variable so edits are possible and easy

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value to display in the viewer."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }

  ImGuiExtra::MemoryEditor _editor{};

  // HexViewer() { _editor.ReadOnly = true; }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);
    _editor.DrawContents(input.payload.bytesValue, input.payload.bytesSize);
    return input;
  }
};

struct Dummy : public Base {
  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _width = value;
      break;
    case 1:
      _height = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _width;
    case 1:
      return _height;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    _width.cleanup();
    _height.cleanup();
  }

  void warmup(SHContext *ctx) {
    _width.warmup(ctx);
    _height.warmup(ctx);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto width = float(_width.get().payload.intValue);
    auto height = float(_height.get().payload.intValue);
    ::ImGui::Dummy({width, height});
    return input;
  }

private:
  static inline Parameters _params = {
      {"Width", SHCCSTR("The width of the item."), CoreInfo::IntOrIntVar},
      {"Height", SHCCSTR("The height of the item."), CoreInfo::IntOrIntVar},
  };

  ParamVar _width{Var(0)};
  ParamVar _height{Var(0)};
};

struct NewLine : public Base {
  SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::NewLine();
    return input;
  }
};

struct SameLine : public Base {
  // TODO add offsets and spacing
  SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::SameLine();
    return input;
  }
};

struct Separator : public Base {
  static SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::Separator();
    return input;
  }
};

struct Spacing : public Base {
  static SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::Spacing();
    return input;
  }
};

struct Indent : public Base {
  static SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::Indent();
    return input;
  }
};

struct Unindent : public Base {
  static SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::Unindent();
    return input;
  }
};

struct GetClipboard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The content of the clipboard."); }

  static SHVar activate(SHContext *context, const SHVar &input) {
    auto contents = ::ImGui::GetClipboardText();
    if (contents)
      return Var(contents);
    else
      return Var("");
  }
};

struct SetClipboard : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value to set in the clipboard."); }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  static SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::SetClipboardText(input.payload.stringValue);
    return input;
  }
};

struct TreeNode : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean indicating whether the tree node is open."); }

  static SHParametersInfo parameters() { return _params; }

  SHTypeInfo compose(SHInstanceData &data) {
    _shards.compose(data);
    return CoreInfo::BoolType;
  }

  void cleanup() {
    _shards.cleanup();
    _flags.cleanup();
  }

  void warmup(SHContext *context) {
    _shards.warmup(context);
    _flags.warmup(context);
  }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _shards = value;
      break;
    case 2:
      _defaultOpen = value.payload.boolValue;
      break;
    case 3:
      _flags = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _shards;
    case 2:
      return Var(_defaultOpen);
    case 3:
      return _flags;
    default:
      return SHVar();
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    auto flags = ::ImGuiTreeNodeFlags(shards::getFlags<Enums::GuiTreeNodeFlags>(_flags.get()));

    if (_defaultOpen) {
      flags |= ::ImGuiTreeNodeFlags_DefaultOpen;
    }

    auto visible = ::ImGui::TreeNodeEx(_label.c_str(), flags);
    if (visible) {
      SHVar output{};
      // run inner shards
      _shards.activate(context, input, output);
      // pop the node if was visible
      ::ImGui::TreePop();
    }

    return Var(visible);
  }

private:
  static inline Parameters _params = {
      {"Label", SHCCSTR("The label of this node."), {CoreInfo::StringType}},
      {"Contents", SHCCSTR("The contents of this node."), CoreInfo::ShardsOrNone},
      {"StartOpen", SHCCSTR("If this node should start in the open state."), {CoreInfo::BoolType}},
      {"Flags",
       SHCCSTR("Flags to enable tree node options."),
       {Enums::GuiTreeNodeFlagsType, Enums::GuiTreeNodeFlagsVarType, Enums::GuiTreeNodeFlagsSeqType,
        Enums::GuiTreeNodeFlagsVarSeqType, CoreInfo::NoneType}},
  };

  std::string _label;
  bool _defaultOpen = false;
  ShardsVar _shards;
  ParamVar _flags{};
};

struct CollapsingHeader : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean indicating whether the header is open."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _shards = value;
      break;
    case 2:
      _defaultOpen = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _shards;
    case 2:
      return Var(_defaultOpen);
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _shards.compose(data);
    return CoreInfo::BoolType;
  }

  void cleanup() { _shards.cleanup(); }

  void warmup(SHContext *ctx) { _shards.warmup(ctx); }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (_defaultOpen) {
      ::ImGui::SetNextItemOpen(true);
      _defaultOpen = false;
    }

    auto active = ::ImGui::CollapsingHeader(_label.c_str());
    if (active) {
      SHVar output{};
      _shards.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params = {
      {"Label", SHCCSTR("The label of this node."), {CoreInfo::StringType}},
      {"Contents", SHCCSTR("The contents under this header."), {CoreInfo::ShardsOrNone}},
      {"StartOpen", SHCCSTR("If this header should start in the open state."), {CoreInfo::BoolType}},
  };

  std::string _label;
  ShardsVar _shards{};
  bool _defaultOpen = false;
};

template <SHType SHT> struct DragBase : public Variable<SHT> {
  float _speed{1.0};

  static inline Parameters paramsInfo{VariableParamsInfo(),
                                      {{"Speed", SHCCSTR("The speed multiplier for this drag widget."), CoreInfo::StringOrNone}}};

  static SHParametersInfo parameters() { return paramsInfo; }

  void setParam(int index, const SHVar &value) {
    if (index < 2)
      Variable<SHT>::setParam(index, value);
    else
      _speed = value.payload.floatValue;
  }

  SHVar getParam(int index) {
    if (index < 2)
      return Variable<SHT>::getParam(index);
    else
      return Var(_speed);
  }
};

#define IMGUIDRAG(_SHT_, _T_, _INFO_, _IMT_, _VAL_)                                               \
  struct _SHT_##Drag : public DragBase<SHType::_SHT_> {                                           \
    _T_ _tmp;                                                                                     \
                                                                                                  \
    static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }                                \
    static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }        \
                                                                                                  \
    static SHTypesInfo outputTypes() { return CoreInfo::_INFO_; }                                 \
    static SHOptionalString outputHelp() { return SHCCSTR("The value produced by this shard."); } \
                                                                                                  \
    SHVar activate(SHContext *context, const SHVar &input) {                                      \
      IDContext idCtx(this);                                                                      \
                                                                                                  \
      if (_variable.isVariable()) {                                                               \
        auto &var = _variable.get();                                                              \
        ::ImGui::DragScalar(_label.c_str(), _IMT_, (void *)&var.payload._VAL_, _speed);           \
                                                                                                  \
        var.valueType = SHType::_SHT_;                                                            \
        return var;                                                                               \
      } else {                                                                                    \
        ::ImGui::DragScalar(_label.c_str(), _IMT_, (void *)&_tmp, _speed);                        \
        return Var(_tmp);                                                                         \
      }                                                                                           \
    }                                                                                             \
  }

IMGUIDRAG(Int, int64_t, IntType, ImGuiDataType_S64, intValue);
IMGUIDRAG(Float, double, FloatType, ImGuiDataType_Double, floatValue);

#define IMGUIDRAG2(_SHT_, _T_, _INFO_, _IMT_, _VAL_, _CMP_)                                       \
  struct _SHT_##Drag : public DragBase<SHType::_SHT_> {                                           \
    SHVar _tmp;                                                                                   \
                                                                                                  \
    static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }                                \
    static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }        \
                                                                                                  \
    static SHTypesInfo outputTypes() { return CoreInfo::_INFO_; }                                 \
    static SHOptionalString outputHelp() { return SHCCSTR("The value produced by this shard."); } \
                                                                                                  \
    SHVar activate(SHContext *context, const SHVar &input) {                                      \
      IDContext idCtx(this);                                                                      \
                                                                                                  \
      if (_variable.isVariable()) {                                                               \
        auto &var = _variable.get();                                                              \
        ::ImGui::DragScalarN(_label.c_str(), _IMT_, (void *)&var.payload._VAL_, _CMP_, _speed);   \
                                                                                                  \
        var.valueType = SHType::_SHT_;                                                            \
        return var;                                                                               \
      } else {                                                                                    \
        _tmp.valueType = SHType::_SHT_;                                                           \
        ::ImGui::DragScalarN(_label.c_str(), _IMT_, (void *)&_tmp.payload._VAL_, _CMP_, _speed);  \
        return _tmp;                                                                              \
      }                                                                                           \
    }                                                                                             \
  }

IMGUIDRAG2(Int2, int64_t, Int2Type, ImGuiDataType_S64, int2Value, 2);
IMGUIDRAG2(Int3, int32_t, Int3Type, ImGuiDataType_S32, int3Value, 3);
IMGUIDRAG2(Int4, int32_t, Int4Type, ImGuiDataType_S32, int4Value, 4);
IMGUIDRAG2(Float2, double, Float2Type, ImGuiDataType_Double, float2Value, 2);
IMGUIDRAG2(Float3, float, Float3Type, ImGuiDataType_Float, float3Value, 3);
IMGUIDRAG2(Float4, float, Float4Type, ImGuiDataType_Float, float4Value, 4);

#define IMGUIINPUT(_SHT_, _T_, _IMT_, _VAL_, _FMT_)                                                                \
  struct _SHT_##Input : public Variable<SHType::_SHT_> {                                                           \
    _T_ _tmp;                                                                                                      \
                                                                                                                   \
    static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }                                                 \
    static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }                         \
                                                                                                                   \
    static SHTypesInfo outputTypes() { return CoreInfo::_SHT_##Type; }                                             \
    static SHOptionalString outputHelp() { return SHCCSTR("The value that was input."); }                          \
                                                                                                                   \
    static SHParametersInfo parameters() { return paramsInfo; }                                                    \
                                                                                                                   \
    void cleanup() {                                                                                               \
      _step.cleanup();                                                                                             \
      _stepFast.cleanup();                                                                                         \
      Variable<SHType::_SHT_>::cleanup();                                                                          \
    }                                                                                                              \
                                                                                                                   \
    void warmup(SHContext *context) {                                                                              \
      Variable<SHType::_SHT_>::warmup(context);                                                                    \
      _step.warmup(context);                                                                                       \
      _stepFast.warmup(context);                                                                                   \
    }                                                                                                              \
                                                                                                                   \
    void setParam(int index, const SHVar &value) {                                                                 \
      switch (index) {                                                                                             \
      case 0:                                                                                                      \
      case 1:                                                                                                      \
        Variable<SHType::_SHT_>::setParam(index, value);                                                           \
        break;                                                                                                     \
      case 2:                                                                                                      \
        _step = value;                                                                                             \
        break;                                                                                                     \
      case 3:                                                                                                      \
        _stepFast = value;                                                                                         \
        break;                                                                                                     \
      default:                                                                                                     \
        break;                                                                                                     \
      }                                                                                                            \
    }                                                                                                              \
                                                                                                                   \
    SHVar getParam(int index) {                                                                                    \
      switch (index) {                                                                                             \
      case 0:                                                                                                      \
      case 1:                                                                                                      \
        return Variable<SHType::_SHT_>::getParam(index);                                                           \
      case 2:                                                                                                      \
        return _step;                                                                                              \
      case 3:                                                                                                      \
        return _stepFast;                                                                                          \
      default:                                                                                                     \
        return Var::Empty;                                                                                         \
      }                                                                                                            \
    }                                                                                                              \
                                                                                                                   \
    SHVar activate(SHContext *context, const SHVar &input) {                                                       \
      IDContext idCtx(this);                                                                                       \
                                                                                                                   \
      _T_ step = _step.get().payload._VAL_##Value;                                                                 \
      _T_ step_fast = _stepFast.get().payload._VAL_##Value;                                                        \
      if (_variable.isVariable()) {                                                                                \
        auto &var = _variable.get();                                                                               \
        ::ImGui::InputScalar(_label.c_str(), _IMT_, (void *)&var.payload._VAL_##Value, step > 0 ? &step : nullptr, \
                             step_fast > 0 ? &step_fast : nullptr, _FMT_, 0);                                      \
                                                                                                                   \
        var.valueType = SHType::_SHT_;                                                                             \
        return var;                                                                                                \
      } else {                                                                                                     \
        ::ImGui::InputScalar(_label.c_str(), _IMT_, (void *)&_tmp, step > 0 ? &step : nullptr,                     \
                             step_fast > 0 ? &step_fast : nullptr, _FMT_, 0);                                      \
        return Var(_tmp);                                                                                          \
      }                                                                                                            \
    }                                                                                                              \
                                                                                                                   \
  private:                                                                                                         \
    static inline Parameters paramsInfo{                                                                           \
        VariableParamsInfo(),                                                                                      \
        {{"Step", SHCCSTR("The value of a single increment."), {CoreInfo::_SHT_##Type, CoreInfo::_SHT_##VarType}}, \
         {"StepFast",                                                                                              \
          SHCCSTR("The value of a single increment, when holding Ctrl"),                                           \
          {CoreInfo::_SHT_##Type, CoreInfo::_SHT_##VarType}}},                                                     \
    };                                                                                                             \
                                                                                                                   \
    ParamVar _step{Var((_T_)0)};                                                                                   \
    ParamVar _stepFast{Var((_T_)0)};                                                                               \
  }

IMGUIINPUT(Int, int64_t, ImGuiDataType_S64, int, "%lld");
IMGUIINPUT(Float, double, ImGuiDataType_Double, float, "%.3f");

#define IMGUIINPUT2(_SHT_, _CMP_, _T_, _IMT_, _VAL_, _FMT_)                                                        \
  struct _SHT_##_CMP_##Input : public Variable<SHType::_SHT_##_CMP_> {                                             \
    SHVar _tmp;                                                                                                    \
                                                                                                                   \
    static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }                                                 \
    static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }                         \
                                                                                                                   \
    static SHTypesInfo outputTypes() { return CoreInfo::_SHT_##_CMP_##Type; }                                      \
    static SHOptionalString outputHelp() { return SHCCSTR("The value that was input."); }                          \
                                                                                                                   \
    static SHParametersInfo parameters() { return paramsInfo; }                                                    \
                                                                                                                   \
    void cleanup() {                                                                                               \
      _step.cleanup();                                                                                             \
      _stepFast.cleanup();                                                                                         \
      Variable<SHType::_SHT_##_CMP_>::cleanup();                                                                   \
    }                                                                                                              \
                                                                                                                   \
    void warmup(SHContext *context) {                                                                              \
      Variable<SHType::_SHT_##_CMP_>::warmup(context);                                                             \
      _step.warmup(context);                                                                                       \
      _stepFast.warmup(context);                                                                                   \
    }                                                                                                              \
                                                                                                                   \
    void setParam(int index, const SHVar &value) {                                                                 \
      switch (index) {                                                                                             \
      case 0:                                                                                                      \
      case 1:                                                                                                      \
        Variable<SHType::_SHT_##_CMP_>::setParam(index, value);                                                    \
        break;                                                                                                     \
      case 2:                                                                                                      \
        _step = value;                                                                                             \
        break;                                                                                                     \
      case 3:                                                                                                      \
        _stepFast = value;                                                                                         \
        break;                                                                                                     \
      default:                                                                                                     \
        break;                                                                                                     \
      }                                                                                                            \
    }                                                                                                              \
                                                                                                                   \
    SHVar getParam(int index) {                                                                                    \
      switch (index) {                                                                                             \
      case 0:                                                                                                      \
      case 1:                                                                                                      \
        return Variable<SHType::_SHT_##_CMP_>::getParam(index);                                                    \
      case 2:                                                                                                      \
        return _step;                                                                                              \
      case 3:                                                                                                      \
        return _stepFast;                                                                                          \
      default:                                                                                                     \
        return Var::Empty;                                                                                         \
      }                                                                                                            \
    }                                                                                                              \
                                                                                                                   \
    SHVar activate(SHContext *context, const SHVar &input) {                                                       \
      IDContext idCtx(this);                                                                                       \
                                                                                                                   \
      _T_ step = _step.get().payload._VAL_##Value;                                                                 \
      _T_ step_fast = _stepFast.get().payload._VAL_##Value;                                                        \
      if (_variable.isVariable()) {                                                                                \
        auto &var = _variable.get();                                                                               \
        ::ImGui::InputScalarN(_label.c_str(), _IMT_, (void *)&var.payload._VAL_##_CMP_##Value, _CMP_,              \
                              step > 0 ? &step : nullptr, step_fast > 0 ? &step_fast : nullptr, _FMT_, 0);         \
                                                                                                                   \
        var.valueType = SHType::_SHT_;                                                                             \
        return var;                                                                                                \
      } else {                                                                                                     \
        _tmp.valueType = SHType::_SHT_;                                                                            \
        ::ImGui::InputScalarN(_label.c_str(), _IMT_, (void *)&_tmp.payload._VAL_##_CMP_##Value, _CMP_,             \
                              step > 0 ? &step : nullptr, step_fast > 0 ? &step_fast : nullptr, _FMT_, 0);         \
        return _tmp;                                                                                               \
      }                                                                                                            \
    }                                                                                                              \
                                                                                                                   \
  private:                                                                                                         \
    static inline Parameters paramsInfo{                                                                           \
        VariableParamsInfo(),                                                                                      \
        {{"Step", SHCCSTR("The value of a single increment."), {CoreInfo::_SHT_##Type, CoreInfo::_SHT_##VarType}}, \
         {"StepFast",                                                                                              \
          SHCCSTR("The value of a single increment, when holding Ctrl"),                                           \
          {CoreInfo::_SHT_##Type, CoreInfo::_SHT_##VarType}}},                                                     \
    };                                                                                                             \
                                                                                                                   \
    ParamVar _step{Var((_T_)0)};                                                                                   \
    ParamVar _stepFast{Var((_T_)0)};                                                                               \
  }

IMGUIINPUT2(Int, 2, int64_t, ImGuiDataType_S64, int, "%lld");
IMGUIINPUT2(Int, 3, int32_t, ImGuiDataType_S32, int, "%d");
IMGUIINPUT2(Int, 4, int32_t, ImGuiDataType_S32, int, "%d");

IMGUIINPUT2(Float, 2, double, ImGuiDataType_Double, float, "%.3f");
IMGUIINPUT2(Float, 3, float, ImGuiDataType_Float, float, "%.3f");
IMGUIINPUT2(Float, 4, float, ImGuiDataType_Float, float, "%.3f");

#define IMGUISLIDER(_SHT_, _T_, _IMT_, _VAL_, _FMT_)                                                           \
  struct _SHT_##Slider : public Variable<SHType::_SHT_> {                                                      \
                                                                                                               \
    static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }                                             \
    static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }                     \
                                                                                                               \
    static SHTypesInfo outputTypes() { return CoreInfo::_SHT_##Type; }                                         \
    static SHOptionalString outputHelp() { return SHCCSTR("The value produced by this shard."); }              \
                                                                                                               \
    static SHParametersInfo parameters() { return paramsInfo; }                                                \
                                                                                                               \
    void cleanup() {                                                                                           \
      _min.cleanup();                                                                                          \
      _max.cleanup();                                                                                          \
      Variable<SHType::_SHT_>::cleanup();                                                                      \
    }                                                                                                          \
                                                                                                               \
    void warmup(SHContext *context) {                                                                          \
      Variable<SHType::_SHT_>::warmup(context);                                                                \
      _min.warmup(context);                                                                                    \
      _max.warmup(context);                                                                                    \
    }                                                                                                          \
                                                                                                               \
    void setParam(int index, const SHVar &value) {                                                             \
      switch (index) {                                                                                         \
      case 0:                                                                                                  \
      case 1:                                                                                                  \
        Variable<SHType::_SHT_>::setParam(index, value);                                                       \
        break;                                                                                                 \
      case 2:                                                                                                  \
        _min = value;                                                                                          \
        break;                                                                                                 \
      case 3:                                                                                                  \
        _max = value;                                                                                          \
        break;                                                                                                 \
      default:                                                                                                 \
        break;                                                                                                 \
      }                                                                                                        \
    }                                                                                                          \
                                                                                                               \
    SHVar getParam(int index) {                                                                                \
      switch (index) {                                                                                         \
      case 0:                                                                                                  \
      case 1:                                                                                                  \
        return Variable<SHType::_SHT_>::getParam(index);                                                       \
      case 2:                                                                                                  \
        return _min;                                                                                           \
      case 3:                                                                                                  \
        return _max;                                                                                           \
      default:                                                                                                 \
        return Var::Empty;                                                                                     \
      }                                                                                                        \
    }                                                                                                          \
                                                                                                               \
    SHVar activate(SHContext *context, const SHVar &input) {                                                   \
      IDContext idCtx(this);                                                                                   \
                                                                                                               \
      _T_ min = _min.get().payload._VAL_##Value;                                                               \
      _T_ max = _max.get().payload._VAL_##Value;                                                               \
      if (_variable.isVariable()) {                                                                            \
        auto &var = _variable.get();                                                                           \
        ::ImGui::SliderScalar(_label.c_str(), _IMT_, (void *)&var.payload._VAL_##Value, &min, &max, _FMT_, 0); \
                                                                                                               \
        var.valueType = SHType::_SHT_;                                                                         \
        return var;                                                                                            \
      } else {                                                                                                 \
        ::ImGui::SliderScalar(_label.c_str(), _IMT_, (void *)&_tmp, &min, &max, _FMT_, 0);                     \
        return Var(_tmp);                                                                                      \
      }                                                                                                        \
    }                                                                                                          \
                                                                                                               \
  private:                                                                                                     \
    static inline Parameters paramsInfo{                                                                       \
        VariableParamsInfo(),                                                                                  \
        {{"Min", SHCCSTR("The minimum value."), {CoreInfo::_SHT_##Type, CoreInfo::_SHT_##VarType}},            \
         {"Max", SHCCSTR("The maximum value."), {CoreInfo::_SHT_##Type, CoreInfo::_SHT_##VarType}}},           \
    };                                                                                                         \
                                                                                                               \
    ParamVar _min{Var((_T_)0)};                                                                                \
    ParamVar _max{Var((_T_)100)};                                                                              \
    _T_ _tmp;                                                                                                  \
  }

IMGUISLIDER(Int, int64_t, ImGuiDataType_S64, int, "%lld");
IMGUISLIDER(Float, double, ImGuiDataType_Double, float, "%.3f");

#define IMGUISLIDER2(_SHT_, _CMP_, _T_, _IMT_, _VAL_, _FMT_)                                                                  \
  struct _SHT_##_CMP_##Slider : public Variable<SHType::_SHT_##_CMP_> {                                                       \
                                                                                                                              \
    static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }                                                            \
    static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }                                    \
                                                                                                                              \
    static SHTypesInfo outputTypes() { return CoreInfo::_SHT_##_CMP_##Type; }                                                 \
    static SHOptionalString outputHelp() { return SHCCSTR("The value produced by this shard."); }                             \
                                                                                                                              \
    static SHParametersInfo parameters() { return paramsInfo; }                                                               \
                                                                                                                              \
    void cleanup() {                                                                                                          \
      _min.cleanup();                                                                                                         \
      _max.cleanup();                                                                                                         \
      Variable<SHType::_SHT_##_CMP_>::cleanup();                                                                              \
    }                                                                                                                         \
                                                                                                                              \
    void warmup(SHContext *context) {                                                                                         \
      Variable<SHType::_SHT_##_CMP_>::warmup(context);                                                                        \
      _min.warmup(context);                                                                                                   \
      _max.warmup(context);                                                                                                   \
    }                                                                                                                         \
                                                                                                                              \
    void setParam(int index, const SHVar &value) {                                                                            \
      switch (index) {                                                                                                        \
      case 0:                                                                                                                 \
      case 1:                                                                                                                 \
        Variable<SHType::_SHT_##_CMP_>::setParam(index, value);                                                               \
        break;                                                                                                                \
      case 2:                                                                                                                 \
        _min = value;                                                                                                         \
        break;                                                                                                                \
      case 3:                                                                                                                 \
        _max = value;                                                                                                         \
        break;                                                                                                                \
      default:                                                                                                                \
        break;                                                                                                                \
      }                                                                                                                       \
    }                                                                                                                         \
                                                                                                                              \
    SHVar getParam(int index) {                                                                                               \
      switch (index) {                                                                                                        \
      case 0:                                                                                                                 \
      case 1:                                                                                                                 \
        return Variable<SHType::_SHT_##_CMP_>::getParam(index);                                                               \
      case 2:                                                                                                                 \
        return _min;                                                                                                          \
      case 3:                                                                                                                 \
        return _max;                                                                                                          \
      default:                                                                                                                \
        return Var::Empty;                                                                                                    \
      }                                                                                                                       \
    }                                                                                                                         \
                                                                                                                              \
    SHVar activate(SHContext *context, const SHVar &input) {                                                                  \
      IDContext idCtx(this);                                                                                                  \
                                                                                                                              \
      _T_ min = _min.get().payload._VAL_##Value;                                                                              \
      _T_ max = _max.get().payload._VAL_##Value;                                                                              \
      if (_variable.isVariable()) {                                                                                           \
        auto &var = _variable.get();                                                                                          \
        ::ImGui::SliderScalarN(_label.c_str(), _IMT_, (void *)&var.payload._VAL_##_CMP_##Value, _CMP_, &min, &max, _FMT_, 0); \
                                                                                                                              \
        var.valueType = SHType::_SHT_;                                                                                        \
        return var;                                                                                                           \
      } else {                                                                                                                \
        ::ImGui::SliderScalarN(_label.c_str(), _IMT_, (void *)&_tmp, _CMP_, &min, &max, _FMT_, 0);                            \
        return Var(_tmp);                                                                                                     \
      }                                                                                                                       \
    }                                                                                                                         \
                                                                                                                              \
  private:                                                                                                                    \
    static inline Parameters paramsInfo{                                                                                      \
        VariableParamsInfo(),                                                                                                 \
        {{"Min", SHCCSTR("The minimum value."), {CoreInfo::_SHT_##Type, CoreInfo::_SHT_##VarType}},                           \
         {"Max", SHCCSTR("The maximum value."), {CoreInfo::_SHT_##Type, CoreInfo::_SHT_##VarType}}},                          \
    };                                                                                                                        \
                                                                                                                              \
    ParamVar _min{Var((_T_)0)};                                                                                               \
    ParamVar _max{Var((_T_)100)};                                                                                             \
    _T_ _tmp;                                                                                                                 \
  }

IMGUISLIDER2(Int, 2, int64_t, ImGuiDataType_S64, int, "%lld");
IMGUISLIDER2(Int, 3, int32_t, ImGuiDataType_S32, int, "%lld");
IMGUISLIDER2(Int, 4, int32_t, ImGuiDataType_S32, int, "%lld");

IMGUISLIDER2(Float, 2, double, ImGuiDataType_Double, float, "%.3f");
IMGUISLIDER2(Float, 3, float, ImGuiDataType_Float, float, "%.3f");
IMGUISLIDER2(Float, 4, float, ImGuiDataType_Float, float, "%.3f");

struct TextInput : public Variable<SHType::String> {

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The string that was input."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
    case 1:
      Variable<SHType::String>::setParam(index, value);
      break;
    case 2: {
      if (value.valueType == None) {
        _hint.clear();
      } else {
        _hint = value.payload.stringValue;
      }
    } break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
    case 1:
      return Variable<SHType::String>::getParam(index);
    case 2:
      return Var(_hint);
    default:
      return Var::Empty;
    }
  }

  static int InputTextCallback(ImGuiInputTextCallbackData *data) {
    TextInput *it = (TextInput *)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
      // Resize string callback
      if (it->_variable.isVariable()) {
        auto &var = it->_variable.get();
        delete[] var.payload.stringValue;
        var.payload.stringCapacity = data->BufTextLen * 2;
        var.payload.stringValue = new char[var.payload.stringCapacity];
        data->Buf = (char *)var.payload.stringValue;
      } else {
        it->_buffer.resize(data->BufTextLen * 2);
        data->Buf = (char *)it->_buffer.c_str();
      }
    }
    return 0;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    if (_exposing && _init) {
      _init = false;
      auto &var = _variable.get();
      // we own the variable so let's run some init
      var.valueType = SHType::String;
      var.payload.stringValue = new char[32];
      var.payload.stringCapacity = 32;
      memset((void *)var.payload.stringValue, 0x0, 32);
    }

    auto *hint = _hint.size() > 0 ? _hint.c_str() : nullptr;
    if (_variable.isVariable()) {
      auto &var = _variable.get();
      ::ImGui::InputTextWithHint(_label.c_str(), hint, (char *)var.payload.stringValue, var.payload.stringCapacity,
                                 ImGuiInputTextFlags_CallbackResize, &InputTextCallback, this);
      return var;
    } else {
      ::ImGui::InputTextWithHint(_label.c_str(), hint, (char *)_buffer.c_str(), _buffer.capacity() + 1,
                                 ImGuiInputTextFlags_CallbackResize, &InputTextCallback, this);
      return Var(_buffer);
    }
  }

private:
  static inline Parameters _params{
      VariableParamsInfo(),
      {{"Hint", SHCCSTR("A hint text displayed when the control is empty."), {CoreInfo::StringType}}},
  };

  // fallback, used only when no variable name is set
  std::string _buffer;
  std::string _hint;
  bool _init{true};
};

struct ColorInput : public Variable<SHType::Color> {
  ImVec4 _lcolor{0.0, 0.0, 0.0, 1.0};

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::ColorType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The color that was input."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    if (_exposing && _init) {
      _init = false;
      auto &var = _variable.get();
      // we own the variable so let's run some init
      var.valueType = SHType::Color;
      var.payload.colorValue.r = 0;
      var.payload.colorValue.g = 0;
      var.payload.colorValue.b = 0;
      var.payload.colorValue.a = 255;
    }

    if (_variable.isVariable()) {
      auto &var = _variable.get();
      auto fc = Style::color2Vec4(var);
      ::ImGui::ColorEdit4(_label.c_str(), &fc.x);
      var.payload.colorValue = Style::vec42Color(fc);
      return var;
    } else {
      ::ImGui::ColorEdit4(_label.c_str(), &_lcolor.x);
      return Var(Style::vec42Color(_lcolor));
    }
  }

private:
  bool _init{true};
};

#if 0
struct Image : public Base {
  ImVec2 _size{1.0, 1.0};
  bool _trueSize = false;

  static SHTypesInfo inputTypes() { return BGFX::Texture::ObjType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A texture object."); }

  static SHTypesInfo outputTypes() { return BGFX::Texture::ObjType; }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Size", SHCCSTR("The drawing size of the image."),
                        CoreInfo::Float2Type),
      ParamsInfo::Param("TrueSize",
                        SHCCSTR("If the given size is in true image pixels."),
                        CoreInfo::BoolType));

  static SHParametersInfo parameters() { return SHParametersInfo(paramsInfo); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _size.x = value.payload.float2Value[0];
      _size.y = value.payload.float2Value[1];
      break;
    case 1:
      _trueSize = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_size.x, _size.y);
    case 1:
      return Var(_trueSize);
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    auto texture = reinterpret_cast<BGFX::Texture *>(input.payload.objectValue);

    if (!_trueSize) {
      ImVec2 size = _size;
      size.x *= texture->width;
      size.y *= texture->height;
      ::ImGui::Image(reinterpret_cast<ImTextureID>(texture->handle.idx), size);
    } else {
      ::ImGui::Image(reinterpret_cast<ImTextureID>(texture->handle.idx), _size);
    }
    return input;
  }
};
#endif

struct PlotContext {
  PlotContext() {
    context = ImPlot::CreateContext();
    ImPlot::SetCurrentContext(context);
  }

  ~PlotContext() {
    ImPlot::SetCurrentContext(nullptr);
    ImPlot::DestroyContext(context);
  }

private:
  ImPlotContext *context{nullptr};
};

struct Plot : public Base {
  Shared<PlotContext> _context{};

  static inline Types Plottable{{CoreInfo::FloatSeqType, CoreInfo::Float2SeqType}};

  ShardsVar _shards;
  SHVar _width{}, _height{};
  std::string _title;
  std::string _fullTitle{"##" + std::to_string(reinterpret_cast<uintptr_t>(this))};
  std::string _xlabel;
  std::string _ylabel;
  ParamVar _xlimits{}, _ylimits{};
  ParamVar _lockx{Var::False}, _locky{Var::False};
  std::array<SHExposedTypeInfo, 5> _required;

  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static inline Parameters params{
      {"Title", SHCCSTR("The title of the plot to create."), {CoreInfo::StringType}},
      {"Contents", SHCCSTR("The shards describing this plot."), {CoreInfo::ShardsOrNone}},
      {"Width", SHCCSTR("The width of the plot area to create."), {CoreInfo::IntOrNone}},
      {"Height", SHCCSTR("The height of the plot area to create."), {CoreInfo::IntOrNone}},
      {"X_Label", SHCCSTR("The X axis label."), {CoreInfo::StringType}},
      {"Y_Label", SHCCSTR("The Y axis label."), {CoreInfo::StringType}},
      {"X_Limits", SHCCSTR("The X axis limits."), {CoreInfo::NoneType, CoreInfo::Float2Type, CoreInfo::Float2VarType}},
      {"Y_Limits", SHCCSTR("The Y axis limits."), {CoreInfo::NoneType, CoreInfo::Float2Type, CoreInfo::Float2VarType}},
      {"Lock_X", SHCCSTR("If the X axis should be locked into its limits."), {CoreInfo::BoolType, CoreInfo::BoolVarType}},
      {"Lock_Y", SHCCSTR("If the Y axis should be locked into its limits."), {CoreInfo::BoolType, CoreInfo::BoolVarType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      _fullTitle = _title + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
      break;
    case 1:
      _shards = value;
      break;
    case 2:
      _width = value;
      break;
    case 3:
      _height = value;
      break;
    case 4:
      _xlabel = value.payload.stringValue;
      break;
    case 5:
      _ylabel = value.payload.stringValue;
      break;
    case 6:
      _xlimits = value;
      break;
    case 7:
      _ylimits = value;
      break;
    case 8:
      _lockx = value;
      break;
    case 9:
      _locky = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_title);
    case 1:
      return _shards;
    case 2:
      return _width;
    case 3:
      return _height;
    case 4:
      return Var(_xlabel);
    case 5:
      return Var(_ylabel);
    case 6:
      return _xlimits;
    case 7:
      return _ylimits;
    case 8:
      return _lockx;
    case 9:
      return _locky;
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _shards.compose(data);
    return data.inputType;
  }

  void cleanup() {
    _shards.cleanup();
    _xlimits.cleanup();
    _ylimits.cleanup();
    _lockx.cleanup();
    _locky.cleanup();
  }

  void warmup(SHContext *context) {
    _context();
    _shards.warmup(context);
    _xlimits.warmup(context);
    _ylimits.warmup(context);
    _lockx.warmup(context);
    _locky.warmup(context);
  }

  SHExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = gfx::Base::mainWindowGlobalsInfo;
    idx++;

    if (_xlimits.isVariable()) {
      _required[idx].name = _xlimits.variableName();
      _required[idx].help = SHCCSTR("The required X axis limits.");
      _required[idx].exposedType = CoreInfo::Float2Type;
      idx++;
    }

    if (_ylimits.isVariable()) {
      _required[idx].name = _ylimits.variableName();
      _required[idx].help = SHCCSTR("The required Y axis limits.");
      _required[idx].exposedType = CoreInfo::Float2Type;
      idx++;
    }

    if (_lockx.isVariable()) {
      _required[idx].name = _lockx.variableName();
      _required[idx].help = SHCCSTR("The required X axis locking.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    if (_locky.isVariable()) {
      _required[idx].name = _locky.variableName();
      _required[idx].help = SHCCSTR("The required Y axis locking.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (_xlimits.get().valueType == Float2) {
      auto limitx = _xlimits.get().payload.float2Value[0];
      auto limity = _xlimits.get().payload.float2Value[1];
      auto locked = _lockx.get().payload.boolValue;
      ImPlot::SetupAxisLimits(ImAxis_X1, limitx, limity, locked ? ImGuiCond_Always : ImGuiCond_Once);
    }

    if (_ylimits.get().valueType == Float2) {
      auto limitx = _ylimits.get().payload.float2Value[0];
      auto limity = _ylimits.get().payload.float2Value[1];
      auto locked = _locky.get().payload.boolValue;
      ImPlot::SetupAxisLimits(ImAxis_Y1, limitx, limity, locked ? ImGuiCond_Always : ImGuiCond_Once);
    }

    ImVec2 size{0, 0};
    if (_width.valueType == Int) {
      size.x = float(_width.payload.intValue);
    }
    if (_height.valueType == Int) {
      size.y = float(_height.payload.intValue);
    }

    if (ImPlot::BeginPlot(_fullTitle.c_str(), _xlabel.size() > 0 ? _xlabel.c_str() : nullptr,
                          _ylabel.size() > 0 ? _ylabel.c_str() : nullptr, size)) {
      DEFER(ImPlot::EndPlot());

      SHVar output{};
      _shards.activate(context, input, output);
    }

    return input;
  }
};

struct PlottableBase : public Base {
  std::string _label;
  std::string _fullLabel{"##" + std::to_string(reinterpret_cast<uintptr_t>(this))};
  enum class Kind { unknown, xAndIndex, xAndY };
  Kind _kind{};
  SHVar _color{};

  static SHTypesInfo inputTypes() { return Plot::Plottable; }
  static SHOptionalString inputHelp() { return SHCCSTR("A sequence of values."); }

  static SHTypesInfo outputTypes() { return Plot::Plottable; }

  static constexpr int nparams = 2;
  static inline Parameters params{{"Label", SHCCSTR("The plot's label."), {CoreInfo::StringType}},
                                  {"Color", SHCCSTR("The plot's color."), {CoreInfo::NoneType, CoreInfo::ColorType}}};
  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      _fullLabel = _label + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
      break;
    case 1:
      _color = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _color;
    default:
      return Var::Empty;
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    assert(data.inputType.basicType == Seq);
    if (data.inputType.seqTypes.len != 1 ||
        (data.inputType.seqTypes.elements[0].basicType != Float && data.inputType.seqTypes.elements[0].basicType != Float2)) {
      throw ComposeError("Expected either a Float or Float2 sequence.");
    }

    if (data.inputType.seqTypes.elements[0].basicType == Float)
      _kind = Kind::xAndIndex;
    else
      _kind = Kind::xAndY;

    return data.inputType;
  }

  void applyModifiers() {
    if (_color.valueType == SHType::Color) {
      ImPlot::PushStyleColor(ImPlotCol_Line, Style::color2Vec4(_color.payload.colorValue));
      ImPlot::PushStyleColor(ImPlotCol_Fill, Style::color2Vec4(_color.payload.colorValue));
    }
  }

  void popModifiers() {
    if (_color.valueType == SHType::Color) {
      ImPlot::PopStyleColor(2);
    }
  }

  static bool isInputValid(const SHVar &input) {
    // prevent division by zero
    return !(input.valueType == Seq && input.payload.seqValue.len == 0);
  }
};

struct PlotLine : public PlottableBase {
  SHVar activate(SHContext *context, const SHVar &input) {
    if (!isInputValid(input))
      return input;

    PlottableBase::applyModifiers();
    DEFER(PlottableBase::popModifiers());
    if (_kind == Kind::xAndY) {
      ImPlot::PlotLineG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<SHVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(seq.elements[idx].payload.float2Value[0], seq.elements[idx].payload.float2Value[1]);
          },
          (void *)&input, int(input.payload.seqValue.len));
    } else if (_kind == Kind::xAndIndex) {
      ImPlot::PlotLineG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<SHVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(double(idx), seq.elements[idx].payload.floatValue);
          },
          (void *)&input, int(input.payload.seqValue.len));
    }
    return input;
  }
};

struct PlotDigital : public PlottableBase {
  SHVar activate(SHContext *context, const SHVar &input) {
    if (!isInputValid(input))
      return input;

    PlottableBase::applyModifiers();
    DEFER(PlottableBase::popModifiers());
    if (_kind == Kind::xAndY) {
      ImPlot::PlotDigitalG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<SHVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(seq.elements[idx].payload.float2Value[0], seq.elements[idx].payload.float2Value[1]);
          },
          (void *)&input, int(input.payload.seqValue.len));
    } else if (_kind == Kind::xAndIndex) {
      ImPlot::PlotDigitalG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<SHVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(double(idx), seq.elements[idx].payload.floatValue);
          },
          (void *)&input, int(input.payload.seqValue.len));
    }
    return input;
  }
};

struct PlotScatter : public PlottableBase {
  SHVar activate(SHContext *context, const SHVar &input) {
    if (!isInputValid(input))
      return input;

    PlottableBase::applyModifiers();
    DEFER(PlottableBase::popModifiers());
    if (_kind == Kind::xAndY) {
      ImPlot::PlotScatterG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<SHVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(seq.elements[idx].payload.float2Value[0], seq.elements[idx].payload.float2Value[1]);
          },
          (void *)&input, int(input.payload.seqValue.len));
    } else if (_kind == Kind::xAndIndex) {
      ImPlot::PlotScatterG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<SHVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(double(idx), seq.elements[idx].payload.floatValue);
          },
          (void *)&input, int(input.payload.seqValue.len));
    }
    return input;
  }
};

struct PlotBars : public PlottableBase {
  typedef void (*PlotBarsProc)(const char *label_id, ImPlotGetter, void *data, int count, double bar_width);

  double _width = 0.67;
  bool _horizontal = false;
  PlotBarsProc _plot = &ImPlot::PlotBarsG;

  static inline Parameters params{
      PlottableBase::params,
      {{"Width", SHCCSTR("The width of each bar"), {CoreInfo::FloatType}},
       {"Horizontal", SHCCSTR("If the bar should be horiziontal rather than vertical"), {CoreInfo::BoolType}}},
  };

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case PlottableBase::nparams + 1:
      _width = value.payload.floatValue;
      break;
    case PlottableBase::nparams + 2:
      _horizontal = value.payload.boolValue;
      if (_horizontal) {
        _plot = &ImPlot::PlotBarsHG;
      } else {
        _plot = &ImPlot::PlotBarsG;
      }
      break;
    default:
      PlottableBase::setParam(index, value);
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case PlottableBase::nparams + 1:
      return Var(_width);
    case PlottableBase::nparams + 2:
      return Var(_horizontal);
      break;
    default:
      return PlottableBase::getParam(index);
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!isInputValid(input))
      return input;

    PlottableBase::applyModifiers();
    DEFER(PlottableBase::popModifiers());
    if (_kind == Kind::xAndY) {
      _plot(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<SHVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(seq.elements[idx].payload.float2Value[0], seq.elements[idx].payload.float2Value[1]);
          },
          (void *)&input, int(input.payload.seqValue.len), _width);
    } else if (_kind == Kind::xAndIndex) {
      _plot(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<SHVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(double(idx), seq.elements[idx].payload.floatValue);
          },
          (void *)&input, int(input.payload.seqValue.len), _width);
    }
    return input;
  }
};

struct HasPointer : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean."); }

  static SHVar activate(SHContext *context, const SHVar &input) { return Var(::ImGui::IsAnyItemHovered()); }
};

struct FPS : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The current framerate."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    ImGuiIO &io = ::ImGui::GetIO();
    return Var(io.Framerate);
  }
};

struct Tooltip : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _shards = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _shards;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { _shards.cleanup(); }

  void warmup(SHContext *context) { _shards.warmup(context); }

  SHTypeInfo compose(SHInstanceData &data) {
    _shards.compose(data);
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (::ImGui::IsItemHovered()) {
      ::ImGui::BeginTooltip();
      DEFER(::ImGui::EndTooltip());

      SHVar output{};
      _shards.activate(context, input, output);
    }
    return input;
  }

private:
  static inline Parameters _params = {
      {"Contents", SHCCSTR("The inner contents shards."), {CoreInfo::ShardsOrNone}},
  };

  ShardsVar _shards{};
};

struct HelpMarker : public Base {

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _desc.clear();
      } else {
        _desc = value.payload.stringValue;
      }
    } break;
    case 1:
      _isInline = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _desc.size() == 0 ? Var::Empty : Var(_desc);
    case 1:
      return Var(_isInline);
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (_isInline)
      ::ImGui::SameLine();
    ::ImGui::TextDisabled("(?)");
    if (::ImGui::IsItemHovered()) {
      ::ImGui::BeginTooltip();
      ::ImGui::PushTextWrapPos(::ImGui::GetFontSize() * 35.0f);
      ::ImGui::TextUnformatted(_desc.c_str());
      ::ImGui::PopTextWrapPos();
      ::ImGui::EndTooltip();
    }

    return input;
  }

private:
  static inline Parameters _params = {
      {"Description", SHCCSTR("The text displayed in a popup."), {CoreInfo::StringType}},
      {"Inline", SHCCSTR("Display on the same line."), {CoreInfo::BoolType}},
  };

  std::string _desc;
  bool _isInline{true};
};

struct ProgressBar : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The value to display."); }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _overlay.clear();
      } else {
        _overlay = value.payload.stringValue;
      }
    } break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _overlay.size() == 0 ? Var::Empty : Var(_overlay);
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto buf = _overlay.size() > 0 ? _overlay.c_str() : nullptr;
    ::ImGui::ProgressBar(input.payload.floatValue, ImVec2(0.f, 0.f), buf);
    return input;
  }

private:
  static inline Parameters _params = {
      {"Overlay", SHCCSTR("The text displayed inside the progress bar."), {CoreInfo::StringType}},
  };

  std::string _overlay;
};

struct MenuBase : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean indicating whether the menu is visible."); }

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      shards = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return shards;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { shards.cleanup(); }

  void warmup(SHContext *context) { shards.warmup(context); }

  SHTypeInfo compose(SHInstanceData &data) {
    shards.compose(data);
    return CoreInfo::BoolType;
  }

protected:
  static inline Parameters params = {
      {"Contents", SHCCSTR("The inner contents shards."), CoreInfo::ShardsOrNone},
  };

  ShardsVar shards{};
};

struct MainMenuBar : public MenuBase {
  SHVar activate(SHContext *context, const SHVar &input) {
    auto active = ::ImGui::BeginMainMenuBar();
    if (active) {
      DEFER(::ImGui::EndMainMenuBar());
      SHVar output{};
      shards.activate(context, input, output);
    }
    return Var(active);
  }
};

struct MenuBar : public MenuBase {
  SHVar activate(SHContext *context, const SHVar &input) {
    auto active = ::ImGui::BeginMenuBar();
    if (active) {
      DEFER(::ImGui::EndMenuBar());
      SHVar output{};
      shards.activate(context, input, output);
    }
    return Var(active);
  }
};

struct Menu : public MenuBase {
  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1:
      _isEnabled = value;
      break;
    default:
      MenuBase::setParam(index - 2, value);
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _isEnabled;
    default:
      return MenuBase::getParam(index - 2);
    }
  }

  void cleanup() {
    _isEnabled.cleanup();

    MenuBase::cleanup();
  }

  void warmup(SHContext *context) {
    MenuBase::warmup(context);

    _isEnabled.warmup(context);
  }

  SHExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = gfx::Base::mainWindowGlobalsInfo;
    idx++;

    if (_isEnabled.isVariable()) {
      _required[idx].name = _isEnabled.variableName();
      _required[idx].help = SHCCSTR("The required IsEnabled.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto active = ::ImGui::BeginMenu(_label.c_str(), _isEnabled.get().payload.boolValue);
    if (active) {
      DEFER(::ImGui::EndMenu());
      SHVar output{};
      shards.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params{{{"Label", SHCCSTR("The label of the menu"), {CoreInfo::StringType}},
                                    {"IsEnabled",
                                     SHCCSTR("Sets whether this menu is enabled. A disabled item cannot be "
                                             "selected or clicked."),
                                     {CoreInfo::BoolType, CoreInfo::BoolVarType}}},
                                   MenuBase::params};

  std::string _label;
  ParamVar _isEnabled{Var::True};
  std::array<SHExposedTypeInfo, 2> _required;
};

struct MenuItem : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Action shards."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1:
      _isChecked = value;
      break;
    case 2:
      _action = value;
      break;
    case 3: {
      if (value.valueType == None) {
        _shortcut.clear();
      } else {
        _shortcut = value.payload.stringValue;
      }
    } break;
    case 4:
      _isEnabled = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _label.size() == 0 ? Var::Empty : Var(_label);
    case 1:
      return _isChecked;
    case 2:
      return _action;
    case 3:
      return _shortcut.size() == 0 ? Var::Empty : Var(_shortcut);
    case 4:
      return _isEnabled;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    _action.cleanup();
    _isEnabled.cleanup();
    _isChecked.cleanup();
  }

  void warmup(SHContext *context) {
    _action.warmup(context);
    _isEnabled.warmup(context);
    _isChecked.warmup(context);
  }

  SHExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = gfx::Base::mainWindowGlobalsInfo;
    idx++;

    if (_isChecked.isVariable()) {
      _required[idx].name = _isChecked.variableName();
      _required[idx].help = SHCCSTR("The required IsChecked.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    if (_isEnabled.isVariable()) {
      _required[idx].name = _isEnabled.variableName();
      _required[idx].help = SHCCSTR("The required IsEnabled.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _action.compose(data);

    if (_isChecked.isVariable()) {
      // search for an existing variable and ensure it's the right type
      auto found = false;
      for (auto &var : data.shared) {
        if (strcmp(var.name, _isChecked.variableName()) == 0) {
          // we found a variable, make sure it's the right type and mark
          if (var.exposedType.basicType != SHType::Bool) {
            throw SHException("GUI - MenuItem: Existing variable type not "
                              "matching the input.");
          }
          // also make sure it's mutable!
          if (!var.isMutable) {
            throw SHException("GUI - MenuItem: Existing variable is not mutable.");
          }
          found = true;
          break;
        }
      }
      if (!found)
        // we didn't find a variable
        throw SHException("GUI - MenuItem: Missing mutable variable.");
    }

    return CoreInfo::BoolType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    bool active;
    if (_isChecked.isVariable()) {
      active = ::ImGui::MenuItem(_label.c_str(), _shortcut.c_str(), &_isChecked.get().payload.boolValue,
                                 _isEnabled.get().payload.boolValue);
    } else {
      active = ::ImGui::MenuItem(_label.c_str(), _shortcut.c_str(), _isChecked.get().payload.boolValue,
                                 _isEnabled.get().payload.boolValue);
    }
    if (active) {
      SHVar output{};
      _action.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params{
      {"Label", SHCCSTR("The label of the menu item"), {CoreInfo::StringType}},
      {"IsChecked",
       SHCCSTR("Sets whether this menu item is checked. A checked item "
               "displays a check mark on the side."),
       {CoreInfo::BoolType, CoreInfo::BoolVarType}},
      {"Action", SHCCSTR(""), {CoreInfo::ShardsOrNone}},
      {"Shortcut", SHCCSTR("A keyboard shortcut to activate that item"), {CoreInfo::StringType}},
      {"IsEnabled",
       SHCCSTR("Sets whether this menu item is enabled. A disabled item cannot "
               "be selected or clicked."),
       {CoreInfo::BoolType, CoreInfo::BoolVarType}},
  };

  std::string _label;
  ParamVar _isChecked{Var::False};
  ShardsVar _action{};
  std::string _shortcut;
  ParamVar _isEnabled{Var::True};
  std::array<SHExposedTypeInfo, 3> _required;
};

struct Combo : public Variable<SHType::Int> {
  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A sequence of values."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The selected value."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto count = input.payload.seqValue.len;
    if (count == 0) {
      if (_variable.isVariable())
        _variable.get().payload.intValue = -1;
      return Var::Empty;
    }

    std::vector<std::string> vec;
    for (uint32_t i = 0; i < count; i++) {
      std::ostringstream stream;
      stream << input.payload.seqValue.elements[i];
      vec.push_back(stream.str());
    }
    const char *items[count];
    for (size_t i = 0; i < vec.size(); i++) {
      items[i] = vec[i].c_str();
    }

    if (_variable.isVariable()) {
      _n = _variable.get().payload.intValue;
      ::ImGui::Combo(_label.c_str(), &_n, items, count);
      _variable.get().payload.intValue = _n;
    } else {
      ::ImGui::Combo(_label.c_str(), &_n, items, count);
    }

    SHVar output{};
    ::shards::cloneVar(output, input.payload.seqValue.elements[_n]);
    return output;
  }

private:
  int _n{0};
};

struct ListBox : public Variable<SHType::Int> {
  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
    case 1:
      Variable<SHType::Int>::setParam(index, value);
      break;
    case 2:
      _height = value.payload.intValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
    case 1:
      return Variable<SHType::Int>::getParam(index);
    case 2:
      return Var(_height);
    default:
      break;
    }
    return Var::Empty;
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A sequence of values."); }

  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The currently selected value."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto count = input.payload.seqValue.len;
    if (count == 0) {
      if (_variable.isVariable())
        _variable.get().payload.intValue = -1;
      return Var::Empty;
    }

    std::vector<std::string> vec;
    for (uint32_t i = 0; i < count; i++) {
      std::ostringstream stream;
      stream << input.payload.seqValue.elements[i];
      vec.push_back(stream.str());
    }
    const char *items[count];
    for (size_t i = 0; i < vec.size(); i++) {
      items[i] = vec[i].c_str();
    }

    if (_variable.isVariable()) {
      _n = _variable.get().payload.intValue;
      ::ImGui::ListBox(_label.c_str(), &_n, items, count, _height);
      _variable.get().payload.intValue = _n;
    } else {
      ::ImGui::ListBox(_label.c_str(), &_n, items, count, _height);
    }

    SHVar output{};
    ::shards::cloneVar(output, input.payload.seqValue.elements[_n]);
    return output;
  }

private:
  static inline Parameters _params = {
      VariableParamsInfo(),
      {{"ItemsHeight", SHCCSTR("Height of the list in number of items"), {CoreInfo::IntType}}},
  };

  int _n{0};
  int _height{-1};
};

struct Selectable : public Variable<SHType::Bool> {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean indicating whether this item is currently selected."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    IDContext idCtx(this);

    auto result = Var::False;
    if (_variable.isVariable()) {
      if (::ImGui::Selectable(_label.c_str(), &_variable.get().payload.boolValue)) {
        _variable.get().valueType = SHType::Bool;
        result = Var::True;
      }
    } else {
      // HACK kinda... we recycle _exposing since we are not using it in this
      // branch
      if (::ImGui::Selectable(_label.c_str(), &_exposing)) {
        result = Var::True;
      }
    }
    return result;
  }
};

struct TabBase : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean indicating whether the tab is visible."); }

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      shards = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return shards;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { shards.cleanup(); }

  void warmup(SHContext *context) { shards.warmup(context); }

  SHTypeInfo compose(SHInstanceData &data) {
    shards.compose(data);
    return CoreInfo::BoolType;
  }

protected:
  static inline Parameters params = {
      {"Contents", SHCCSTR("The inner contents shards."), CoreInfo::ShardsOrNone},
  };

  ShardsVar shards{};
};

struct TabBar : public TabBase {
  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _name.clear();
      } else {
        _name = value.payload.stringValue;
      }
    } break;
    case 1:
      TabBase::setParam(index - 1, value);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_name);
    case 1:
      return TabBase::getParam(index - 1);
    default:
      break;
    }
    return Var::Empty;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto str_id = _name.size() > 0 ? _name.c_str() : "DefaultTabBar";
    auto active = ::ImGui::BeginTabBar(str_id);
    if (active) {
      DEFER(::ImGui::EndTabBar());
      SHVar output{};
      shards.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params = {
      {{"Name", SHCCSTR("A unique name for this tab bar."), {CoreInfo::StringType}}},
      TabBase::params,
  };

  std::string _name;
};

struct TabItem : public TabBase {
  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1:
      TabBase::setParam(index - 1, value);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return TabBase::getParam(index - 1);
    default:
      break;
    }
    return Var::Empty;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto active = ::ImGui::BeginTabItem(_label.c_str());
    if (active) {
      DEFER(::ImGui::EndTabItem());
      SHVar output{};
      shards.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params = {
      {{"Label", SHCCSTR("The label of the tab"), {CoreInfo::StringType}}},
      TabBase::params,
  };

  std::string _label;
};

struct Group : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _shards = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _shards;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { _shards.cleanup(); }

  void warmup(SHContext *context) { _shards.warmup(context); }

  SHTypeInfo compose(SHInstanceData &data) {
    _shards.compose(data);
    return CoreInfo::BoolType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::BeginGroup();
    DEFER(::ImGui::EndGroup());
    SHVar output{};
    _shards.activate(context, input, output);
    return input;
  }

private:
  static inline Parameters _params = {
      {"Contents", SHCCSTR("The inner contents shards."), CoreInfo::ShardsOrNone},
  };

  ShardsVar _shards{};
};

struct Disable : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _shards = value;
      break;
    case 1:
      _disable = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _shards;
    case 1:
      return _disable;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() {
    _disable.cleanup();
    _shards.cleanup();
  }

  void warmup(SHContext *context) {
    _disable.warmup(context);
    _shards.warmup(context);
  }

  SHExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = gfx::Base::mainWindowGlobalsInfo;
    idx++;

    if (_disable.isVariable()) {
      _required[idx].name = _disable.variableName();
      _required[idx].help = SHCCSTR("The required Disable.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _shards.compose(data);
    return data.inputType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::BeginDisabled(_disable.get().payload.boolValue);
    DEFER(::ImGui::EndDisabled());
    SHVar output{};
    _shards.activate(context, input, output);
    return input;
  }

private:
  static inline Parameters _params = {
      {"Contents", SHCCSTR("The inner contents shards."), {CoreInfo::ShardsOrNone}},
      {"Disable", SHCCSTR("Sets whether the contents should be disabled."), {CoreInfo::BoolType, CoreInfo::BoolVarType}},
  };

  ParamVar _disable{Var::True};
  ShardsVar _shards{};
  std::array<SHExposedTypeInfo, 2> _required;
};

struct Table : public Base {
  static SHOptionalString inputHelp() { return SHCCSTR("The value will be passed to the Contents shards."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean indicating whether the table is visible."); }

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _name.clear();
      } else {
        _name = value.payload.stringValue;
      }
    } break;
    case 1:
      _columns = value.payload.intValue;
      break;
    case 2:
      _shards = value;
      break;
    case 3:
      _flags = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_name);
    case 1:
      return Var(_columns);
    case 2:
      return _shards;
    case 3:
      return _flags;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() {
    _shards.cleanup();
    _flags.cleanup();
  }

  void warmup(SHContext *context) {
    _shards.warmup(context);
    _flags.warmup(context);
  }

  SHTypeInfo compose(SHInstanceData &data) {
    _shards.compose(data);
    return CoreInfo::BoolType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto flags = ::ImGuiTableFlags(shards::getFlags<Enums::GuiTableFlags>(_flags.get()));
    auto str_id = _name.size() > 0 ? _name.c_str() : "DefaultTable";
    auto active = ::ImGui::BeginTable(str_id, _columns, flags);
    if (active) {
      DEFER(::ImGui::EndTable());
      SHVar output{};
      _shards.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params = {
      {"Name", SHCCSTR("A unique name for this table."), {CoreInfo::StringType}},
      {"Columns", SHCCSTR("The number of columns."), {CoreInfo::IntType}},
      {"Contents", SHCCSTR("The inner contents shards."), CoreInfo::ShardsOrNone},
      {"Flags",
       SHCCSTR("Flags to enable table options."),
       {Enums::GuiTableFlagsType, Enums::GuiTableFlagsVarType, Enums::GuiTableFlagsSeqType, Enums::GuiTableFlagsVarSeqType,
        CoreInfo::NoneType}},
  };

  std::string _name;
  int _columns;
  ShardsVar _shards{};
  ParamVar _flags{};
};

struct TableHeadersRow : public Base {
  SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::TableHeadersRow();
    return input;
  }
};

struct TableNextColumn : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The input value is ignored."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean indicating whether the table column is visible."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto active = ::ImGui::TableNextColumn();
    return Var(active);
  }
};

struct TableNextRow : public Base {
  SHVar activate(SHContext *context, const SHVar &input) {
    ::ImGui::TableNextRow();
    return input;
  }
};

struct TableSetColumnIndex : public Base {
  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The table index."); }

  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A boolean indicating whether the table column is visible."); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &index = input.payload.intValue;
    auto active = ::ImGui::TableSetColumnIndex(index);
    return Var(active);
  }
};

struct TableSetupColumn : public Base {

  static SHParametersInfo parameters() { return _params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1:
      _flags = value;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _flags;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { _flags.cleanup(); }

  void warmup(SHContext *context) { _flags.warmup(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto flags = ::ImGuiTableColumnFlags(shards::getFlags<Enums::GuiTableColumnFlags>(_flags.get()));
    ::ImGui::TableSetupColumn(_label.c_str(), flags);
    return input;
  }

private:
  static inline Parameters _params = {
      {"Label", SHCCSTR("Column header"), {CoreInfo::StringType}},
      {"Flags",
       SHCCSTR("Flags to enable column options."),
       {Enums::GuiTableColumnFlagsType, Enums::GuiTableColumnFlagsVarType, Enums::GuiTableColumnFlagsSeqType,
        Enums::GuiTableColumnFlagsVarSeqType, CoreInfo::NoneType}},
  };

  std::string _label;
  ParamVar _flags{};
};

void registerShards() {
  REGISTER_SHARD("GUI.Style", Style);
  REGISTER_SHARD("GUI.Window", Window);
  REGISTER_SHARD("GUI.ChildWindow", ChildWindow);
  REGISTER_SHARD("GUI.Checkbox", Checkbox);
  REGISTER_SHARD("GUI.CheckboxFlags", CheckboxFlags);
  REGISTER_SHARD("GUI.RadioButton", RadioButton);
  REGISTER_SHARD("GUI.Text", Text);
  REGISTER_SHARD("GUI.Bullet", Bullet);
  REGISTER_SHARD("GUI.Button", Button);
  REGISTER_SHARD("GUI.HexViewer", HexViewer);
  REGISTER_SHARD("GUI.Dummy", Dummy);
  REGISTER_SHARD("GUI.NewLine", NewLine);
  REGISTER_SHARD("GUI.SameLine", SameLine);
  REGISTER_SHARD("GUI.Separator", Separator);
  REGISTER_SHARD("GUI.Spacing", Spacing);
  REGISTER_SHARD("GUI.Indent", Indent);
  REGISTER_SHARD("GUI.Unindent", Unindent);
  REGISTER_SHARD("GUI.TreeNode", TreeNode);
  REGISTER_SHARD("GUI.CollapsingHeader", CollapsingHeader);
  REGISTER_SHARD("GUI.IntInput", IntInput);
  REGISTER_SHARD("GUI.FloatInput", FloatInput);
  REGISTER_SHARD("GUI.Int2Input", Int2Input);
  REGISTER_SHARD("GUI.Int3Input", Int3Input);
  REGISTER_SHARD("GUI.Int4Input", Int4Input);
  REGISTER_SHARD("GUI.Float2Input", Float2Input);
  REGISTER_SHARD("GUI.Float3Input", Float3Input);
  REGISTER_SHARD("GUI.Float4Input", Float4Input);
  REGISTER_SHARD("GUI.IntDrag", IntDrag);
  REGISTER_SHARD("GUI.FloatDrag", FloatDrag);
  REGISTER_SHARD("GUI.Int2Drag", Int2Drag);
  REGISTER_SHARD("GUI.Int3Drag", Int3Drag);
  REGISTER_SHARD("GUI.Int4Drag", Int4Drag);
  REGISTER_SHARD("GUI.Float2Drag", Float2Drag);
  REGISTER_SHARD("GUI.Float3Drag", Float3Drag);
  REGISTER_SHARD("GUI.Float4Drag", Float4Drag);
  REGISTER_SHARD("GUI.IntSlider", IntSlider);
  REGISTER_SHARD("GUI.FloatSlider", FloatSlider);
  REGISTER_SHARD("GUI.Int2Slider", Int2Slider);
  REGISTER_SHARD("GUI.Int3Slider", Int3Slider);
  REGISTER_SHARD("GUI.Int4Slider", Int4Slider);
  REGISTER_SHARD("GUI.Float2Slider", Float2Slider);
  REGISTER_SHARD("GUI.Float3Slider", Float3Slider);
  REGISTER_SHARD("GUI.Float4Slider", Float4Slider);
  REGISTER_SHARD("GUI.TextInput", TextInput);
#if 0
  REGISTER_SHARD("GUI.Image", Image);
#endif
  REGISTER_SHARD("GUI.Plot", Plot);
  REGISTER_SHARD("GUI.PlotLine", PlotLine);
  REGISTER_SHARD("GUI.PlotDigital", PlotDigital);
  REGISTER_SHARD("GUI.PlotScatter", PlotScatter);
  REGISTER_SHARD("GUI.PlotBars", PlotBars);
  REGISTER_SHARD("GUI.GetClipboard", GetClipboard);
  REGISTER_SHARD("GUI.SetClipboard", SetClipboard);
  REGISTER_SHARD("GUI.ColorInput", ColorInput);
  REGISTER_SHARD("GUI.HasPointer", HasPointer);
  REGISTER_SHARD("GUI.FPS", FPS);
  REGISTER_SHARD("GUI.Tooltip", Tooltip);
  REGISTER_SHARD("GUI.HelpMarker", HelpMarker);
  REGISTER_SHARD("GUI.ProgressBar", ProgressBar);
  REGISTER_SHARD("GUI.MainMenuBar", MainMenuBar);
  REGISTER_SHARD("GUI.MenuBar", MenuBar);
  REGISTER_SHARD("GUI.Menu", Menu);
  REGISTER_SHARD("GUI.MenuItem", MenuItem);
  REGISTER_SHARD("GUI.Combo", Combo);
  REGISTER_SHARD("GUI.ListBox", ListBox);
  REGISTER_SHARD("GUI.Selectable", Selectable);
  REGISTER_SHARD("GUI.TabBar", TabBar);
  REGISTER_SHARD("GUI.TabItem", TabItem);
  REGISTER_SHARD("GUI.Group", Group);
  REGISTER_SHARD("GUI.Disable", Disable);
  REGISTER_SHARD("GUI.Table", Table);
  REGISTER_SHARD("GUI.HeadersRow", TableHeadersRow);
  REGISTER_SHARD("GUI.NextColumn", TableNextColumn);
  REGISTER_SHARD("GUI.NextRow", TableNextRow);
  REGISTER_SHARD("GUI.SetColumnIndex", TableSetColumnIndex);
  REGISTER_SHARD("GUI.SetupColumn", TableSetupColumn);
}
}; // namespace ImGui
}; // namespace shards
