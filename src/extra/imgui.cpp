/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "imgui.hpp"

#include "bgfx.hpp"
#include "blocks/shared.hpp"
#include "runtime.hpp"
#include <implot.h>

using namespace chainblocks;

namespace chainblocks {
namespace ImGui {
struct Base {
  static inline CBExposedTypeInfo ContextInfo = ExposedInfo::Variable(
      "GUI.Context", CBCCSTR("The ImGui Context."), Context::Info);
  static inline ExposedInfo requiredInfo = ExposedInfo(ContextInfo);

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(requiredInfo);
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
};

struct IDContext {
  IDContext(void *ptr) { ::ImGui::PushID(ptr); }
  ~IDContext() { ::ImGui::PopID(); }
};

struct Style : public Base {
  static inline Type styleEnumInfo{
      {CBType::Enum, {.enumeration = {.vendorId = CoreCC, .typeId = 'guiS'}}}};

  enum ImGuiStyle {
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

  typedef EnumInfo<ImGuiStyle> ImGuiStyleInfo;
  static inline ImGuiStyleInfo imguiEnumInfo{"GuiStyle", CoreCC, 'guiS'};

  ImGuiStyle _key{};

  static inline ParamsInfo paramsInfo = ParamsInfo(ParamsInfo::Param(
      "Style", CBCCSTR("A style key to set."), styleEnumInfo));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _key = ImGuiStyle(value.payload.enumValue);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var::Enum(_key, CoreCC, 'guiS');
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
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
        throw CBException(
            "this ImGui Style block expected a Float variable as input!");
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
        throw CBException(
            "this ImGui Style block expected a Float2 variable as input!");
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
        throw CBException(
            "this ImGui Style block expected a Color variable as input!");
      }
      break;
    }
    return data.inputType;
  }

  static ImVec2 var2Vec2(const CBVar &input) {
    ImVec2 res;
    res.x = input.payload.float2Value[0];
    res.y = input.payload.float2Value[1];
    return res;
  }

  static ImVec4 color2Vec4(const CBColor &color) {
    // remember, we edited the shader to do srgb->linear
    ImVec4 res;
    res.x = color.r / 255.0f;
    res.y = color.g / 255.0f;
    res.z = color.b / 255.0f;
    res.w = color.a / 255.0f;
    return res;
  }

  static ImVec4 color2Vec4(const CBVar &input) {
    return color2Vec4(input.payload.colorValue);
  }

  static CBColor vec42Color(const ImVec4 &color) {
    // remember, we edited the shader to do srgb->linear
    CBColor res;
    res.r = roundf(color.x * 255.0f);
    res.g = roundf(color.y * 255.0f);
    res.b = roundf(color.z * 255.0f);
    res.a = roundf(color.w * 255.0f);
    return res;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
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
  chainblocks::BlocksVar _blks{};
  std::string _title;
  bool firstActivation{true};
  Var _pos{}, _width{}, _height{};
  bool _movable{false};
  bool _closable{false};
  bool _resizable{false};
  bool _showMenuBar{false};
  ParamVar _notClosed{Var::True};
  std::array<CBExposedTypeInfo, 2> _required;

  static inline Parameters _params{
      {"Title",
       CBCCSTR("The title of the window to create."),
       {CoreInfo::StringType}},
      {"Pos",
       CBCCSTR("The x/y position of the window to create. If the value is a "
               "Float2, it will be interpreted as relative to the container "
               "window size."),
       {CoreInfo::Int2Type, CoreInfo::Float2Type, CoreInfo::NoneType}},
      {"Width",
       CBCCSTR("The width of the window to create. If the value is a Float, it "
               "will be interpreted as relative to the container window size."),
       {CoreInfo::IntType, CoreInfo::FloatType, CoreInfo::NoneType}},
      {"Height",
       CBCCSTR(
           "The height of the window to create. If the value is a Float, it "
           "will be interpreted as relative to the container window size."),
       {CoreInfo::IntType, CoreInfo::FloatType, CoreInfo::NoneType}},
      {"Contents", CBCCSTR("The inner contents blocks."),
       CoreInfo::BlocksOrNone},
      {"AllowMove",
       CBCCSTR("If the window can be moved."),
       {CoreInfo::BoolType}},
      {"AllowResize",
       CBCCSTR("If the window can be resized."),
       {CoreInfo::BoolType}},
      {"AllowCollapse",
       CBCCSTR("If the window can be collapsed."),
       {CoreInfo::BoolType}},
      {"ShowMenuBar",
       CBCCSTR("If the window should display a menubar."),
       {CoreInfo::BoolType}},
      {"OnClose",
       CBCCSTR("Passing a variable will display a close button in the "
               "upper-right corner. Clicking will set the variable to false "
               "and hide the window."),
       {CoreInfo::BoolVarType}},
  };

  static CBParametersInfo parameters() { return _params; }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blks.compose(data);
    return data.inputType;
  }

  void setParam(int index, const CBVar &value) {
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
      _movable = value.payload.boolValue;
      break;
    case 6:
      _resizable = value.payload.boolValue;
      break;
    case 7:
      _closable = value.payload.boolValue;
      break;
    case 8:
      _showMenuBar = value.payload.boolValue;
      break;
    case 9:
      _notClosed = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
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
      return Var(_movable);
    case 6:
      return Var(_resizable);
    case 7:
      return Var(_closable);
    case 8:
      return Var(_showMenuBar);
    case 9:
      return _notClosed;
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    _blks.cleanup();
    _notClosed.cleanup();
  }

  void warmup(CBContext *context) {
    _blks.warmup(context);
    _notClosed.warmup(context);
    firstActivation = true;
  }

  CBExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = Base::ContextInfo;
    idx++;

