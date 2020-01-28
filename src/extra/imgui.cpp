/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "imgui.hpp"
#include "bgfx.hpp"
#include "blocks/shared.hpp"
#include "runtime.hpp"

using namespace chainblocks;

namespace chainblocks {
namespace ImGui {
struct Base {
  CBVar *_context;

  static inline ExposedInfo consumedInfo = ExposedInfo(ExposedInfo::Variable(
      "ImGui.Context", "The ImGui Context.", Context::Info));

  CBExposedTypesInfo consumedVariables() {
    return CBExposedTypesInfo(consumedInfo);
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
      {CBType::Enum, {.enumeration = {.vendorId = 'frag', .typeId = 'ImGS'}}}};

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
  static inline ImGuiStyleInfo imguiEnumInfo{"ImGuiStyle", 'frag', 'ImGS'};

  ImGuiStyle _key{};

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Style", "A style key to set.", styleEnumInfo));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
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
      return Var::Enum(_key, 'frag', 'ImGS');
    default:
      return Empty;
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
  int _curX = -1, _curY = -1, _curW = -1, _curH = -1;
  CBVar _posX{}, _posY{}, _width{}, _height{};
  CBValidationResult _validation{};

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Title", "The title of the window to create.",
                        CoreInfo::StringType),
      ParamsInfo::Param("PosX",
                        "The horizontal position of the window to create",
                        CoreInfo::IntOrNone),
      ParamsInfo::Param("PosY",
                        "The vertical position of the window to create.",
                        CoreInfo::IntOrNone),
      ParamsInfo::Param("Width", "The width of the window to create",
                        CoreInfo::IntOrNone),
      ParamsInfo::Param("Height", "The height of the window to create.",
                        CoreInfo::IntOrNone),
      ParamsInfo::Param("Contents", "The inner contents blocks.",
                        CoreInfo::BlocksOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blks.validate(data);
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() { return _validation.exposedInfo; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _title = value.payload.stringValue;
      break;
    case 1:
      _posX = value;
      break;
    case 2:
      _posY = value;
      break;
    case 3:
      _width = value;
      break;
    case 4:
      _height = value;
      break;
    case 5:
      _blks = value;
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
      return _posX;
    case 2:
      return _posY;
    case 3:
      return _posX;
    case 4:
      return _posY;
    case 5:
      return _blks;
    default:
      return Empty;
    }
  }

  void cleanup() {
    _blks.reset();
    _curX = -1;
    _curY = -1;
    _context = nullptr;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_blks)
      return input;

    int flags = ImGuiWindowFlags_NoSavedSettings;

    if (_posX.valueType == Int && _posY.valueType == Int) {
      if ((_curX != _posX.payload.intValue) ||
          (_curY != _posY.payload.intValue)) {
        _curX = _posX.payload.intValue;
        _curY = _posY.payload.intValue;
        ImVec2 pos = {float(_curX), float(_curY)};
        ::ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
      }
      flags |= ImGuiWindowFlags_NoMove;
    }

    if (_width.valueType == Int && _height.valueType == Int) {
      if ((_curW != _width.payload.intValue) ||
          (_curH != _height.payload.intValue)) {
        _curW = _width.payload.intValue;
        _curH = _height.payload.intValue;
        ImVec2 size = {float(_curW), float(_curH)};
        ::ImGui::SetNextWindowSize(size, ImGuiCond_Always);
      }
      flags |= ImGuiWindowFlags_NoResize;
    }

    ::ImGui::Begin(_title.c_str(), nullptr, flags);

    CBVar output = Empty;
    if (unlikely(!activateBlocks(CBVar(_blks).payload.seqValue, context, input,
                                 output))) {
      ::ImGui::End();
      return StopChain;
    }
    ::ImGui::End();
    return input;
  }
};

