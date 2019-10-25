/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "imgui.hpp"
#include "blocks/shared.hpp"
#include "runtime.hpp"

using namespace chainblocks;

namespace chainblocks {
namespace ImGui {
struct Base {
  CBVar *_context;

  static inline ExposedInfo consumedInfo = ExposedInfo(ExposedInfo::Variable(
      "ImGui.Context", "The ImGui Context.", CBTypeInfo(Context::Info)));

  CBExposedTypesInfo consumedVariables() {
    return CBExposedTypesInfo(consumedInfo);
  }

  static CBTypesInfo inputTypes() {
    return CBTypesInfo((SharedTypes::anyInfo));
  }

  static CBTypesInfo outputTypes() {
    return CBTypesInfo((SharedTypes::anyInfo));
  }
};

struct IDContext {
  IDContext(void *ptr) { ::ImGui::PushID(ptr); }
  ~IDContext() { ::ImGui::PopID(); }
};

struct Style : public Base {
  static inline CBEnumInfo StyleKeysEnum = {"ImGuiStyle"};
  static inline TypeInfo styleEnumInfo = TypeInfo::Enum('frag', 'ImGS');
  static inline TypesInfo styleEnumInfos = TypesInfo(styleEnumInfo);
  enum StyleKeys {
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

  static void InitEnums() {
    // These leak for now
    stbds_arrpush(StyleKeysEnum.labels, "Alpha");
    stbds_arrpush(StyleKeysEnum.labels, "WindowPadding");
    stbds_arrpush(StyleKeysEnum.labels, "WindowRounding");
    stbds_arrpush(StyleKeysEnum.labels, "WindowBorderSize");
    stbds_arrpush(StyleKeysEnum.labels, "WindowMinSize");
    stbds_arrpush(StyleKeysEnum.labels, "WindowTitleAlign");
    stbds_arrpush(StyleKeysEnum.labels, "WindowMenuButtonPosition");
    stbds_arrpush(StyleKeysEnum.labels, "ChildRounding");
    stbds_arrpush(StyleKeysEnum.labels, "ChildBorderSize");
    stbds_arrpush(StyleKeysEnum.labels, "PopupRounding");
    stbds_arrpush(StyleKeysEnum.labels, "PopupBorderSize");
    stbds_arrpush(StyleKeysEnum.labels, "FramePadding");
    stbds_arrpush(StyleKeysEnum.labels, "FrameRounding");
    stbds_arrpush(StyleKeysEnum.labels, "FrameBorderSize");
    stbds_arrpush(StyleKeysEnum.labels, "ItemSpacing");
    stbds_arrpush(StyleKeysEnum.labels, "ItemInnerSpacing");
    stbds_arrpush(StyleKeysEnum.labels, "TouchExtraPadding");
    stbds_arrpush(StyleKeysEnum.labels, "IndentSpacing");
    stbds_arrpush(StyleKeysEnum.labels, "ColumnsMinSpacing");
    stbds_arrpush(StyleKeysEnum.labels, "ScrollbarSize");
    stbds_arrpush(StyleKeysEnum.labels, "ScrollbarRounding");
    stbds_arrpush(StyleKeysEnum.labels, "GrabMinSize");
    stbds_arrpush(StyleKeysEnum.labels, "GrabRounding");
    stbds_arrpush(StyleKeysEnum.labels, "TabRounding");
    stbds_arrpush(StyleKeysEnum.labels, "TabBorderSize");
    stbds_arrpush(StyleKeysEnum.labels, "ColorButtonPosition");
    stbds_arrpush(StyleKeysEnum.labels, "ButtonTextAlign");
    stbds_arrpush(StyleKeysEnum.labels, "SelectableTextAlign");
    stbds_arrpush(StyleKeysEnum.labels, "DisplayWindowPadding");
    stbds_arrpush(StyleKeysEnum.labels, "DisplaySafeAreaPadding");
    stbds_arrpush(StyleKeysEnum.labels, "MouseCursorScale");
    stbds_arrpush(StyleKeysEnum.labels, "AntiAliasedLines");
    stbds_arrpush(StyleKeysEnum.labels, "AntiAliasedFill");
    stbds_arrpush(StyleKeysEnum.labels, "CurveTessellationTol");
    stbds_arrpush(StyleKeysEnum.labels, "TextColor");
    stbds_arrpush(StyleKeysEnum.labels, "TextDisabledColor");
    stbds_arrpush(StyleKeysEnum.labels, "WindowBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "ChildBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "PopupBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "BorderColor");
    stbds_arrpush(StyleKeysEnum.labels, "BorderShadowColor");
    stbds_arrpush(StyleKeysEnum.labels, "FrameBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "FrameBgHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "FrameBgActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "TitleBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "TitleBgActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "TitleBgCollapsedColor");
    stbds_arrpush(StyleKeysEnum.labels, "MenuBarBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "ScrollbarBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "ScrollbarGrabColor");
    stbds_arrpush(StyleKeysEnum.labels, "ScrollbarGrabHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "ScrollbarGrabActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "CheckMarkColor");
    stbds_arrpush(StyleKeysEnum.labels, "SliderGrabColor");
    stbds_arrpush(StyleKeysEnum.labels, "SliderGrabActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "ButtonColor");
    stbds_arrpush(StyleKeysEnum.labels, "ButtonHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "ButtonActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "HeaderColor");
    stbds_arrpush(StyleKeysEnum.labels, "HeaderHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "HeaderActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "SeparatorColor");
    stbds_arrpush(StyleKeysEnum.labels, "SeparatorHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "SeparatorActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "ResizeGripColor");
    stbds_arrpush(StyleKeysEnum.labels, "ResizeGripHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "ResizeGripActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "TabColor");
    stbds_arrpush(StyleKeysEnum.labels, "TabHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "TabActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "TabUnfocusedColor");
    stbds_arrpush(StyleKeysEnum.labels, "TabUnfocusedActiveColor");
    stbds_arrpush(StyleKeysEnum.labels, "PlotLinesColor");
    stbds_arrpush(StyleKeysEnum.labels, "PlotLinesHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "PlotHistogramColor");
    stbds_arrpush(StyleKeysEnum.labels, "PlotHistogramHoveredColor");
    stbds_arrpush(StyleKeysEnum.labels, "TextSelectedBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "DragDropTargetColor");
    stbds_arrpush(StyleKeysEnum.labels, "NavHighlightColor");
    stbds_arrpush(StyleKeysEnum.labels, "NavWindowingHighlightColor");
    stbds_arrpush(StyleKeysEnum.labels, "NavWindowingDimBgColor");
    stbds_arrpush(StyleKeysEnum.labels, "ModalWindowDimBgColor");
    registerEnumType('frag', 'ImGS', StyleKeysEnum);
  }

  StyleKeys _key{};

  static inline ParamsInfo paramsInfo = ParamsInfo(ParamsInfo::Param(
      "Style", "A style key to set.", CBTypesInfo(styleEnumInfos)));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _key = StyleKeys(value.payload.enumValue);
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

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
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
      if (inputType.basicType != Float) {
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
      if (inputType.basicType != Float2) {
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
      if (inputType.basicType != Color) {
        throw CBException(
            "this ImGui Style block expected a Color variable as input!");
      }
      break;
    }
    return inputType;
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

struct Window : public Base, public chainblocks::BlocksUser {
  std::string _title;
  int _curX = -1, _curY = -1, _curW = -1, _curH = -1;
  CBVar _posX{}, _posY{}, _width{}, _height{};

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Title", "The title of the window to create.",
                        CBTypesInfo(SharedTypes::strInfo)),
      ParamsInfo::Param("PosX",
                        "The horizontal position of the window to create",
                        CBTypesInfo(SharedTypes::intOrNoneInfo)),
      ParamsInfo::Param("PosY",
                        "The vertical position of the window to create.",
                        CBTypesInfo(SharedTypes::intOrNoneInfo)),
      ParamsInfo::Param("Width", "The width of the window to create",
                        CBTypesInfo(SharedTypes::intOrNoneInfo)),
      ParamsInfo::Param("Height", "The height of the window to create.",
                        CBTypesInfo(SharedTypes::intOrNoneInfo)),
      ParamsInfo::Param("Contents", "The inner contents blocks.",
                        CBTypesInfo(SharedTypes::blocksOrNoneInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

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
      cloneVar(_blocks, value);
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
      return _blocks;
    default:
      return Empty;
    }
  }

  void destroy() { chainblocks::BlocksUser::destroy(); }

  void cleanup() {
    chainblocks::BlocksUser::cleanup();

    _curX = -1;
    _curY = -1;
    _context = nullptr;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (_blocks.valueType != Seq)
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
    if (unlikely(!activateBlocks(_blocks.payload.seqValue, context, input,
                                 output))) {
      ::ImGui::End();
      return StopChain;
    }
    ::ImGui::End();
    return input;
  }
};

struct CheckBox : public Base {
  std::string _label;
  std::string _variable;
  CBVar *_downVar = nullptr;
  ExposedInfo _consumedInfo{};
  bool _exposing = false;

  void cleanup() { _downVar = nullptr; }

  CBTypeInfo inferTypes(CBTypeInfo inputType, CBExposedTypesInfo consumables) {
    if (_variable.size() > 0) {
      IterableExposedInfo vars(consumables);
      _exposing = true; // assume we expose a new bool variable
      // search for a possible existing variable and make sure it's bool
      for (auto &var : vars) {
        if (strcmp(var.name, _variable.c_str()) == 0) {
          // we found a variable, make sure it's bool and mark exposing off
          _exposing = false;
          if (var.exposedType.basicType != Bool) {
            throw CBException("CheckBox: Expected a boolean variable.");
          }
          break;
        }
      }
    }
    return CBTypeInfo(SharedTypes::boolInfo);
  }

  CBExposedTypesInfo consumedVariables() {
    if (_variable.size() > 0 && !_exposing) {
      _consumedInfo =
          ExposedInfo(consumedInfo,
                      ExposedInfo::Variable(_variable.c_str(),
                                            "The consumed boolean variable.",
                                            CBTypeInfo(SharedTypes::boolInfo)));
      return CBExposedTypesInfo(consumedInfo);
    } else {
      return nullptr;
    }
  }

  CBExposedTypesInfo exposedVariables() {
    if (_variable.size() > 0 && _exposing) {
      _consumedInfo =
          ExposedInfo(consumedInfo,
                      ExposedInfo::Variable(_variable.c_str(),
                                            "The exposed boolean variable.",
                                            CBTypeInfo(SharedTypes::boolInfo)));
      return CBExposedTypesInfo(consumedInfo);
    } else {
      return nullptr;
    }
  }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Label", "The label for this widget.",
                        CBTypesInfo(SharedTypes::strOrNoneInfo)),
      ParamsInfo::Param(
          "Variable", "The name of the variable that holds the boolean value.",
          CBTypesInfo(SharedTypes::strOrNoneInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  static CBTypesInfo inputTypes() {
    return CBTypesInfo((SharedTypes::noneInfo));
  }

  static CBTypesInfo outputTypes() {
    return CBTypesInfo((SharedTypes::boolInfo));
  }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      _variable = value.payload.stringValue;
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
      return _variable.size() == 0 ? Empty : Var(_variable);
    default:
      return Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    if (!_downVar && _variable.size() > 0) {
      _downVar = contextVariable(context, _variable.c_str());
    }

    if (_downVar) {
      ::ImGui::Checkbox(_label.c_str(), &_downVar->payload.boolValue);
      return _downVar->payload.boolValue ? True : False;
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
                                   CBTypesInfo(SharedTypes::strOrNoneInfo)),
                 ParamsInfo::Param("Color", "The optional color of the text.",
                                   CBTypesInfo(SharedTypes::colorOrNoneInfo)));

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

struct SameLine : public Base {
  // TODO add offsets and spacing
  CBVar activate(CBContext *context, const CBVar &input) {
    ::ImGui::SameLine();
    return input;
  }
};

struct Button : public Base, public BlocksUser {
  static inline CBEnumInfo ImguiButtonsEnumInfo = {"ImGuiButton"};
  static inline TypeInfo buttonTypeInfo = TypeInfo::Enum('frag', 'ImGB');
  static inline TypesInfo buttonTypesInfo = TypesInfo(buttonTypeInfo);
  static void InitEnums() {
    // These leak for now
    stbds_arrpush(Button::ImguiButtonsEnumInfo.labels, "Normal");
    stbds_arrpush(Button::ImguiButtonsEnumInfo.labels, "Small");
    stbds_arrpush(Button::ImguiButtonsEnumInfo.labels, "Invisible");
    stbds_arrpush(Button::ImguiButtonsEnumInfo.labels, "ArrowLeft");
    stbds_arrpush(Button::ImguiButtonsEnumInfo.labels, "ArrowRight");
    stbds_arrpush(Button::ImguiButtonsEnumInfo.labels, "ArrowUp");
    stbds_arrpush(Button::ImguiButtonsEnumInfo.labels, "ArrowDown");
    registerEnumType('frag', 'ImGB', Button::ImguiButtonsEnumInfo);
  }

  enum ButtonTypes {
    Normal,
    Small,
    Invisible,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown
  };

  ButtonTypes _type{};
  std::string _label;
  ImVec2 _size = {0, 0};

  static CBTypesInfo inputTypes() {
    return CBTypesInfo((SharedTypes::noneInfo));
  }

  static CBTypesInfo outputTypes() {
    return CBTypesInfo((SharedTypes::boolInfo));
  }

  static inline ParamsInfo paramsInfo = ParamsInfo(
      ParamsInfo::Param("Label", "The text label of this button.",
                        CBTypesInfo(SharedTypes::strInfo)),
      ParamsInfo::Param("Action",
                        "The blocks to execute when the button is pressed.",
                        CBTypesInfo(SharedTypes::blocksOrNoneInfo)),
      ParamsInfo::Param("Type", "The button type.",
                        CBTypesInfo(buttonTypesInfo)),
      ParamsInfo::Param("Size", "The optional size override.",
                        CBTypesInfo(SharedTypes::float2Info)));

  static CBParametersInfo parameters() { return CBParametersInfo(paramsInfo); }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _label = value.payload.stringValue;
      break;
    case 1:
      cloneVar(_blocks, value);
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
      return _blocks;
    case 2:
      return Var::Enum(_type, 'frag', 'ImGB');
    case 3:
      return Var(_size.x, _size.x);
    default:
      return Empty;
    }
  }

#define IMBTN_RUN_ACTION                                                       \
  {                                                                            \
    CBVar output = Empty;                                                      \
    if (unlikely(!activateBlocks(_blocks.payload.seqValue, context, input,     \
                                 output))) {                                   \
      return StopChain;                                                        \
    }                                                                          \
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);

    auto result = chainblocks::False;
    ImVec2 size;
    switch (_type) {
    case Normal:
      if (::ImGui::Button(_label.c_str(), _size)) {
        IMBTN_RUN_ACTION;
        result = chainblocks::True;
      }
      break;
    case Small:
      if (::ImGui::SmallButton(_label.c_str())) {
        IMBTN_RUN_ACTION;
        result = chainblocks::True;
      }
      break;
    case Invisible:
      if (::ImGui::InvisibleButton(_label.c_str(), _size)) {
        IMBTN_RUN_ACTION;
        result = chainblocks::True;
      }
      break;
    case ArrowLeft:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Left)) {
        IMBTN_RUN_ACTION;
        result = chainblocks::True;
      }
      break;
    case ArrowRight:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Right)) {
        IMBTN_RUN_ACTION;
        result = chainblocks::True;
      }
      break;
    case ArrowUp:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Up)) {
        IMBTN_RUN_ACTION;
        result = chainblocks::True;
      }
      break;
    case ArrowDown:
      if (::ImGui::ArrowButton(_label.c_str(), ImGuiDir_Down)) {
        IMBTN_RUN_ACTION;
        result = chainblocks::True;
      }
      break;
    }
    return result;
  }
};

struct HexViewer : public Base {
  static CBTypesInfo inputTypes() {
    return CBTypesInfo((SharedTypes::bytesInfo));
  }