    if (_notClosed.isVariable()) {
      _required[idx].name = _notClosed.variableName();
      _required[idx].help = CBCCSTR("The required OnClose.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_blks)
      return input;

    int flags = ImGuiWindowFlags_NoSavedSettings;

    flags |= !_movable ? ImGuiWindowFlags_NoMove : 0;
    flags |= !_closable ? ImGuiWindowFlags_NoCollapse : 0;
    flags |= !_resizable ? ImGuiWindowFlags_NoResize : 0;
    flags |= _showMenuBar ? ImGuiWindowFlags_MenuBar : 0;

    if (firstActivation) {
      const ImGuiIO &io = ::ImGui::GetIO();

      if (_pos.valueType == Int2) {
        ImVec2 pos = {float(_pos.payload.int2Value[0]),
                      float(_pos.payload.int2Value[1])};
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

    auto active = ::ImGui::Begin(
        _title.c_str(),
        _notClosed.isVariable() ? &_notClosed.get().payload.boolValue : nullptr,
        flags);
    DEFER(::ImGui::End());
    if (active) {
      CBVar output{};
      _blks.activate(context, input, output);
    }
    return input;
  }
};

struct ChildWindow : public Base {
  chainblocks::BlocksVar _blks{};
  CBVar _width{}, _height{};
  bool _border = false;
  static inline ImGuiID windowIds{0};
  ImGuiID _wndId = ++windowIds;

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Width",
                        CBCCSTR("The width of the child window to create"),
                        CoreInfo::IntOrNone),
      ParamsInfo::Param("Height",
                        CBCCSTR("The height of the child window to create."),
                        CoreInfo::IntOrNone),
      ParamsInfo::Param(
          "Border",
          CBCCSTR("If we want to draw a border frame around the child window."),
          CoreInfo::BoolType),
      ParamsInfo::Param("Contents", CBCCSTR("The inner contents blocks."),
                        CoreInfo::BlocksOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blks.compose(data);
    return data.inputType;
  }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  void warmup(CBContext *context) { _blks.warmup(context); }

  CBVar activate(CBContext *context, const CBVar &input) {
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
      CBVar output{};
      _blks.activate(context, input, output);
    }
    return input;
  }
};

Parameters &VariableParamsInfo() {
  static Parameters params{
      {"Label", CBCCSTR("The label for this widget."), CoreInfo::StringOrNone},
      {"Variable",
       CBCCSTR("The name of the variable that holds the input value."),
       CoreInfo::StringOrNone}};
  return params;
}

template <CBType CT> struct Variable : public Base {
  static inline Type varType{{CT}};

  std::string _label;
  std::string _variable_name;
  CBVar *_variable = nullptr;
  ExposedInfo _expInfo{};
  bool _exposing = false;

  void cleanup() { cleanupVariable(); }

  void cleanupVariable() {
    if (_variable) {
      releaseVariable(_variable);
      _variable = nullptr;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_variable_name.size() > 0) {
      _exposing = true; // assume we expose a new variable
      // search for a possible existing variable and ensure it's the right type
      for (auto &var : data.shared) {
        if (strcmp(var.name, _variable_name.c_str()) == 0) {
          // we found a variable, make sure it's the right type and mark
          // exposing off
          _exposing = false;
          if (var.exposedType.basicType != CT) {
            throw CBException("ImGui - Variable: Existing variable type not "
                              "matching the input.");
          }
          // also make sure it's mutable!
          if (!var.isMutable) {
            throw CBException(
                "ImGui - Variable: Existing variable is not mutable.");
          }
          break;
        }
      }
    }
    return varType;
  }

  CBExposedTypesInfo requiredVariables() {
    if (_variable_name.size() > 0 && !_exposing) {
      _expInfo = ExposedInfo(
          requiredInfo,
          ExposedInfo::Variable(_variable_name.c_str(),
                                CBCCSTR("The required input variable."),
                                CBTypeInfo(varType)));
      return CBExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  CBExposedTypesInfo exposedVariables() {
    if (_variable_name.size() > 0 && _exposing) {
      _expInfo = ExposedInfo(
          requiredInfo,
          ExposedInfo::Variable(_variable_name.c_str(),
                                CBCCSTR("The exposed input variable."),
                                CBTypeInfo(varType), true));
      return CBExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  static CBParametersInfo parameters() {
    static CBParametersInfo info = VariableParamsInfo();
    return info;
  }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1: {
      if (value.valueType == None) {
        _variable_name.clear();
      } else {
        _variable_name = value.payload.stringValue;
      }
      cleanupVariable();
    } break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _label.size() == 0 ? Var::Empty : Var(_label);
    case 1:
      return _variable_name.size() == 0 ? Var::Empty : Var(_variable_name);
    default:
      return Var::Empty;
    }
  }
};

template <CBType CT1, CBType CT2> struct Variable2 : public Base {
  static inline Type varType1{{CT1}};
  static inline Type varType2{{CT2}};

  std::string _label;
  std::string _variable_name;
  CBVar *_variable = nullptr;
  ExposedInfo _expInfo{};
  bool _exposing = false;

  void cleanup() { cleanupVariable(); }

  void cleanupVariable() {
    if (_variable) {
      releaseVariable(_variable);
      _variable = nullptr;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_variable_name.size() > 0) {
      _exposing = true; // assume we expose a new variable
      // search for a possible existing variable and ensure it's the right type
      for (auto &var : data.shared) {
        if (strcmp(var.name, _variable_name.c_str()) == 0) {
          // we found a variable, make sure it's the right type and mark
          // exposing off
          _exposing = false;
          if (var.exposedType.basicType != CT1 &&
              var.exposedType.basicType != CT2) {
            throw CBException("ImGui - Variable: Existing variable type not "
                              "matching the input.");
          }
          // also make sure it's mutable!
          if (!var.isMutable) {
            throw CBException(
                "ImGui - Variable: Existing variable is not mutable.");
          }
          break;
        }
      }
    }
    return CoreInfo::AnyType;
  }

  CBExposedTypesInfo requiredVariables() {
    if (_variable_name.size() > 0 && !_exposing) {
      _expInfo = ExposedInfo(
          requiredInfo,
          ExposedInfo::Variable(_variable_name.c_str(),
                                CBCCSTR("The required input variable."),
                                CBTypeInfo(varType1)),
          ExposedInfo::Variable(_variable_name.c_str(),
                                CBCCSTR("The required input variable."),
                                CBTypeInfo(varType2)));
      return CBExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  CBExposedTypesInfo exposedVariables() {
    if (_variable_name.size() > 0 && _exposing) {
      _expInfo = ExposedInfo(
          requiredInfo,
          ExposedInfo::Variable(_variable_name.c_str(),
                                CBCCSTR("The exposed input variable."),
                                CBTypeInfo(varType1), true),
          ExposedInfo::Variable(_variable_name.c_str(),
                                CBCCSTR("The exposed input variable."),
                                CBTypeInfo(varType2), true));
      return CBExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  static CBParametersInfo parameters() {
    static CBParametersInfo info = VariableParamsInfo();
    return info;
  }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0: {
      if (value.valueType == None) {
        _label.clear();
      } else {
        _label = value.payload.stringValue;
      }
    } break;
    case 1: {
      if (value.valueType == None) {
        _variable_name.clear();
      } else {
        _variable_name = value.payload.stringValue;
      }
      cleanupVariable();
    } break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _label.size() == 0 ? Var::Empty : Var(_label);
    case 1:
      return _variable_name.size() == 0 ? Var::Empty : Var(_variable_name);
    default:
      return Var::Empty;
    }
  }
};

struct Checkbox : public Variable<CBType::Bool> {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_variable && _variable_name.size() > 0) {
      _variable = referenceVariable(context, _variable_name.c_str());
    }

    auto result = Var::False;
    if (_variable) {
      if (::ImGui::Checkbox(_label.c_str(), &_variable->payload.boolValue)) {
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

struct CheckboxFlags : public Variable2<CBType::Int, CBType::Enum> {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    if (index < 2)
      Variable2<CBType::Int, CBType::Enum>::setParam(index, value);
    else
      _value = value;
  }

  CBVar getParam(int index) {
    if (index < 2)
      return Variable2<CBType::Int, CBType::Enum>::getParam(index);
    else
      return _value;
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    Variable2<CBType::Int, CBType::Enum>::compose(data);

    // ideally here we should check that the type of _value is the same as
    // _variable.

    return CoreInfo::BoolType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_variable && _variable_name.size() > 0) {
      _variable = referenceVariable(context, _variable_name.c_str());
    }

    auto result = Var::False;
    switch (_value.valueType) {
    case CBType::Int: {
      int *flags;
      if (_variable) {
        flags = reinterpret_cast<int *>(&_variable->payload.intValue);
      } else {
        flags = reinterpret_cast<int *>(&_tmp.payload.intValue);
      }
      if (::ImGui::CheckboxFlags(_label.c_str(), flags,
                                 int(_value.payload.intValue))) {
        result = Var::True;
      }
    } break;
    case CBType::Enum: {
      int *flags;
      if (_variable) {
        flags = reinterpret_cast<int *>(&_variable->payload.enumValue);
      } else {
        flags = reinterpret_cast<int *>(&_tmp.payload.enumValue);
      }
      if (::ImGui::CheckboxFlags(_label.c_str(), flags,
                                 int(_value.payload.enumValue))) {
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
      VariableParamsInfo(),
      {{"Value",
        CBCCSTR("The flag value to set or unset."),
        {CoreInfo::IntType, CoreInfo::AnyEnumType}}}};

  CBVar _value{};
  CBVar _tmp{};
};

struct RadioButton : public Variable<CBType::Int> {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return paramsInfo; }

  void setParam(int index, const CBVar &value) {
    if (index < 2)
      Variable<CBType::Int>::setParam(index, value);
    else
      _value = value.payload.intValue;
  }

  CBVar getParam(int index) {
    if (index < 2)
      return Variable<CBType::Int>::getParam(index);
    else
      return Var(_value);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_variable && _variable_name.size() > 0) {
      _variable = referenceVariable(context, _variable_name.c_str());
    }

    auto result = Var::False;
    if (_variable) {
      if (::ImGui::RadioButton(_label.c_str(),
                               _variable->payload.intValue == _value)) {
        _variable->payload.intValue = _value;
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
  static inline Parameters paramsInfo{
      VariableParamsInfo(),
      {{"Value", CBCCSTR("The value to compare with."), {CoreInfo::IntType}}}};

  CBInt _value;
};

struct Text : public Base {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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
  CBVar activate(CBContext *context, const CBVar &input) {
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
      {"Label",
       CBCCSTR("An optional label for the value."),
       {CoreInfo::StringOrNone}},
      {"Color",
       CBCCSTR("The optional color of the text."),
       {CoreInfo::ColorOrNone}},
      {"Format",
       CBCCSTR("An optional format for the text."),
       {CoreInfo::StringOrNone}},
      {"Wrap",
       CBCCSTR("Whether to wrap the text to the next line if it doesn't fit "
               "horizontally."),
       {CoreInfo::BoolType}},
      {"Bullet",
       CBCCSTR("Display a small circle before the text."),
       {CoreInfo::BoolType}},
  };

  std::string _label;
  CBVar _color{};
  std::string _format;
  bool _wrap{false};
  bool _bullet{false};
};

struct Bullet : public Base {
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::Bullet();
    return input;
  }
};

struct Button : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _blocks = value;
      break;
    case 2:
      _type = Enums::GuiButtonKind(value.payload.enumValue);
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _blocks;
    case 2:
      return Var::Enum(_type, CoreCC, Enums::GuiButtonKindCC);
    case 3:
      return Var(_size.x, _size.x);
    case 4:
      return Var(_repeat);
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return CoreInfo::BoolType;
  }

  void cleanup() { _blocks.cleanup(); }

  void warmup(CBContext *ctx) { _blocks.warmup(ctx); }

#define IMBTN_RUN_ACTION                                                       \
  {                                                                            \
    CBVar output = Var::Empty;                                                 \
    _blocks.activate(context, input, output);                                  \
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    ::ImGui::PushButtonRepeat(_repeat);
    DEFER(::ImGui::PopButtonRepeat());

    auto result = Var::False;
    ImVec2 size;
    switch (_type) {
    case Enums::GuiButtonKind::Normal:
      if (::ImGui::Button(_label.c_str(), _size)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButtonKind::Small:
      if (::ImGui::SmallButton(_label.c_str())) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButtonKind::Invisible:
      if (::ImGui::InvisibleButton(_label.c_str(), _size)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButtonKind::ArrowLeft:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Left)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButtonKind::ArrowRight:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Right)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButtonKind::ArrowUp:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Up)) {
        IMBTN_RUN_ACTION;
        result = Var::True;
      }
      break;
    case Enums::GuiButtonKind::ArrowDown:
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
      {"Label",
       CBCCSTR("The text label of this button."),
       {CoreInfo::StringType}},
      {"Action", CBCCSTR("The blocks to execute when the button is pressed."),
       CoreInfo::BlocksOrNone},
      {"Type", CBCCSTR("The button type."), {Enums::GuiButtonKindType}},
      {"Size", CBCCSTR("The optional size override."), {CoreInfo::Float2Type}},
      {"Repeat",
       CBCCSTR("Whether to repeat the action while the button is pressed."),
       {CoreInfo::BoolType}},
  };

  std::string _label;
  BlocksVar _blocks{};
  Enums::GuiButtonKind _type{};
  ImVec2 _size = {0, 0};
  bool _repeat{false};
};

struct HexViewer : public Base {
  // TODO use a variable so edits are possible and easy

  static CBTypesInfo inputTypes() { return CoreInfo::BytesType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

  ImGuiExtra::MemoryEditor _editor{};

  // HexViewer() { _editor.ReadOnly = true; }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);
    _editor.DrawContents(input.payload.bytesValue, input.payload.bytesSize);
    return input;
  }
};

struct Dummy : public Base {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  void warmup(CBContext *ctx) {
    _width.warmup(ctx);
    _height.warmup(ctx);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto width = float(_width.get().payload.intValue);
    auto height = float(_height.get().payload.intValue);
    ::ImGui::Dummy({width, height});
    return input;
  }

private:
  static inline Parameters _params = {
      {"Width", CBCCSTR("The width of the item."), CoreInfo::IntOrIntVar},
      {"Height", CBCCSTR("The height of the item."), CoreInfo::IntOrIntVar},
  };

  ParamVar _width{Var(0)};
  ParamVar _height{Var(0)};
};

struct NewLine : public Base {
  CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::NewLine();
    return input;
  }
};

struct SameLine : public Base {
  // TODO add offsets and spacing
  CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::SameLine();
    return input;
  }
};

struct Separator : public Base {
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::Separator();
    return input;
  }
};

struct Spacing : public Base {
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::Spacing();
    return input;
  }
};

struct Indent : public Base {
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::Indent();
    return input;
  }
};