template <CBType CT> struct Variable : public Base {
  static inline Type varType{{CT}};

  std::string _label;
  std::string _variable_name;
  CBVar *_variable = nullptr;
  ExposedInfo _expInfo{};
  bool _exposing = false;

  virtual void cleanup() { _variable = nullptr; }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_variable_name.size() > 0) {
      IterableExposedInfo vars(data.consumables);
      _exposing = true; // assume we expose a new variable
      // search for a possible existing variable and ensure it's the right type
      for (auto &var : vars) {
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

  CBExposedTypesInfo consumedVariables() {
    if (_variable_name.size() > 0 && !_exposing) {
      _expInfo = ExposedInfo(
          consumedInfo, ExposedInfo::Variable(_variable_name.c_str(),
                                              "The consumed input variable.",
                                              CBTypeInfo(varType)));
      return CBExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  CBExposedTypesInfo exposedVariables() {
    if (_variable_name.size() > 0 && _exposing) {
      _expInfo = ExposedInfo(
          consumedInfo, ExposedInfo::Variable(_variable_name.c_str(),
                                              "The exposed input variable.",
                                              CBTypeInfo(varType)));
      return CBExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Label", "The label for this widget.",
                        CoreInfo::StringOrNone),
      ParamsInfo::Param("Variable",
                        "The name of the variable that holds the input value.",
                        CoreInfo::StringOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _variable_name = value.payload.stringValue;
      cleanup();
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _label.size() == 0 ? Empty : Var(_label);
    case 1:
      return _variable_name.size() == 0 ? Empty : Var(_variable_name);
    default:
      return Empty;
    }
  }
};

struct CheckBox : public Variable<CBType::Bool> {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_variable && _variable_name.size() > 0) {
      _variable = findVariable(context, _variable_name.c_str());
    }

    if (_variable) {
      ::ImGui::Checkbox(_label.c_str(), &_variable->payload.boolValue);
      return _variable->payload.boolValue ? True : False;
    } else {
      // HACK kinda... we recycle _exposing since we are not using it in this
      // branch
      ::ImGui::Checkbox(_label.c_str(), &_exposing);
      return _exposing ? True : False;
    }
  }
};

struct Text : public Base {
  // TODO add color

  std::string _label;
  CBVar _color{};

  static inline ParamsInfo paramsInfo =
      ParamsInfo(ParamsInfo::Param("Label", "An optional label for the value.",
                                   CoreInfo::StringOrNone),
                 ParamsInfo::Param("Color", "The optional color of the text.",
                                   CoreInfo::ColorOrNone));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _color = value;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _label.size() == 0 ? Empty : Var(_label);
    case 1:
      return _color;
    default:
      return Empty;
    }
  }

  VarStringStream _text;
  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    _text.write(input);

    if (_color.valueType == Color)
      ::ImGui::PushStyleColor(ImGuiCol_Text, Style::color2Vec4(_color));

    if (_label.size() > 0) {
      ::ImGui::LabelText(_label.c_str(), _text.str());
    } else {
      ::ImGui::Text(_text.str());
    }

    if (_color.valueType == Color)
      ::ImGui::PopStyleColor();

    return input;
  }
};

struct Button : public Base {
  CBValidationResult _validation{};
  static inline Type buttonTypeInfo{
      {CBType::Enum, {.enumeration = {.vendorId = 'frag', .typeId = 'ImGB'}}}};

  enum ButtonTypes {
    Normal,
    Small,
    Invisible,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown
  };

  typedef EnumInfo<ButtonTypes> ButtonEnumInfo;
  static inline ButtonEnumInfo buttonEnumInfo{"ImGuiButton", 'frag', 'ImGB'};

  BlocksVar _blks{};
  ButtonTypes _type{};
  std::string _label;
  ImVec2 _size = {0, 0};

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::BoolType; }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Label", "The text label of this button.",
                        CoreInfo::StringType),
      ParamsInfo::Param("Action",
                        "The blocks to execute when the button is pressed.",
                        CoreInfo::BlocksOrNone),
      ParamsInfo::Param("Type", "The button type.", buttonTypeInfo),
      ParamsInfo::Param("Size", "The optional size override.",
                        CoreInfo::Float2Type));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _blks = value;
      break;
    case 2:
      _type = ButtonTypes(value.payload.enumValue);
      break;
    case 3:
      _size.x = value.payload.float2Value[0];
      _size.y = value.payload.float2Value[1];
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
      return _blks;
    case 2:
      return Var::Enum(_type, 'frag', 'ImGB');
    case 3:
      return Var(_size.x, _size.x);
    default:
      return Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blks.validate(data);
    return data.inputType;
  }

  CBExposedTypesInfo exposedVariables() { return _validation.exposedInfo; }

  void cleanup() { _blks.reset(); }