  static CBTypesInfo outputTypes() {
    return CBTypesInfo((SharedTypes::bytesInfo));
  }

  ImGuiExtra::MemoryEditor _editor{};

  HexViewer() { _editor.ReadOnly = true; }

  CBVar activate(CBContext *context, const CBVar &input) {
    IDContext idCtx(this);
    _editor.DrawContents(input.payload.bytesValue, input.payload.bytesSize);
    return input;
  }
};

// Register
RUNTIME_BLOCK(ImGui, Style);
RUNTIME_BLOCK_inferTypes(Style);
RUNTIME_BLOCK_consumedVariables(Style);
RUNTIME_BLOCK_parameters(Style);
RUNTIME_BLOCK_setParam(Style);
RUNTIME_BLOCK_getParam(Style);
RUNTIME_BLOCK_inputTypes(Style);
RUNTIME_BLOCK_outputTypes(Style);
RUNTIME_BLOCK_activate(Style);
RUNTIME_BLOCK_END(Style);

RUNTIME_BLOCK(ImGui, Window);
RUNTIME_BLOCK_destroy(Window);
RUNTIME_BLOCK_cleanup(Window);
RUNTIME_BLOCK_inferTypes(Window);
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
RUNTIME_BLOCK_inferTypes(CheckBox);
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

RUNTIME_BLOCK(ImGui, SameLine);
RUNTIME_BLOCK_consumedVariables(SameLine);
RUNTIME_BLOCK_inputTypes(SameLine);
RUNTIME_BLOCK_outputTypes(SameLine);
RUNTIME_BLOCK_activate(SameLine);
RUNTIME_BLOCK_END(SameLine);

RUNTIME_BLOCK(ImGui, Button);
RUNTIME_BLOCK_destroy(Button);
RUNTIME_BLOCK_cleanup(Button);
RUNTIME_BLOCK_inferTypes(Button);
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
  REGISTER_BLOCK(ImGui, SameLine);
  REGISTER_BLOCK(ImGui, Button);
  REGISTER_BLOCK(ImGui, HexViewer);
  Button::InitEnums();
  Style::InitEnums();
}
}; // namespace ImGui
}; // namespace chainblocks