struct Unindent : public Base {
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::Unindent();
    return input;
  }
};

struct GetClipboard : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  static CBVar activate(CBContext *context, const CBVar &input) {
    auto contents = ::ImGui::GetClipboardText();
    if (contents)
      return Var(contents);
    else
      return Var("");
  }
};

struct SetClipboard : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::SetClipboardText(input.payload.stringValue);
    return input;
  }
};

struct TreeNode : public Base {
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return _params; }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return CoreInfo::BoolType;
  }

  void cleanup() {
    _blocks.cleanup();
    _flags.cleanup();
  }

  void warmup(CBContext *context) {
    _blocks.warmup(context);
    _flags.warmup(context);
  }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _blocks = value;
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _blocks;
    case 2:
      return Var(_defaultOpen);
    case 3:
      return _flags;
    default:
      return CBVar();
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    auto flags = ::ImGuiTreeNodeFlags(_flags.get().payload.enumValue);

    if (_defaultOpen) {
      flags |= ::ImGuiTreeNodeFlags_DefaultOpen;
    }

    auto visible = ::ImGui::TreeNodeEx(_label.c_str(), flags);
    if (visible) {
      CBVar output{};
      // run inner blocks
      _blocks.activate(context, input, output);
      // pop the node if was visible
      ::ImGui::TreePop();
    }

    return Var(visible);
  }

private:
  static inline Parameters _params = {
      {"Label", CBCCSTR("The label of this node."), {CoreInfo::StringType}},
      {"Contents", CBCCSTR("The contents of this node."),
       CoreInfo::BlocksOrNone},
      {"StartOpen",
       CBCCSTR("If this node should start in the open state."),
       {CoreInfo::BoolType}},
      {"Flags",
       CBCCSTR("Flags to enable tree node options."),
       {Enums::GuiTreeNodeFlagsType,
        Type::VariableOf(Enums::GuiTreeNodeFlagsType)}},
  };

  std::string _label;
  bool _defaultOpen = false;
  BlocksVar _blocks;
  ParamVar _flags{};
};

struct CollapsingHeader : public Base {
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _blocks = value;
      break;
    case 2:
      _defaultOpen = value.payload.boolValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _blocks;
    case 2:
      return Var(_defaultOpen);
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return CoreInfo::BoolType;
  }

  void cleanup() { _blocks.cleanup(); }

  void warmup(CBContext *ctx) { _blocks.warmup(ctx); }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (_defaultOpen) {
      ::ImGui::SetNextItemOpen(true);
      _defaultOpen = false;
    }

    auto active = ::ImGui::CollapsingHeader(_label.c_str());
    if (active) {
      CBVar output{};
      _blocks.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params = {
      {"Label", CBCCSTR("The label of this node."), {CoreInfo::StringType}},
      {"Contents",
       CBCCSTR("The contents under this header."),
       {CoreInfo::BlocksOrNone}},
      {"StartOpen",
       CBCCSTR("If this header should start in the open state."),
       {CoreInfo::BoolType}},
  };

  std::string _label;
  BlocksVar _blocks{};
  bool _defaultOpen = false;
};

template <CBType CBT> struct DragBase : public Variable<CBT> {
  float _speed{1.0};

  static inline Parameters paramsInfo{
      VariableParamsInfo(),
      {{"Speed", CBCCSTR("The speed multiplier for this drag widget."),
        CoreInfo::StringOrNone}}};

  static CBParametersInfo parameters() { return paramsInfo; }

  void setParam(int index, const CBVar &value) {
    if (index < 2)
      Variable<CBT>::setParam(index, value);
    else
      _speed = value.payload.floatValue;
  }

  CBVar getParam(int index) {
    if (index < 2)
      return Variable<CBT>::getParam(index);
    else
      return Var(_speed);
  }
};

#define IMGUIDRAG(_CBT_, _T_, _INFO_, _IMT_, _VAL_)                            \
  struct _CBT_##Drag : public DragBase<CBType::_CBT_> {                        \
    _T_ _tmp;                                                                  \
                                                                               \
    static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }             \
                                                                               \
    static CBTypesInfo outputTypes() { return CoreInfo::_INFO_; }              \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      IDContext idCtx(this);                                                   \
                                                                               \
      if (!_variable && _variable_name.size() > 0) {                           \
        _variable = referenceVariable(context, _variable_name.c_str());        \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_;                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      if (_variable) {                                                         \
        ::ImGui::DragScalar(_label.c_str(), _IMT_,                             \
                            (void *)&_variable->payload._VAL_, _speed);        \
        return *_variable;                                                     \
      } else {                                                                 \
        ::ImGui::DragScalar(_label.c_str(), _IMT_, (void *)&_tmp, _speed);     \
        return Var(_tmp);                                                      \
      }                                                                        \
    }                                                                          \
  }

IMGUIDRAG(Int, int64_t, IntType, ImGuiDataType_S64, intValue);
IMGUIDRAG(Float, double, FloatType, ImGuiDataType_Double, floatValue);

#define IMGUIDRAG2(_CBT_, _T_, _INFO_, _IMT_, _VAL_, _CMP_)                    \
  struct _CBT_##Drag : public DragBase<CBType::_CBT_> {                        \
    CBVar _tmp;                                                                \
                                                                               \
    static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }             \
                                                                               \
    static CBTypesInfo outputTypes() { return CoreInfo::_INFO_; }              \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      IDContext idCtx(this);                                                   \
                                                                               \
      if (!_variable && _variable_name.size() > 0) {                           \
        _variable = referenceVariable(context, _variable_name.c_str());        \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_;                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      if (_variable) {                                                         \
        ::ImGui::DragScalarN(_label.c_str(), _IMT_,                            \
                             (void *)&_variable->payload._VAL_, _CMP_,         \
                             _speed);                                          \
        return *_variable;                                                     \
      } else {                                                                 \
        _tmp.valueType = _CBT_;                                                \
        ::ImGui::DragScalarN(_label.c_str(), _IMT_,                            \
                             (void *)&_tmp.payload._VAL_, _CMP_, _speed);      \
        return _tmp;                                                           \
      }                                                                        \
    }                                                                          \
  }

IMGUIDRAG2(Int2, int64_t, Int2Type, ImGuiDataType_S64, int2Value, 2);
IMGUIDRAG2(Int3, int32_t, Int3Type, ImGuiDataType_S32, int3Value, 3);
IMGUIDRAG2(Int4, int32_t, Int4Type, ImGuiDataType_S32, int4Value, 4);
IMGUIDRAG2(Float2, double, Float2Type, ImGuiDataType_Double, float2Value, 2);
IMGUIDRAG2(Float3, float, Float3Type, ImGuiDataType_Float, float3Value, 3);
IMGUIDRAG2(Float4, float, Float4Type, ImGuiDataType_Float, float4Value, 4);