#define IMBTN_RUN_ACTION                                                       \
  {                                                                            \
    CBVar output = Empty;                                                      \
    if (unlikely(!activateBlocks(CBVar(_blks).payload.seqValue, context,       \
                                 input, output))) {                            \
      return StopChain;                                                        \
    }                                                                          \
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    auto result = False;
    ImVec2 size;
    switch (_type) {
    case Normal:
      if (::ImGui::Button(_label.c_str(), _size)) {
        IMBTN_RUN_ACTION;
        result = True;
      }
      break;
    case Small:
      if (::ImGui::SmallButton(_label.c_str())) {
        IMBTN_RUN_ACTION;
        result = True;
      }
      break;
    case Invisible:
      if (::ImGui::InvisibleButton(_label.c_str(), _size)) {
        IMBTN_RUN_ACTION;
        result = True;
      }
      break;
    case ArrowLeft:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Left)) {
        IMBTN_RUN_ACTION;
        result = True;
      }
      break;
    case ArrowRight:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Right)) {
        IMBTN_RUN_ACTION;
        result = True;
      }
      break;
    case ArrowUp:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Up)) {
        IMBTN_RUN_ACTION;
        result = True;
      }
      break;
    case ArrowDown:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Down)) {
        IMBTN_RUN_ACTION;
        result = True;
      }
      break;
    }
    return result;
  }
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

struct SameLine : public Base {
  // TODO add offsets and spacing
  CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::SameLine();
    return input;
  }
};

typedef BlockWrapper<SameLine> SameLineBlock;

struct Separator : public Base {
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::Separator();
    return input;
  }
};

typedef BlockWrapper<Separator> SeparatorBlock;

struct Indent : public Base {
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::Indent();
    return input;
  }
};

typedef BlockWrapper<Indent> IndentBlock;

struct Unindent : public Base {
  static CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::Unindent();
    return input;
  }
};

typedef BlockWrapper<Unindent> UnindentBlock;

struct TreeNode : public Base {
  std::string _label;
  bool _defaultOpen = true;
  BlocksVar _blocks;

  static inline ParamsInfo params = ParamsInfo(
      ParamsInfo::Param("Label", "The label of this node.",
                        CoreInfo::StringType),
      ParamsInfo::Param("Contents", "The contents of this node.",
                        CoreInfo::BlocksOrNone),
      ParamsInfo::Param("StartOpen",
                        "If this node should start in the open state.",
                        CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  CBTypeInfo compose(const CBInstanceData &data) {
    _blocks.validate(data);
    return data.inputType;
  }

  void cleanup() { _blocks.reset(); }

  void setParam(int index, CBVar value) {
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
      return CBVar();
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (_defaultOpen) {
      ::ImGui::SetNextItemOpen(true);
      _defaultOpen = false;
    }

    auto visible = ::ImGui::TreeNode(_label.c_str());
    if (visible) {
      // run inner blocks
      _blocks.activate(context, input);

      // pop the node if was visible
      ::ImGui::TreePop();
    }

    return Var(visible);
  }
};

typedef BlockWrapper<TreeNode> TreeNodeBlock;

#define IMGUIDRAG(_CBT_, _T_, _INFO_, _IMT_, _VAL_)                            \
  struct _CBT_##Drag : public Variable<CBType::_CBT_> {                        \
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
        _variable = findVariable(context, _variable_name.c_str());             \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_;                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      float speed = 1.0;                                                       \
      if (_variable) {                                                         \
        ::ImGui::DragScalar(_label.c_str(), _IMT_,                             \
                            (void *)&_variable->payload._VAL_, speed);         \
        return *_variable;                                                     \
      } else {                                                                 \
        ::ImGui::DragScalar(_label.c_str(), _IMT_, (void *)&_tmp, speed);      \
        return Var(_tmp);                                                      \
      }                                                                        \
    }                                                                          \
  };                                                                           \
                                                                               \
  typedef BlockWrapper<_CBT_##Drag> _CBT_##DragBlock;

IMGUIDRAG(Int, int64_t, IntType, ImGuiDataType_S64, intValue);
IMGUIDRAG(Float, double, FloatType, ImGuiDataType_Double, floatValue);