#define IMGUIINPUT(_CBT_, _T_, _IMT_, _VAL_, _FMT_)                            \
  struct _CBT_##Input : public Variable<CBType::_CBT_> {                       \
    _T_ _tmp;                                                                  \
                                                                               \
    static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }             \
                                                                               \
    static CBTypesInfo outputTypes() { return CoreInfo::_CBT_##Type; }         \
                                                                               \
    static CBParametersInfo parameters() { return paramsInfo; }                \
                                                                               \
    void cleanup() {                                                           \
      _step.cleanup();                                                         \
      _stepFast.cleanup();                                                     \
      Variable<CBType::_CBT_>::cleanup();                                      \
    }                                                                          \
                                                                               \
    void warmup(CBContext *context) {                                          \
      _step.warmup(context);                                                   \
      _stepFast.warmup(context);                                               \
    }                                                                          \
                                                                               \
    void setParam(int index, const CBVar &value) {                             \
      switch (index) {                                                         \
      case 0:                                                                  \
      case 1:                                                                  \
        Variable<CBType::_CBT_>::setParam(index, value);                       \
        break;                                                                 \
      case 2:                                                                  \
        _step = value;                                                         \
        break;                                                                 \
      case 3:                                                                  \
        _stepFast = value;                                                     \
        break;                                                                 \
      default:                                                                 \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar getParam(int index) {                                                \
      switch (index) {                                                         \
      case 0:                                                                  \
      case 1:                                                                  \
        return Variable<CBType::_CBT_>::getParam(index);                       \
      case 2:                                                                  \
        return _step;                                                          \
      case 3:                                                                  \
        return _stepFast;                                                      \
      default:                                                                 \
        return Var::Empty;                                                     \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      IDContext idCtx(this);                                                   \
                                                                               \
      if (!_variable && _variable_name.size() > 0) {                           \
        _variable = referenceVariable(context, _variable_name.c_str());        \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_;                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      _T_ step = _step.get().payload._VAL_##Value;                             \
      _T_ step_fast = _stepFast.get().payload._VAL_##Value;                    \
      if (_variable) {                                                         \
        ::ImGui::InputScalar(_label.c_str(), _IMT_,                            \
                             (void *)&_variable->payload._VAL_##Value,         \
                             step > 0 ? &step : nullptr,                       \
                             step_fast > 0 ? &step_fast : nullptr, _FMT_, 0);  \
        return *_variable;                                                     \
      } else {                                                                 \
        ::ImGui::InputScalar(_label.c_str(), _IMT_, (void *)&_tmp,             \
                             step > 0 ? &step : nullptr,                       \
                             step_fast > 0 ? &step_fast : nullptr, _FMT_, 0);  \
        return Var(_tmp);                                                      \
      }                                                                        \
    }                                                                          \
                                                                               \
  private:                                                                     \
    static inline Parameters paramsInfo{                                       \
        VariableParamsInfo(),                                                  \
        {{"Step",                                                              \
          CBCCSTR("The value of a single increment."),                         \
          {CoreInfo::_CBT_##Type, CoreInfo::_CBT_##VarType}},                  \
         {"StepFast",                                                          \
          CBCCSTR("The value of a single increment, when holding Ctrl"),       \
          {CoreInfo::_CBT_##Type, CoreInfo::_CBT_##VarType}}},                 \
    };                                                                         \
                                                                               \
    ParamVar _step{Var((_T_)0)};                                               \
    ParamVar _stepFast{Var((_T_)0)};                                           \
  }

IMGUIINPUT(Int, int64_t, ImGuiDataType_S64, int, "%lld");
IMGUIINPUT(Float, double, ImGuiDataType_Double, float, "%.3f");

#define IMGUIINPUT2(_CBT_, _CMP_, _T_, _IMT_, _VAL_, _FMT_)                    \
  struct _CBT_##_CMP_##Input : public Variable<CBType::_CBT_##_CMP_> {         \
    CBVar _tmp;                                                                \
                                                                               \
    static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }             \
                                                                               \
    static CBTypesInfo outputTypes() { return CoreInfo::_CBT_##_CMP_##Type; }  \
                                                                               \
    static CBParametersInfo parameters() { return paramsInfo; }                \
                                                                               \
    void cleanup() {                                                           \
      _step.cleanup();                                                         \
      _stepFast.cleanup();                                                     \
      Variable<CBType::_CBT_##_CMP_>::cleanup();                               \
    }                                                                          \
                                                                               \
    void warmup(CBContext *context) {                                          \
      _step.warmup(context);                                                   \
      _stepFast.warmup(context);                                               \
    }                                                                          \
                                                                               \
    void setParam(int index, const CBVar &value) {                             \
      switch (index) {                                                         \
      case 0:                                                                  \
      case 1:                                                                  \
        Variable<CBType::_CBT_##_CMP_>::setParam(index, value);                \
        break;                                                                 \
      case 2:                                                                  \
        _step = value;                                                         \
        break;                                                                 \
      case 3:                                                                  \
        _stepFast = value;                                                     \
        break;                                                                 \
      default:                                                                 \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar getParam(int index) {                                                \
      switch (index) {                                                         \
      case 0:                                                                  \
      case 1:                                                                  \
        return Variable<CBType::_CBT_##_CMP_>::getParam(index);                \
      case 2:                                                                  \
        return _step;                                                          \
      case 3:                                                                  \
        return _stepFast;                                                      \
      default:                                                                 \
        return Var::Empty;                                                     \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      IDContext idCtx(this);                                                   \
                                                                               \
      if (!_variable && _variable_name.size() > 0) {                           \
        _variable = referenceVariable(context, _variable_name.c_str());        \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_##_CMP_;                                 \
        }                                                                      \
      }                                                                        \
                                                                               \
      _T_ step = _step.get().payload._VAL_##Value;                             \
      _T_ step_fast = _stepFast.get().payload._VAL_##Value;                    \
      if (_variable) {                                                         \
        ::ImGui::InputScalarN(_label.c_str(), _IMT_,                           \
                              (void *)&_variable->payload._VAL_##_CMP_##Value, \
                              _CMP_, step > 0 ? &step : nullptr,               \
                              step_fast > 0 ? &step_fast : nullptr, _FMT_, 0); \
        return *_variable;                                                     \
      } else {                                                                 \
        _tmp.valueType = _CBT_;                                                \
        ::ImGui::InputScalarN(_label.c_str(), _IMT_,                           \
                              (void *)&_tmp.payload._VAL_##_CMP_##Value,       \
                              _CMP_, step > 0 ? &step : nullptr,               \
                              step_fast > 0 ? &step_fast : nullptr, _FMT_, 0); \
        return _tmp;                                                           \
      }                                                                        \
    }                                                                          \
                                                                               \
  private:                                                                     \
    static inline Parameters paramsInfo{                                       \
        VariableParamsInfo(),                                                  \
        {{"Step",                                                              \
          CBCCSTR("The value of a single increment."),                         \
          {CoreInfo::_CBT_##Type, CoreInfo::_CBT_##VarType}},                  \
         {"StepFast",                                                          \
          CBCCSTR("The value of a single increment, when holding Ctrl"),       \
          {CoreInfo::_CBT_##Type, CoreInfo::_CBT_##VarType}}},                 \
    };                                                                         \
                                                                               \
    ParamVar _step{Var((_T_)0)};                                               \
    ParamVar _stepFast{Var((_T_)0)};                                           \
  }

IMGUIINPUT2(Int, 2, int64_t, ImGuiDataType_S64, int, "%lld");
IMGUIINPUT2(Int, 3, int32_t, ImGuiDataType_S32, int, "%d");
IMGUIINPUT2(Int, 4, int32_t, ImGuiDataType_S32, int, "%d");

IMGUIINPUT2(Float, 2, double, ImGuiDataType_Double, float, "%.3f");
IMGUIINPUT2(Float, 3, float, ImGuiDataType_Float, float, "%.3f");
IMGUIINPUT2(Float, 4, float, ImGuiDataType_Float, float, "%.3f");

#define IMGUISLIDER(_CBT_, _T_, _IMT_, _VAL_, _FMT_)                           \
  struct _CBT_##Slider : public Variable<CBType::_CBT_> {                      \
                                                                               \
    static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }             \
                                                                               \
    static CBTypesInfo outputTypes() { return CoreInfo::_CBT_##Type; }         \
                                                                               \
    static CBParametersInfo parameters() { return paramsInfo; }                \
                                                                               \
    void cleanup() {                                                           \
      _min.cleanup();                                                          \
      _max.cleanup();                                                          \
      Variable<CBType::_CBT_>::cleanup();                                      \
    }                                                                          \
                                                                               \
    void warmup(CBContext *context) {                                          \
      _min.warmup(context);                                                    \
      _max.warmup(context);                                                    \
    }                                                                          \
                                                                               \
    void setParam(int index, const CBVar &value) {                             \
      switch (index) {                                                         \
      case 0:                                                                  \
      case 1:                                                                  \
        Variable<CBType::_CBT_>::setParam(index, value);                       \
        break;                                                                 \
      case 2:                                                                  \
        _min = value;                                                          \
        break;                                                                 \
      case 3:                                                                  \
        _max = value;                                                          \
        break;                                                                 \
      default:                                                                 \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar getParam(int index) {                                                \
      switch (index) {                                                         \
      case 0:                                                                  \
      case 1:                                                                  \
        return Variable<CBType::_CBT_>::getParam(index);                       \
      case 2:                                                                  \
        return _min;                                                           \
      case 3:                                                                  \
        return _max;                                                           \
      default:                                                                 \
        return Var::Empty;                                                     \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      IDContext idCtx(this);                                                   \
                                                                               \
      if (!_variable && _variable_name.size() > 0) {                           \
        _variable = referenceVariable(context, _variable_name.c_str());        \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_;                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      _T_ min = _min.get().payload._VAL_##Value;                               \
      _T_ max = _max.get().payload._VAL_##Value;                               \
      if (_variable) {                                                         \
        ::ImGui::SliderScalar(_label.c_str(), _IMT_,                           \
                              (void *)&_variable->payload._VAL_##Value, &min,  \
                              &max, _FMT_, 0);                                 \
        return *_variable;                                                     \
      } else {                                                                 \
        ::ImGui::SliderScalar(_label.c_str(), _IMT_, (void *)&_tmp, &min,      \
                              &max, _FMT_, 0);                                 \
        return Var(_tmp);                                                      \
      }                                                                        \
    }                                                                          \
                                                                               \
  private:                                                                     \
    static inline Parameters paramsInfo{                                       \
        VariableParamsInfo(),                                                  \
        {{"Min",                                                               \
          CBCCSTR("The minimum value."),                                       \
          {CoreInfo::_CBT_##Type, CoreInfo::_CBT_##VarType}},                  \
         {"Max",                                                               \
          CBCCSTR("The maximum value."),                                       \
          {CoreInfo::_CBT_##Type, CoreInfo::_CBT_##VarType}}},                 \
    };                                                                         \
                                                                               \
    ParamVar _min{Var((_T_)0)};                                                \
    ParamVar _max{Var((_T_)100)};                                              \
    _T_ _tmp;                                                                  \
  }

IMGUISLIDER(Int, int64_t, ImGuiDataType_S64, int, "%lld");
IMGUISLIDER(Float, double, ImGuiDataType_Double, float, "%.3f");

#define IMGUISLIDER2(_CBT_, _CMP_, _T_, _IMT_, _VAL_, _FMT_)                   \
  struct _CBT_##_CMP_##Slider : public Variable<CBType::_CBT_##_CMP_> {        \
                                                                               \
    static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }             \
                                                                               \
    static CBTypesInfo outputTypes() { return CoreInfo::_CBT_##_CMP_##Type; }  \
                                                                               \
    static CBParametersInfo parameters() { return paramsInfo; }                \
                                                                               \
    void cleanup() {                                                           \
      _min.cleanup();                                                          \
      _max.cleanup();                                                          \
      Variable<CBType::_CBT_##_CMP_>::cleanup();                               \
    }                                                                          \
                                                                               \
    void warmup(CBContext *context) {                                          \
      _min.warmup(context);                                                    \
      _max.warmup(context);                                                    \
    }                                                                          \
                                                                               \
    void setParam(int index, const CBVar &value) {                             \
      switch (index) {                                                         \
      case 0:                                                                  \
      case 1:                                                                  \
        Variable<CBType::_CBT_##_CMP_>::setParam(index, value);                \
        break;                                                                 \
      case 2:                                                                  \
        _min = value;                                                          \
        break;                                                                 \
      case 3:                                                                  \
        _max = value;                                                          \
        break;                                                                 \
      default:                                                                 \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar getParam(int index) {                                                \
      switch (index) {                                                         \
      case 0:                                                                  \
      case 1:                                                                  \
        return Variable<CBType::_CBT_##_CMP_>::getParam(index);                \
      case 2:                                                                  \
        return _min;                                                           \
      case 3:                                                                  \
        return _max;                                                           \
      default:                                                                 \
        return Var::Empty;                                                     \
      }                                                                        \
    }                                                                          \
                                                                               \
    CBVar activate(CBContext *context, const CBVar &input) {                   \
      IDContext idCtx(this);                                                   \
                                                                               \
      if (!_variable && _variable_name.size() > 0) {                           \
        _variable = referenceVariable(context, _variable_name.c_str());        \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_##_CMP_;                                 \
        }                                                                      \
      }                                                                        \
                                                                               \
      _T_ min = _min.get().payload._VAL_##Value;                               \
      _T_ max = _max.get().payload._VAL_##Value;                               \
      if (_variable) {                                                         \
        ::ImGui::SliderScalarN(                                                \
            _label.c_str(), _IMT_,                                             \
            (void *)&_variable->payload._VAL_##_CMP_##Value, _CMP_, &min,      \
            &max, _FMT_, 0);                                                   \
        return *_variable;                                                     \
      } else {                                                                 \
        ::ImGui::SliderScalarN(_label.c_str(), _IMT_, (void *)&_tmp, _CMP_,    \
                               &min, &max, _FMT_, 0);                          \
        return Var(_tmp);                                                      \
      }                                                                        \
    }                                                                          \
                                                                               \
  private:                                                                     \
    static inline Parameters paramsInfo{                                       \
        VariableParamsInfo(),                                                  \
        {{"Min",                                                               \
          CBCCSTR("The minimum value."),                                       \
          {CoreInfo::_CBT_##Type, CoreInfo::_CBT_##VarType}},                  \
         {"Max",                                                               \
          CBCCSTR("The maximum value."),                                       \
          {CoreInfo::_CBT_##Type, CoreInfo::_CBT_##VarType}}},                 \
    };                                                                         \
                                                                               \
    ParamVar _min{Var((_T_)0)};                                                \
    ParamVar _max{Var((_T_)100)};                                              \
    _T_ _tmp;                                                                  \
  }

IMGUISLIDER2(Int, 2, int64_t, ImGuiDataType_S64, int, "%lld");
IMGUISLIDER2(Int, 3, int32_t, ImGuiDataType_S32, int, "%lld");
IMGUISLIDER2(Int, 4, int32_t, ImGuiDataType_S32, int, "%lld");

IMGUISLIDER2(Float, 2, double, ImGuiDataType_Double, float, "%.3f");
IMGUISLIDER2(Float, 3, float, ImGuiDataType_Float, float, "%.3f");
IMGUISLIDER2(Float, 4, float, ImGuiDataType_Float, float, "%.3f");

struct TextInput : public Variable<CBType::String> {

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
    case 1:
      Variable<CBType::String>::setParam(index, value);
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
    case 1:
      return Variable<CBType::String>::getParam(index);
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
      if (it->_variable) {
        delete[] it->_variable->payload.stringValue;
        it->_variable->payload.stringCapacity = data->BufTextLen * 2;
        it->_variable->payload.stringValue =
            new char[it->_variable->payload.stringCapacity];
        data->Buf = (char *)it->_variable->payload.stringValue;
      } else {
        it->_buffer.resize(data->BufTextLen * 2);
        data->Buf = (char *)it->_buffer.c_str();
      }
    }
    return 0;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_variable && _variable_name.size() > 0) {
      _variable = referenceVariable(context, _variable_name.c_str());
      if (_exposing) {
        // we own the variable so let's run some init
        _variable->valueType = String;
        _variable->payload.stringValue = new char[32];
        _variable->payload.stringCapacity = 32;
        memset((void *)_variable->payload.stringValue, 0x0, 32);
      }
    }

    auto *hint = _hint.size() > 0 ? _hint.c_str() : nullptr;
    if (_variable) {
      ::ImGui::InputTextWithHint(
          _label.c_str(), hint, (char *)_variable->payload.stringValue,
          _variable->payload.stringCapacity, ImGuiInputTextFlags_CallbackResize,
          &InputTextCallback, this);
      return *_variable;
    } else {
      ::ImGui::InputTextWithHint(
          _label.c_str(), hint, (char *)_buffer.c_str(), _buffer.capacity() + 1,
          ImGuiInputTextFlags_CallbackResize, &InputTextCallback, this);
      return Var(_buffer);
    }
  }

private:
  static inline Parameters _params{
      VariableParamsInfo(),
      {{"Hint",
        CBCCSTR("A hint text displayed when the control is empty."),
        {CoreInfo::StringType}}},
  };

  // fallback, used only when no variable name is set
  std::string _buffer;
  std::string _hint;
};

struct ColorInput : public Variable<CBType::Color> {
  ImVec4 _lcolor{0.0, 0.0, 0.0, 1.0};

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::ColorType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_variable && _variable_name.size() > 0) {
      _variable = referenceVariable(context, _variable_name.c_str());
      if (_exposing) {
        // we own the variable so let's run some init
        _variable->valueType = Color;
        _variable->payload.colorValue.r = 0;
        _variable->payload.colorValue.g = 0;
        _variable->payload.colorValue.b = 0;
        _variable->payload.colorValue.a = 255;
      }
    }

    if (_variable) {
      auto fc = Style::color2Vec4(*_variable);
      ::ImGui::ColorEdit4(_label.c_str(), &fc.x);
      _variable->payload.colorValue = Style::vec42Color(fc);
      return *_variable;
    } else {
      ::ImGui::ColorEdit4(_label.c_str(), &_lcolor.x);
      return Var(Style::vec42Color(_lcolor));
    }
  }
};

struct Image : public Base {
  ImVec2 _size{1.0, 1.0};
  bool _trueSize = false;

  static CBTypesInfo inputTypes() { return BGFX::Texture::ObjType; }

  static CBTypesInfo outputTypes() { return BGFX::Texture::ObjType; }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Size", CBCCSTR("The drawing size of the image."),
                        CoreInfo::Float2Type),
      ParamsInfo::Param("TrueSize",
                        CBCCSTR("If the given size is in true image pixels."),
                        CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_size.x, _size.y);
    case 1:
      return Var(_trueSize);
    default:
      return Var::Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
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

  static inline Types Plottable{
      {CoreInfo::FloatSeqType, CoreInfo::Float2SeqType}};

  BlocksVar _blocks;
  CBVar _width{}, _height{};
  std::string _title;
  std::string _fullTitle{"##" +
                         std::to_string(reinterpret_cast<uintptr_t>(this))};
  std::string _xlabel;
  std::string _ylabel;
  ParamVar _xlimits{}, _ylimits{};
  ParamVar _lockx{Var::False}, _locky{Var::False};
  std::array<CBExposedTypeInfo, 5> _required;

  static inline Parameters params{
      {"Title",
       CBCCSTR("The title of the plot to create."),
       {CoreInfo::StringType}},
      {"Contents",
       CBCCSTR("The blocks describing this plot."),
       {CoreInfo::BlocksOrNone}},
      {"Width",
       CBCCSTR("The width of the plot area to create."),
       {CoreInfo::IntOrNone}},
      {"Height",
       CBCCSTR("The height of the plot area to create."),
       {CoreInfo::IntOrNone}},
      {"X_Label", CBCCSTR("The X axis label."), {CoreInfo::StringType}},
      {"Y_Label", CBCCSTR("The Y axis label."), {CoreInfo::StringType}},
      {"X_Limits",
       CBCCSTR("The X axis limits."),
       {CoreInfo::NoneType, CoreInfo::Float2Type, CoreInfo::Float2VarType}},
      {"Y_Limits",
       CBCCSTR("The Y axis limits."),
       {CoreInfo::NoneType, CoreInfo::Float2Type, CoreInfo::Float2VarType}},
      {"Lock_X",
       CBCCSTR("If the X axis should be locked into its limits."),
       {CoreInfo::BoolType, CoreInfo::BoolVarType}},
      {"Lock_Y",
       CBCCSTR("If the Y axis should be locked into its limits."),
       {CoreInfo::BoolType, CoreInfo::BoolVarType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      _fullTitle =
          _title + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
      break;
    case 1:
      _blocks = value;
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_title);
    case 1:
      return _blocks;
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

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return data.inputType;
  }

  void cleanup() {
    _blocks.cleanup();
    _xlimits.cleanup();
    _ylimits.cleanup();
    _lockx.cleanup();
    _locky.cleanup();
  }

  void warmup(CBContext *context) {
    _context();
    _blocks.warmup(context);
    _xlimits.warmup(context);
    _ylimits.warmup(context);
    _lockx.warmup(context);
    _locky.warmup(context);
  }

  CBExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = Base::ContextInfo;
    idx++;

    if (_xlimits.isVariable()) {
      _required[idx].name = _xlimits.variableName();
      _required[idx].help = CBCCSTR("The required X axis limits.");
      _required[idx].exposedType = CoreInfo::Float2Type;
      idx++;
    }

    if (_ylimits.isVariable()) {
      _required[idx].name = _ylimits.variableName();
      _required[idx].help = CBCCSTR("The required Y axis limits.");
      _required[idx].exposedType = CoreInfo::Float2Type;
      idx++;
    }

    if (_lockx.isVariable()) {
      _required[idx].name = _lockx.variableName();
      _required[idx].help = CBCCSTR("The required X axis locking.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    if (_locky.isVariable()) {
      _required[idx].name = _locky.variableName();
      _required[idx].help = CBCCSTR("The required Y axis locking.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (_xlimits.get().valueType == Float2) {
      auto limitx = _xlimits.get().payload.float2Value[0];
      auto limity = _xlimits.get().payload.float2Value[1];
      auto locked = _lockx.get().payload.boolValue;
      ImPlot::SetNextPlotLimitsX(limitx, limity,
                                 locked ? ImGuiCond_Always : ImGuiCond_Once);
    }

    if (_ylimits.get().valueType == Float2) {
      auto limitx = _ylimits.get().payload.float2Value[0];
      auto limity = _ylimits.get().payload.float2Value[1];
      auto locked = _locky.get().payload.boolValue;
      ImPlot::SetNextPlotLimitsY(limitx, limity,
                                 locked ? ImGuiCond_Always : ImGuiCond_Once);
    }

    ImVec2 size{0, 0};
    if (_width.valueType == Int) {
      size.x = float(_width.payload.intValue);
    }
    if (_height.valueType == Int) {
      size.y = float(_height.payload.intValue);
    }

    if (ImPlot::BeginPlot(
            _fullTitle.c_str(), _xlabel.size() > 0 ? _xlabel.c_str() : nullptr,
            _ylabel.size() > 0 ? _ylabel.c_str() : nullptr, size)) {
      DEFER(ImPlot::EndPlot());

      CBVar output{};
      _blocks.activate(context, input, output);
    }

    return input;
  }
};

struct PlottableBase : public Base {
  std::string _label;
  std::string _fullLabel{"##" +
                         std::to_string(reinterpret_cast<uintptr_t>(this))};
  enum class Kind { unknown, xAndIndex, xAndY };
  Kind _kind{};
  CBVar _color{};

  static CBTypesInfo inputTypes() { return Plot::Plottable; }
  static CBTypesInfo outputTypes() { return Plot::Plottable; }

  static constexpr int nparams = 2;
  static inline Parameters params{
      {"Label", CBCCSTR("The plot's label."), {CoreInfo::StringType}},
      {"Color",
       CBCCSTR("The plot's color."),
       {CoreInfo::NoneType, CoreInfo::ColorType}}};
  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      _fullLabel =
          _label + "##" + std::to_string(reinterpret_cast<uintptr_t>(this));
      break;
    case 1:
      _color = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_label);
    case 1:
      return _color;
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    assert(data.inputType.basicType == Seq);
    if (data.inputType.seqTypes.len != 1 ||
        (data.inputType.seqTypes.elements[0].basicType != Float &&
         data.inputType.seqTypes.elements[0].basicType != Float2)) {
      throw ComposeError("Expected either a Float or Float2 sequence.");
    }

    if (data.inputType.seqTypes.elements[0].basicType == Float)
      _kind = Kind::xAndIndex;
    else
      _kind = Kind::xAndY;

    return data.inputType;
  }

  void applyModifiers() {
    if (_color.valueType == CBType::Color) {
      ImPlot::PushStyleColor(ImPlotCol_Line,
                             Style::color2Vec4(_color.payload.colorValue));
      ImPlot::PushStyleColor(ImPlotCol_Fill,
                             Style::color2Vec4(_color.payload.colorValue));
    }
  }

  void popModifiers() {
    if (_color.valueType == CBType::Color) {
      ImPlot::PopStyleColor(2);
    }
  }

  static bool isInputValid(const CBVar &input) {
    // prevent division by zero
    return !(input.valueType == Seq && input.payload.seqValue.len == 0);
  }
};

struct PlotLine : public PlottableBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    if (!isInputValid(input))
      return input;

    PlottableBase::applyModifiers();
    DEFER(PlottableBase::popModifiers());
    if (_kind == Kind::xAndY) {
      ImPlot::PlotLineG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<CBVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(seq.elements[idx].payload.float2Value[0],
                               seq.elements[idx].payload.float2Value[1]);
          },
          (void *)&input, int(input.payload.seqValue.len), 0);
    } else if (_kind == Kind::xAndIndex) {
      ImPlot::PlotLineG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<CBVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(double(idx),
                               seq.elements[idx].payload.floatValue);
          },
          (void *)&input, int(input.payload.seqValue.len), 0);
    }
    return input;
  }
};

struct PlotDigital : public PlottableBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    if (!isInputValid(input))
      return input;

    PlottableBase::applyModifiers();
    DEFER(PlottableBase::popModifiers());
    if (_kind == Kind::xAndY) {
      ImPlot::PlotDigitalG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<CBVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(seq.elements[idx].payload.float2Value[0],
                               seq.elements[idx].payload.float2Value[1]);
          },
          (void *)&input, int(input.payload.seqValue.len), 0);
    } else if (_kind == Kind::xAndIndex) {
      ImPlot::PlotDigitalG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<CBVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(double(idx),
                               seq.elements[idx].payload.floatValue);
          },
          (void *)&input, int(input.payload.seqValue.len), 0);
    }
    return input;
  }
};

struct PlotScatter : public PlottableBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    if (!isInputValid(input))
      return input;

    PlottableBase::applyModifiers();
    DEFER(PlottableBase::popModifiers());
    if (_kind == Kind::xAndY) {
      ImPlot::PlotScatterG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<CBVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(seq.elements[idx].payload.float2Value[0],
                               seq.elements[idx].payload.float2Value[1]);
          },
          (void *)&input, int(input.payload.seqValue.len), 0);
    } else if (_kind == Kind::xAndIndex) {
      ImPlot::PlotScatterG(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<CBVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(double(idx),
                               seq.elements[idx].payload.floatValue);
          },
          (void *)&input, int(input.payload.seqValue.len), 0);
    }
    return input;
  }
};

struct PlotBars : public PlottableBase {
  typedef void (*PlotBarsProc)(const char *label_id,
                               ImPlotPoint (*getter)(void *data, int idx),
                               void *data, int count, double width, int offset);

  double _width = 0.67;
  bool _horizontal = false;
  PlotBarsProc _plot = &ImPlot::PlotBarsG;

  static inline Parameters params{
      PlottableBase::params,
      {{"Width", CBCCSTR("The width of each bar"), {CoreInfo::FloatType}},
       {"Horizontal",
        CBCCSTR("If the bar should be horiziontal rather than vertical"),
        {CoreInfo::BoolType}}},
  };

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!isInputValid(input))
      return input;

    PlottableBase::applyModifiers();
    DEFER(PlottableBase::popModifiers());
    if (_kind == Kind::xAndY) {
      _plot(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<CBVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(seq.elements[idx].payload.float2Value[0],
                               seq.elements[idx].payload.float2Value[1]);
          },
          (void *)&input, int(input.payload.seqValue.len), _width, 0);
    } else if (_kind == Kind::xAndIndex) {
      _plot(
          _fullLabel.c_str(),
          [](void *data, int idx) {
            auto input = reinterpret_cast<CBVar *>(data);
            auto seq = input->payload.seqValue;
            return ImPlotPoint(double(idx),
                               seq.elements[idx].payload.floatValue);
          },
          (void *)&input, int(input.payload.seqValue.len), _width, 0);
    }
    return input;
  }
};

struct HasPointer : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }
  static CBVar activate(CBContext *context, const CBVar &input) {
    return Var(::ImGui::IsAnyItemHovered());
  }
};

struct FPS : public Base {
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    ImGuiIO &io = ::ImGui::GetIO();
    return Var(io.Framerate);
  }
};

struct Version : public Base {
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    return Var(IMGUI_VERSION);
  }
};

struct Tooltip : public Base {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _blocks = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _blocks;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { _blocks.cleanup(); }

  void warmup(CBContext *context) { _blocks.warmup(context); }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (::ImGui::IsItemHovered()) {
      ::ImGui::BeginTooltip();
      DEFER(::ImGui::EndTooltip());

      CBVar output{};
      _blocks.activate(context, input, output);
    }
    return input;
  }

private:
  static inline Parameters _params = {
      {"Contents",
       CBCCSTR("The inner contents blocks."),
       {CoreInfo::BlocksOrNone}},
  };

  BlocksVar _blocks{};
};

struct HelpMarker : public Base {

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _desc.size() == 0 ? Var::Empty : Var(_desc);
    case 1:
      return Var(_isInline);
    default:
      return Var::Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
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
      {"Description",
       CBCCSTR("The text displayed in a popup."),
       {CoreInfo::StringType}},
      {"Inline", CBCCSTR("Display on the same line."), {CoreInfo::BoolType}},
  };

  std::string _desc;
  bool _isInline{true};
};

struct ProgressBar : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::FloatType; }

  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _overlay.size() == 0 ? Var::Empty : Var(_overlay);
    default:
      return Var::Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto buf = _overlay.size() > 0 ? _overlay.c_str() : nullptr;
    ::ImGui::ProgressBar(input.payload.floatValue, ImVec2(0.f, 0.f), buf);
    return input;
  }