#define IMGUIDRAG2(_CBT_, _T_, _INFO_, _IMT_, _VAL_, _CMP_)                    \
  struct _CBT_##Drag : public Variable<CBType::_CBT_> {                        \
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
        _variable = findVariable(context, _variable_name.c_str());             \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_;                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      float speed = 1.0;                                                       \
      if (_variable) {                                                         \
        ::ImGui::DragScalarN(_label.c_str(), _IMT_,                            \
                             (void *)&_variable->payload._VAL_, _CMP_, speed); \
        return *_variable;                                                     \
      } else {                                                                 \
        _tmp.valueType = _CBT_;                                                \
        ::ImGui::DragScalarN(_label.c_str(), _IMT_,                            \
                             (void *)&_tmp.payload._VAL_, _CMP_, speed);       \
        return _tmp;                                                           \
      }                                                                        \
    }                                                                          \
  };                                                                           \
                                                                               \
  typedef BlockWrapper<_CBT_##Drag> _CBT_##DragBlock;

IMGUIDRAG2(Int2, int64_t, Int2Type, ImGuiDataType_S64, int2Value, 2);
IMGUIDRAG2(Int3, int32_t, Int3Type, ImGuiDataType_S32, int3Value, 3);
IMGUIDRAG2(Int4, int32_t, Int4Type, ImGuiDataType_S32, int4Value, 4);
IMGUIDRAG2(Float2, double, Float2Type, ImGuiDataType_Double, float2Value, 2);
IMGUIDRAG2(Float3, float, Float3Type, ImGuiDataType_Float, float3Value, 3);
IMGUIDRAG2(Float4, float, Float4Type, ImGuiDataType_Float, float4Value, 4);

#define IMGUIINPUT(_CBT_, _T_, _INFO_, _IMT_, _VAL_, _FMT_)                    \
  struct _CBT_##Input : public Variable<CBType::_CBT_> {                       \
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
        _variable = findVariable(context, _variable_name.c_str());             \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_;                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      _T_ step = 1;                                                            \
      _T_ step_fast = step * 100;                                              \
      if (_variable) {                                                         \
        ::ImGui::InputScalar(_label.c_str(), _IMT_,                            \
                             (void *)&_variable->payload._VAL_, &step,         \
                             &step_fast, _FMT_, 0);                            \
        return *_variable;                                                     \
      } else {                                                                 \
        ::ImGui::InputScalar(_label.c_str(), _IMT_, (void *)&_tmp, &step,      \
                             &step_fast, _FMT_, 0);                            \
        return Var(_tmp);                                                      \
      }                                                                        \
    }                                                                          \
  };                                                                           \
                                                                               \
  typedef BlockWrapper<_CBT_##Input> _CBT_##InputBlock;

IMGUIINPUT(Int, int64_t, IntType, ImGuiDataType_S64, intValue, "%lld");
IMGUIINPUT(Float, double, FloatType, ImGuiDataType_Double, floatValue, "%f");

#define IMGUIINPUT2(_CBT_, _T_, _INFO_, _IMT_, _VAL_, _FMT_, _CMP_)            \
  struct _CBT_##Input : public Variable<CBType::_CBT_> {                       \
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
        _variable = findVariable(context, _variable_name.c_str());             \
        if (_exposing) {                                                       \
          _variable->valueType = _CBT_;                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      _T_ step = 1;                                                            \
      _T_ step_fast = step * 100;                                              \
      if (_variable) {                                                         \
        ::ImGui::InputScalarN(_label.c_str(), _IMT_,                           \
                              (void *)&_variable->payload._VAL_, _CMP_, &step, \
                              &step_fast, _FMT_, 0);                           \
        return *_variable;                                                     \
      } else {                                                                 \
        _tmp.valueType = _CBT_;                                                \
        ::ImGui::InputScalarN(_label.c_str(), _IMT_,                           \
                              (void *)&_tmp.payload._VAL_, _CMP_, &step,       \
                              &step_fast, _FMT_, 0);                           \
        return _tmp;                                                           \
      }                                                                        \
    }                                                                          \
  };                                                                           \
                                                                               \
  typedef BlockWrapper<_CBT_##Input> _CBT_##InputBlock;

IMGUIINPUT2(Int2, int64_t, Int2Type, ImGuiDataType_S64, int2Value, "%lld", 2);
IMGUIINPUT2(Int3, int32_t, Int3Type, ImGuiDataType_S32, int3Value, "%d", 3);
IMGUIINPUT2(Int4, int32_t, Int4Type, ImGuiDataType_S32, int4Value, "%d", 4);