private:
  static inline Parameters _params = {
      {"Overlay",
       CBCCSTR("The text displayed inside the progress bar."),
       {CoreInfo::StringType}},
  };

  std::string _overlay;
};

struct MenuBase : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      blocks = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return blocks;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { blocks.cleanup(); }

  void warmup(CBContext *context) { blocks.warmup(context); }

  CBTypeInfo compose(const CBInstanceData &data) {
    blocks.compose(data);
    return CoreInfo::BoolType;
  }

protected:
  static inline Parameters params = {
      {"Contents", CBCCSTR("The inner contents blocks."),
       CoreInfo::BlocksOrNone},
  };

  BlocksVar blocks{};
};

struct MainMenuBar : public MenuBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto active = ::ImGui::BeginMainMenuBar();
    if (active) {
      DEFER(::ImGui::EndMainMenuBar());
      CBVar output{};
      blocks.activate(context, input, output);
    }
    return Var(active);
  }
};

struct MenuBar : public MenuBase {
  CBVar activate(CBContext *context, const CBVar &input) {
    auto active = ::ImGui::BeginMenuBar();
    if (active) {
      DEFER(::ImGui::EndMenuBar());
      CBVar output{};
      blocks.activate(context, input, output);
    }
    return Var(active);
  }
};

struct Menu : public MenuBase {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  void warmup(CBContext *context) {
    MenuBase::warmup(context);

    _isEnabled.warmup(context);
  }

  CBExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = Base::ContextInfo;
    idx++;

    if (_isEnabled.isVariable()) {
      _required[idx].name = _isEnabled.variableName();
      _required[idx].help = CBCCSTR("The required IsEnabled.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto active =
        ::ImGui::BeginMenu(_label.c_str(), _isEnabled.get().payload.boolValue);
    if (active) {
      DEFER(::ImGui::EndMenu());
      CBVar output{};
      blocks.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params{
      {{"Label", CBCCSTR("The label of the menu"), {CoreInfo::StringType}},
       {"IsEnabled",
        CBCCSTR("Sets whether this menu is enabled. A disabled item cannot be "
                "selected or clicked."),
        {CoreInfo::BoolType, CoreInfo::BoolVarType}}},
      MenuBase::params};

  std::string _label;
  ParamVar _isEnabled{Var::True};
  std::array<CBExposedTypeInfo, 2> _required;
};

struct MenuItem : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  void warmup(CBContext *context) {
    _action.warmup(context);
    _isEnabled.warmup(context);
    _isChecked.warmup(context);
  }

  CBExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = Base::ContextInfo;
    idx++;

    if (_isChecked.isVariable()) {
      _required[idx].name = _isChecked.variableName();
      _required[idx].help = CBCCSTR("The required IsChecked.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    if (_isEnabled.isVariable()) {
      _required[idx].name = _isEnabled.variableName();
      _required[idx].help = CBCCSTR("The required IsEnabled.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _action.compose(data);

    if (_isChecked.isVariable()) {
      // search for an existing variable and ensure it's the right type
      auto found = false;
      for (auto &var : data.shared) {
        if (strcmp(var.name, _isChecked.variableName()) == 0) {
          // we found a variable, make sure it's the right type and mark
          if (var.exposedType.basicType != CBType::Bool) {
            throw CBException("Gui - MenuItem: Existing variable type not "
                              "matching the input.");
          }
          // also make sure it's mutable!
          if (!var.isMutable) {
            throw CBException(
                "Gui - MenuItem: Existing variable is not mutable.");
          }
          found = true;
          break;
        }
      }
      if (!found)
        // we didn't find a variable
        throw CBException("Gui - MenuItem: Missing mutable variable.");
    }

    return CoreInfo::BoolType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    bool active;
    if (_isChecked.isVariable()) {
      active = ::ImGui::MenuItem(_label.c_str(), _shortcut.c_str(),
                                 &_isChecked.get().payload.boolValue,
                                 _isEnabled.get().payload.boolValue);
    } else {
      active = ::ImGui::MenuItem(_label.c_str(), _shortcut.c_str(),
                                 _isChecked.get().payload.boolValue,
                                 _isEnabled.get().payload.boolValue);
    }
    if (active) {
      CBVar output{};
      _action.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params{
      {"Label", CBCCSTR("The label of the menu item"), {CoreInfo::StringType}},
      {"IsChecked",
       CBCCSTR("Sets whether this menu item is checked. A checked item "
               "displays a check mark on the side."),
       {CoreInfo::BoolType, CoreInfo::BoolVarType}},
      {"Action", CBCCSTR(""), {CoreInfo::BlocksOrNone}},
      {"Shortcut",
       CBCCSTR("A keyboard shortcut to activate that item"),
       {CoreInfo::StringType}},
      {"IsEnabled",
       CBCCSTR("Sets whether this menu item is enabled. A disabled item cannot "
               "be selected or clicked."),
       {CoreInfo::BoolType, CoreInfo::BoolVarType}},
  };

  std::string _label;
  ParamVar _isChecked{Var::False};
  BlocksVar _action{};
  std::string _shortcut;
  ParamVar _isEnabled{Var::True};
  std::array<CBExposedTypeInfo, 3> _required;
};

struct Combo : public Variable<CBType::Int> {
  static CBTypesInfo inputTypes() { return CoreInfo::AnySeqType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_variable && _variable_name.size() > 0) {
      _variable = referenceVariable(context, _variable_name.c_str());
    }

    auto count = input.payload.seqValue.len;
    if (count == 0) {
      if (_variable)
        _variable->payload.intValue = -1;
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

    if (_variable) {
      _n = _variable->payload.intValue;
      ::ImGui::Combo(_label.c_str(), &_n, items, count);
      _variable->payload.intValue = _n;
    } else {
      ::ImGui::Combo(_label.c_str(), &_n, items, count);
    }

    CBVar output{};
    ::chainblocks::cloneVar(output, input.payload.seqValue.elements[_n]);
    return output;
  }

private:
  int _n{0};
};

struct ListBox : public Variable<CBType::Int> {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
    case 1:
      Variable<CBType::Int>::setParam(index, value);
      break;
    case 2:
      _height = value.payload.intValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
    case 1:
      return Variable<CBType::Int>::getParam(index);
    case 2:
      return Var(_height);
    default:
      break;
    }
    return Var::Empty;
  }

  static CBTypesInfo inputTypes() { return CoreInfo::AnySeqType; }

  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_variable && _variable_name.size() > 0) {
      _variable = referenceVariable(context, _variable_name.c_str());
    }

    auto count = input.payload.seqValue.len;
    if (count == 0) {
      if (_variable)
        _variable->payload.intValue = -1;
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

    if (_variable) {
      _n = _variable->payload.intValue;
      ::ImGui::ListBox(_label.c_str(), &_n, items, count, _height);
      _variable->payload.intValue = _n;
    } else {
      ::ImGui::ListBox(_label.c_str(), &_n, items, count, _height);
    }

    CBVar output{};
    ::chainblocks::cloneVar(output, input.payload.seqValue.elements[_n]);
    return output;
  }

private:
  static inline Parameters _params = {
      VariableParamsInfo(),
      {{"ItemsHeight",
        CBCCSTR("Height of the list in number of items"),
        {CoreInfo::IntType}}},
  };

  int _n{0};
  int _height{-1};
};

struct Selectable : public Variable<CBType::Bool> {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_variable && _variable_name.size() > 0) {
      _variable = referenceVariable(context, _variable_name.c_str());
    }

    auto result = Var::False;
    if (_variable) {
      if (::ImGui::Selectable(_label.c_str(), &_variable->payload.boolValue)) {
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
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      blocks = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return blocks;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { blocks.cleanup(); }

  void warmup(CBContext *context) { blocks.warmup(context); }

  CBTypeInfo compose(const CBInstanceData &data) {
    blocks.compose(data);
    return CoreInfo::BoolType;
  }

protected:
  static inline Parameters params = {
      {"Contents", CBCCSTR("The inner contents blocks."),
       CoreInfo::BlocksOrNone},
  };

  BlocksVar blocks{};
};

struct TabBar : public TabBase {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  CBVar activate(CBContext *context, const CBVar &input) {
    auto str_id = _name.size() > 0 ? _name.c_str() : "DefaultTabBar";
    auto active = ::ImGui::BeginTabBar(str_id);
    if (active) {
      DEFER(::ImGui::EndTabBar());
      CBVar output{};
      blocks.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params = {
      {{"Name",
        CBCCSTR("A unique name for this tab bar."),
        {CoreInfo::StringType}}},
      TabBase::params,
  };

  std::string _name;
};

struct TabItem : public TabBase {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  CBVar activate(CBContext *context, const CBVar &input) {
    auto active = ::ImGui::BeginTabItem(_label.c_str());
    if (active) {
      DEFER(::ImGui::EndTabItem());
      CBVar output{};
      blocks.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params = {
      {{"Label", CBCCSTR("The label of the tab"), {CoreInfo::StringType}}},
      TabBase::params,
  };

  std::string _label;
};

struct Group : public Base {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _blocks = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _blocks;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() { _blocks.cleanup(); }

  void warmup(CBContext *context) { _blocks.warmup(context); }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return CoreInfo::BoolType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::BeginGroup();
    DEFER(::ImGui::EndGroup());
    CBVar output{};
    _blocks.activate(context, input, output);
    return input;
  }

private:
  static inline Parameters _params = {
      {"Contents", CBCCSTR("The inner contents blocks."),
       CoreInfo::BlocksOrNone},
  };

  BlocksVar _blocks{};
};

struct Disable : public Base {
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _disable = value;
      break;
    case 1:
      _blocks = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _disable;
    case 1:
      return _blocks;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() {
    _disable.cleanup();
    _blocks.cleanup();
  }

  void warmup(CBContext *context) {
    _disable.warmup(context);
    _blocks.warmup(context);
  }

  CBExposedTypesInfo requiredVariables() {
    int idx = 0;
    _required[idx] = Base::ContextInfo;
    idx++;

    if (_disable.isVariable()) {
      _required[idx].name = _disable.variableName();
      _required[idx].help = CBCCSTR("The required Disable.");
      _required[idx].exposedType = CoreInfo::BoolType;
      idx++;
    }

    return {_required.data(), uint32_t(idx), 0};
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return data.inputType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::BeginDisabled(_disable.get().payload.boolValue);
    DEFER(::ImGui::EndDisabled());
    CBVar output{};
    _blocks.activate(context, input, output);
    return input;
  }

private:
  static inline Parameters _params = {
      {"Disable",
       CBCCSTR("Sets whether the contents should be disabled."),
       {CoreInfo::BoolType, CoreInfo::BoolVarType}},
      {"Contents",
       CBCCSTR("The inner contents blocks."),
       {CoreInfo::BlocksOrNone}},
  };

  ParamVar _disable{Var::True};
  BlocksVar _blocks{};
  std::array<CBExposedTypeInfo, 2> _required;
};

struct Table : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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
      _blocks = value;
      break;
    case 3:
      _flags = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_name);
    case 1:
      return Var(_columns);
    case 2:
      return _blocks;
    case 3:
      return _flags;
    default:
      break;
    }
    return Var::Empty;
  }

  void cleanup() {
    _blocks.cleanup();
    _flags.cleanup();
  }

  void warmup(CBContext *context) {
    _blocks.warmup(context);
    _flags.warmup(context);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.compose(data);
    return CoreInfo::BoolType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto flags = ::ImGuiTableFlags(_flags.get().payload.enumValue);
    auto str_id = _name.size() > 0 ? _name.c_str() : "DefaultTable";
    auto active = ::ImGui::BeginTable(str_id, _columns, flags);
    if (active) {
      DEFER(::ImGui::EndTable());
      CBVar output{};
      _blocks.activate(context, input, output);
    }
    return Var(active);
  }

private:
  static inline Parameters _params = {
      {"Name",
       CBCCSTR("A unique name for this table."),
       {CoreInfo::StringType}},
      {"Columns", CBCCSTR("The number of columns."), {CoreInfo::IntType}},
      {"Contents", CBCCSTR("The inner contents blocks."),
       CoreInfo::BlocksOrNone},
      {"Flags",
       CBCCSTR("Flags to enable table options."),
       {Enums::GuiTableFlagsType, Type::VariableOf(Enums::GuiTableFlagsType)}},
  };

  std::string _name;
  int _columns;
  BlocksVar _blocks{};
  ParamVar _flags{};
};

struct TableHeadersRow : public Base {
  CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::TableHeadersRow();
    return input;
  }
};

struct TableNextColumn : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto active = ::ImGui::TableNextColumn();
    return Var(active);
  }
};