IMGUIINPUT2(Float2, double, Float2Type, ImGuiDataType_Double, float2Value,
            "%.3f", 2);
IMGUIINPUT2(Float3, float, Float3Type, ImGuiDataType_Float, float3Value, "%.3f",
            3);
IMGUIINPUT2(Float4, float, Float4Type, ImGuiDataType_Float, float4Value, "%.3f",
            4);

struct TextInput : public Variable<CBType::String> {
  // fallback, used only when no variable name is set
  std::string _buffer;

  void cleanup() override {
    if (_variable && _exposing) {
      destroyVar(*_variable);
    }
    Variable<CBType::String>::cleanup();
  }

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static int InputTextCallback(ImGuiInputTextCallbackData *data) {
    TextInput *it = (TextInput *)data->UserData;
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
      // Resize string callback
      if (it->_variable) {
        delete[] it->_variable->payload.stringValue;
        it->_variable->capacity.value = data->BufTextLen * 2;
        it->_variable->payload.stringValue =
            new char[it->_variable->capacity.value];
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
      _variable = findVariable(context, _variable_name.c_str());
      if (_exposing) {
        // we own the variable so let's run some init
        _variable->valueType = String;
        _variable->payload.stringValue = new char[32];
        _variable->capacity.value = 32;
        memset((void *)_variable->payload.stringValue, 0x0, 32);
      }
    }

    if (_variable) {
      ::ImGui::InputText(_label.c_str(), (char *)_variable->payload.stringValue,
                         _variable->capacity.value,
                         ImGuiInputTextFlags_CallbackResize, &InputTextCallback,
                         this);
      return *_variable;
    } else {
      ::ImGui::InputText(_label.c_str(), (char *)_buffer.c_str(),
                         _buffer.capacity() + 1, 0, &InputTextCallback, this);
      return Var(_buffer);
    }
  }
};

typedef BlockWrapper<TextInput> TextInputBlock;

struct Image : public Base {
  ImVec2 _size{1.0, 1.0};
  bool _trueSize = false;

  static CBTypesInfo inputTypes() { return BGFX::Texture::TextureHandleType; }

  static CBTypesInfo outputTypes() { return BGFX::Texture::TextureHandleType; }

  static inline ParamsInfo paramsInfo =
      ParamsInfo(ParamsInfo::Param("Size", "The drawing size of the image.",
                                   CoreInfo::Float2Type),
                 ParamsInfo::Param("TrueSize",
                                   "If the given size is in true image pixels.",
                                   CoreInfo::BoolType));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
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
      return Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);
    auto texture =
        *reinterpret_cast<BGFX::Texture *>(input.payload.objectValue);
    if (!_trueSize) {
      ImVec2 size = _size;
      size.x *= texture.width;
      size.y *= texture.height;
      ::ImGui::Image(texture.handle, size);
    } else {
      ::ImGui::Image(texture.handle, _size);
    }
    return input;
  }
};

typedef BlockWrapper<Image> ImageBlock;

// Register
RUNTIME_BLOCK(ImGui, Style);
RUNTIME_BLOCK_compose(Style);
RUNTIME_BLOCK_consumedVariables(Style);
RUNTIME_BLOCK_parameters(Style);
RUNTIME_BLOCK_setParam(Style);
RUNTIME_BLOCK_getParam(Style);
RUNTIME_BLOCK_inputTypes(Style);
RUNTIME_BLOCK_outputTypes(Style);
RUNTIME_BLOCK_activate(Style);
RUNTIME_BLOCK_END(Style);

RUNTIME_BLOCK(ImGui, Window);
RUNTIME_BLOCK_cleanup(Window);
RUNTIME_BLOCK_compose(Window);
RUNTIME_BLOCK_consumedVariables(Window);
RUNTIME_BLOCK_exposedVariables(Window);
RUNTIME_BLOCK_parameters(Window);
RUNTIME_BLOCK_setParam(Window);
RUNTIME_BLOCK_getParam(Window);
RUNTIME_BLOCK_inputTypes(Window);
RUNTIME_BLOCK_outputTypes(Window);
RUNTIME_BLOCK_activate(Window);
RUNTIME_BLOCK_END(Window);

RUNTIME_BLOCK(ImGui, CheckBox);
RUNTIME_BLOCK_cleanup(CheckBox);
RUNTIME_BLOCK_compose(CheckBox);
RUNTIME_BLOCK_consumedVariables(CheckBox);
RUNTIME_BLOCK_exposedVariables(CheckBox);
RUNTIME_BLOCK_parameters(CheckBox);
RUNTIME_BLOCK_setParam(CheckBox);
RUNTIME_BLOCK_getParam(CheckBox);
RUNTIME_BLOCK_inputTypes(CheckBox);
RUNTIME_BLOCK_outputTypes(CheckBox);
RUNTIME_BLOCK_activate(CheckBox);
RUNTIME_BLOCK_END(CheckBox);

RUNTIME_BLOCK(ImGui, Text);
RUNTIME_BLOCK_consumedVariables(Text);
RUNTIME_BLOCK_parameters(Text);
RUNTIME_BLOCK_setParam(Text);
RUNTIME_BLOCK_getParam(Text);
RUNTIME_BLOCK_inputTypes(Text);
RUNTIME_BLOCK_outputTypes(Text);
RUNTIME_BLOCK_activate(Text);
RUNTIME_BLOCK_END(Text);

RUNTIME_BLOCK(ImGui, Button);
RUNTIME_BLOCK_cleanup(Button);
RUNTIME_BLOCK_compose(Button);
RUNTIME_BLOCK_consumedVariables(Button);
RUNTIME_BLOCK_exposedVariables(Button);
RUNTIME_BLOCK_parameters(Button);
RUNTIME_BLOCK_setParam(Button);
RUNTIME_BLOCK_getParam(Button);
RUNTIME_BLOCK_inputTypes(Button);
RUNTIME_BLOCK_outputTypes(Button);
RUNTIME_BLOCK_activate(Button);
RUNTIME_BLOCK_END(Button);

RUNTIME_BLOCK(ImGui, HexViewer);
RUNTIME_BLOCK_consumedVariables(HexViewer);
RUNTIME_BLOCK_inputTypes(HexViewer);
RUNTIME_BLOCK_outputTypes(HexViewer);
RUNTIME_BLOCK_activate(HexViewer);
RUNTIME_BLOCK_END(HexViewer);

void registerImGuiBlocks() {
  REGISTER_BLOCK(ImGui, Style);
  REGISTER_BLOCK(ImGui, Window);
  REGISTER_BLOCK(ImGui, CheckBox);
  REGISTER_BLOCK(ImGui, Text);
  REGISTER_BLOCK(ImGui, Button);
  REGISTER_BLOCK(ImGui, HexViewer);
  registerBlock("ImGui.SameLine", &SameLineBlock::create);
  registerBlock("ImGui.Separator", &SeparatorBlock::create);
  registerBlock("ImGui.Indent", &IndentBlock::create);
  registerBlock("ImGui.Unindent", &UnindentBlock::create);
  registerBlock("ImGui.TreeNode", &TreeNodeBlock::create);
  registerBlock("ImGui.IntInput", &IntInputBlock::create);
  registerBlock("ImGui.FloatInput", &FloatInputBlock::create);
  registerBlock("ImGui.Int2Input", &Int2InputBlock::create);
  registerBlock("ImGui.Int3Input", &Int3InputBlock::create);
  registerBlock("ImGui.Int4Input", &Int4InputBlock::create);
  registerBlock("ImGui.Float2Input", &Float2InputBlock::create);
  registerBlock("ImGui.Float3Input", &Float3InputBlock::create);
  registerBlock("ImGui.Float4Input", &Float4InputBlock::create);
  registerBlock("ImGui.IntDrag", &IntDragBlock::create);
  registerBlock("ImGui.FloatDrag", &FloatDragBlock::create);
  registerBlock("ImGui.Int2Drag", &Int2DragBlock::create);
  registerBlock("ImGui.Int3Drag", &Int3DragBlock::create);
  registerBlock("ImGui.Int4Drag", &Int4DragBlock::create);
  registerBlock("ImGui.Float2Drag", &Float2DragBlock::create);
  registerBlock("ImGui.Float3Drag", &Float3DragBlock::create);
  registerBlock("ImGui.Float4Drag", &Float4DragBlock::create);
  registerBlock("ImGui.TextInput", &TextInputBlock::create);
  registerBlock("ImGui.Image", &ImageBlock::create);
}
}; // namespace ImGui
}; // namespace chainblocks