struct TableNextRow : public Base {
  CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::TableNextRow();
    return input;
  }
};

struct TableSetColumnIndex : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::IntType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto &index = input.payload.intValue;
    auto active = ::ImGui::TableSetColumnIndex(index);
    return Var(active);
  }
};

struct TableSetupColumn : public Base {

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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

  void warmup(CBContext *context) { _flags.warmup(context); }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto flags = ::ImGuiTableColumnFlags(_flags.get().payload.enumValue);
    ::ImGui::TableSetupColumn(_label.c_str(), flags);
    return input;
  }

private:
  static inline Parameters _params = {
      {"Label", CBCCSTR("Column header"), {CoreInfo::StringType}},
      {"Flags",
       CBCCSTR("Flags to enable column options."),
       {Enums::GuiTableColumnFlagsType,
        Type::VariableOf(Enums::GuiTableColumnFlagsType)}},
  };

  std::string _label;
  ParamVar _flags{};
};

void registerImGuiBlocks() {
  REGISTER_CBLOCK("GUI.Style", Style);
  REGISTER_CBLOCK("GUI.Window", Window);
  REGISTER_CBLOCK("GUI.ChildWindow", ChildWindow);
  REGISTER_CBLOCK("GUI.Checkbox", Checkbox);
  REGISTER_CBLOCK("GUI.CheckboxFlags", CheckboxFlags);
  REGISTER_CBLOCK("GUI.RadioButton", RadioButton);
  REGISTER_CBLOCK("GUI.Text", Text);
  REGISTER_CBLOCK("GUI.Bullet", Bullet);
  REGISTER_CBLOCK("GUI.Button", Button);
  REGISTER_CBLOCK("GUI.HexViewer", HexViewer);
  REGISTER_CBLOCK("GUI.Dummy", Dummy);
  REGISTER_CBLOCK("GUI.NewLine", NewLine);
  REGISTER_CBLOCK("GUI.SameLine", SameLine);
  REGISTER_CBLOCK("GUI.Separator", Separator);
  REGISTER_CBLOCK("GUI.Spacing", Spacing);
  REGISTER_CBLOCK("GUI.Indent", Indent);
  REGISTER_CBLOCK("GUI.Unindent", Unindent);
  REGISTER_CBLOCK("GUI.TreeNode", TreeNode);
  REGISTER_CBLOCK("GUI.CollapsingHeader", CollapsingHeader);
  REGISTER_CBLOCK("GUI.IntInput", IntInput);
  REGISTER_CBLOCK("GUI.FloatInput", FloatInput);
  REGISTER_CBLOCK("GUI.Int2Input", Int2Input);
  REGISTER_CBLOCK("GUI.Int3Input", Int3Input);
  REGISTER_CBLOCK("GUI.Int4Input", Int4Input);
  REGISTER_CBLOCK("GUI.Float2Input", Float2Input);
  REGISTER_CBLOCK("GUI.Float3Input", Float3Input);
  REGISTER_CBLOCK("GUI.Float4Input", Float4Input);
  REGISTER_CBLOCK("GUI.IntDrag", IntDrag);
  REGISTER_CBLOCK("GUI.FloatDrag", FloatDrag);
  REGISTER_CBLOCK("GUI.Int2Drag", Int2Drag);
  REGISTER_CBLOCK("GUI.Int3Drag", Int3Drag);
  REGISTER_CBLOCK("GUI.Int4Drag", Int4Drag);
  REGISTER_CBLOCK("GUI.Float2Drag", Float2Drag);
  REGISTER_CBLOCK("GUI.Float3Drag", Float3Drag);
  REGISTER_CBLOCK("GUI.Float4Drag", Float4Drag);
  REGISTER_CBLOCK("GUI.IntSlider", IntSlider);
  REGISTER_CBLOCK("GUI.FloatSlider", FloatSlider);
  REGISTER_CBLOCK("GUI.Int2Slider", Int2Slider);
  REGISTER_CBLOCK("GUI.Int3Slider", Int3Slider);
  REGISTER_CBLOCK("GUI.Int4Slider", Int4Slider);
  REGISTER_CBLOCK("GUI.Float2Slider", Float2Slider);
  REGISTER_CBLOCK("GUI.Float3Slider", Float3Slider);
  REGISTER_CBLOCK("GUI.Float4Slider", Float4Slider);
  REGISTER_CBLOCK("GUI.TextInput", TextInput);
  REGISTER_CBLOCK("GUI.Image", Image);
  REGISTER_CBLOCK("GUI.Plot", Plot);
  REGISTER_CBLOCK("GUI.PlotLine", PlotLine);
  REGISTER_CBLOCK("GUI.PlotDigital", PlotDigital);
  REGISTER_CBLOCK("GUI.PlotScatter", PlotScatter);
  REGISTER_CBLOCK("GUI.PlotBars", PlotBars);
  REGISTER_CBLOCK("GUI.GetClipboard", GetClipboard);
  REGISTER_CBLOCK("GUI.SetClipboard", SetClipboard);
  REGISTER_CBLOCK("GUI.ColorInput", ColorInput);
  REGISTER_CBLOCK("GUI.HasPointer", HasPointer);
  REGISTER_CBLOCK("GUI.FPS", FPS);
  REGISTER_CBLOCK("GUI.Version", Version);
  REGISTER_CBLOCK("GUI.Tooltip", Tooltip);
  REGISTER_CBLOCK("GUI.HelpMarker", HelpMarker);
  REGISTER_CBLOCK("GUI.ProgressBar", ProgressBar);
  REGISTER_CBLOCK("GUI.MainMenuBar", MainMenuBar);
  REGISTER_CBLOCK("GUI.MenuBar", MenuBar);
  REGISTER_CBLOCK("GUI.Menu", Menu);
  REGISTER_CBLOCK("GUI.MenuItem", MenuItem);
  REGISTER_CBLOCK("GUI.Combo", Combo);
  REGISTER_CBLOCK("GUI.ListBox", ListBox);
  REGISTER_CBLOCK("GUI.Selectable", Selectable);
  REGISTER_CBLOCK("GUI.TabBar", TabBar);
  REGISTER_CBLOCK("GUI.TabItem", TabItem);
  REGISTER_CBLOCK("GUI.Group", Group);
  REGISTER_CBLOCK("GUI.Disable", Disable);
  REGISTER_CBLOCK("GUI.Table", Table);
  REGISTER_CBLOCK("GUI.HeadersRow", TableHeadersRow);
  REGISTER_CBLOCK("GUI.NextColumn", TableNextColumn);
  REGISTER_CBLOCK("GUI.NextRow", TableNextRow);
  REGISTER_CBLOCK("GUI.SetColumnIndex", TableSetColumnIndex);
  REGISTER_CBLOCK("GUI.SetupColumn", TableSetupColumn);
}
}; // namespace ImGui
}; // namespace chainblocks
