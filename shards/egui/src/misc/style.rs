/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::util::{
  apply_widget_visuals, into_color, into_margin, into_rounding, into_shadow, into_stroke, into_vec2,
};
use crate::widgets::{TextWrap, TEXTWRAP_TYPE};
use crate::{CONTEXTS_NAME, PARENTS_UI_NAME};
use shards::core::{register_enum, register_shard};
use shards::shard::Shard;
use shards::types::{common_type, ClonedVar, TableVar};
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var, NONE_TYPES};
use shards::util::get_or_var;

#[derive(shards::shards_enum)]
#[enum_info(b"egFf", "FontFamily", "Which style of font.")]
pub enum FontFamily {
  Proportional = 0,
  Monospace = 1,
}

#[derive(shards::shards_enum)]
#[enum_info(b"egTs", "TextStyle", "Text syle identifier")]
pub enum TextStyle {
  #[enum_value("Used when small text is needed.")]
  Small,

  #[enum_value("Normal labels. Easily readable, doesn't take up too much space.")]
  Body,

  #[enum_value("Same size as [`Self::Body`], but used when monospace is important (for code snippets, aligning numbers, etc).")]
  Monospace,

  #[enum_value("Buttons. Maybe slightly bigger than [`Self::Body`].")]
  Button,

  #[enum_value("Heading. Probably larger than [`Self::Body`].")]
  Heading,
}

impl Into<egui::TextStyle> for TextStyle {
  fn into(self) -> egui::TextStyle {
    match self {
      Self::Small => egui::TextStyle::Small,
      Self::Body => egui::TextStyle::Body,
      Self::Monospace => egui::TextStyle::Monospace,
      Self::Button => egui::TextStyle::Button,
      Self::Heading => egui::TextStyle::Heading,
    }
  }
}

impl Into<egui::FontFamily> for FontFamily {
  fn into(self) -> egui::FontFamily {
    match self {
      Self::Proportional => egui::FontFamily::Proportional,
      Self::Monospace => egui::FontFamily::Monospace,
    }
  }
}

pub fn into_text_style(v: &Var) -> Result<egui::TextStyle, &'static str> {
  Ok(if let Ok(ts) = v.try_into() as Result<TextStyle, _> {
    ts.into()
  } else {
    let style_name: &str = v.try_into()?;
    egui::TextStyle::Name(style_name.into())
  })
}

pub fn into_font_family(v: &Var) -> Result<egui::FontFamily, &'static str> {
  Ok(if let Ok(ff) = v.try_into() as Result<FontFamily, _> {
    ff.into()
  } else {
    let family_name: &str = v.try_into()?;
    egui::FontFamily::Name(family_name.into())
  })
}

pub fn into_font_id(v: &Var, ctx: &Context) -> Result<egui::FontId, &'static str> {
  let tbl: TableVar = v.try_into()?;
  let size: f32 = get_or_var(tbl.get_static("Size").ok_or("Missing Size")?, ctx).try_into()?;
  let family = into_font_family(get_or_var(
    tbl.get_static("Family").ok_or("Missing Family")?,
    ctx,
  ))?;
  Ok(egui::FontId { size, family })
}

pub fn when_set<F: FnOnce(&Var) -> Result<(), &'static str>>(
  pv: &ParamVar,
  f: F,
) -> Result<(), &'static str> {
  let v = pv.get();
  if !v.is_none() {
    f(v)
  } else {
    Ok(())
  }
}

lazy_static! {
  static ref TEXT_STYLE_TYPES: Types = vec![common_type::none, *TEXTSTYLE_TYPE, Type::context_variable(&TEXTSTYLE_TYPES[..]), common_type::string, common_type::string_var];
  static ref FONT_FAMILY_TYPES: Types = vec![common_type::none, *FONTFAMILY_TYPE, Type::context_variable(&FONTFAMILY_TYPES[..]), common_type::string, common_type::string_var];

  // { Size: Type::Float FontFamily: FontFamily }
  static ref FONT_ID_TYPES: Types = vec![common_type::none, common_type::any_table, common_type::any_table_var];
}

#[derive(shards::shard)]
#[shard_info("UI.Style", "Apply style changes to the current UI scope.")]
struct StyleShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_warmup]
  parents: ParamVar,

  #[shard_warmup]
  contexts: ParamVar,

  #[shard_param(
    "InheritDefault",
    "Inherit default style instead of current style.",
    [common_type::bool]
  )]
  inherit_default: ClonedVar,

  #[shard_param(
    "OverrideTextStyle",
    "If set this will change the default TextStyle for all widgets.",
    TEXT_STYLE_TYPES
  )]
  override_text_style: ParamVar,

  #[shard_param(
    "FontId",
    "If set this will change the font family and size for all widgets.",
    FONT_ID_TYPES
  )]
  font_id: ParamVar,

  #[shard_param(
    "TextStyles",
    "The FontFamily and size you want to use for a specific TextStyle.",
    [common_type::none, common_type::any_table, common_type::any_table_var]
  )]
  text_styles: ParamVar,

  #[shard_param(
    "DragValueTextStyle",
    "The style to use for DragValue text.",
    TEXT_STYLE_TYPES
  )]
  drag_value_text_style: ParamVar,

  #[shard_param("Wrap", "If set, labels, buttons, etc will use this to determine wrap behavior of the text at the right edge of the Ui they are in.", [common_type::none, common_type::bool, *TEXTWRAP_TYPE])]
  wrap: ParamVar,

  #[shard_param("AnimationTime", "How many seconds a typical animation should last.", [common_type::none, common_type::float, common_type::float_var])]
  animation_time: ParamVar,

  #[shard_param("ExplanationTooltips", "Show tooltips explaining DragValues etc when hovered.", [common_type::none, common_type::bool])]
  explanation_tooltips: ParamVar,

  #[shard_param("ResizeGrabRadiusSide", "Mouse must be this close to the side of a window to resize", [common_type::none, common_type::float, common_type::float_var])]
  resize_grab_radius_side: ParamVar,

  #[shard_param("ResizeGrabRadiusCorner", "Mouse must be this close to the corner of a window to resize", [common_type::none, common_type::float, common_type::float_var])]
  resize_grab_radius_corner: ParamVar,

  #[shard_param("ShowTooltipsOnlyWhenStill", "If `false`, tooltips will show up anytime you hover anything, even is mouse is still moving", [common_type::none, common_type::bool])]
  show_tooltips_only_when_still: ParamVar,

  // ------ Spacing members ------
  #[shard_param("ItemSpacing", "Horizontal and vertical spacing between widgets.", [common_type::none, common_type::float2, common_type::float2_var])]
  item_spacing: ParamVar,

  #[shard_param("WindowMargin", "Horizontal and vertical margins within a window frame.", [common_type::none, common_type::float4, common_type::float4_var])]
  window_margin: ParamVar,

  #[shard_param("ButtonPadding", "Button size is text size plus this on each side", [common_type::none, common_type::float2, common_type::float2_var])]
  button_padding: ParamVar,

  #[shard_param("MenuMargin", "Horizontal and vertical margins within a menu frame.", [common_type::none, common_type::float4, common_type::float4_var])]
  menu_margin: ParamVar,

  #[shard_param("Indent", "Indent collapsing regions etc by this much.", [common_type::none, common_type::float, common_type::float_var])]
  indent: ParamVar,

  #[shard_param("InteractSize", "Minimum size of a DragValue, color picker button, and other small widgets. interact_size.y is the default height of button, slider, etc. Anything clickable should be (at least) this size.", [common_type::none, common_type::float2, common_type::float2_var])]
  interact_size: ParamVar,

  #[shard_param("SliderWidth", "Default width of a Slider.", [common_type::none, common_type::float, common_type::float_var])]
  slider_width: ParamVar,

  #[shard_param("ComboWidth", "Default (minimum) width of a ComboBox.", [common_type::none, common_type::float, common_type::float_var])]
  combo_width: ParamVar,

  #[shard_param("TextEditWidth", "Default width of a TextEdit.", [common_type::none, common_type::float, common_type::float_var])]
  text_edit_width: ParamVar,

  #[shard_param("IconWidth", "Checkboxes, radio button and collapsing headers have an icon at the start. This is the width/height of the outer part of this icon (e.g. the BOX of the checkbox).", [common_type::none, common_type::float, common_type::float_var])]
  icon_width: ParamVar,

  #[shard_param("IconWidthInner", "Checkboxes, radio button and collapsing headers have an icon at the start. This is the width/height of the inner part of this icon (e.g. the check of the checkbox).", [common_type::none, common_type::float, common_type::float_var])]
  icon_width_inner: ParamVar,

  #[shard_param("IconSpacing", "Checkboxes, radio button and collapsing headers have an icon at the start. This is the spacing between the icon and the text", [common_type::none, common_type::float, common_type::float_var])]
  icon_spacing: ParamVar,

  #[shard_param("DefaultAreaSize", "The size used for the [`Ui::max_rect`] the first frame. Text will wrap at this width, and images that expand to fill the available space will expand to this size.", [common_type::none, common_type::float2, common_type::float2_var])]
  default_area_size: ParamVar,

  #[shard_param("TooltipWidth", "Width of a tooltip (on_hover_ui, on_hover_text etc).", [common_type::none, common_type::float, common_type::float_var])]
  tooltip_width: ParamVar,

  #[shard_param("IndentEndsWithHorizontalLine", "End indented regions with a horizontal line", [common_type::none, common_type::bool])]
  indent_ends_with_horizontal_line: ParamVar,

  #[shard_param("ComboHeight", "Height of a combo-box before showing scroll bars.", [common_type::none, common_type::float, common_type::float_var])]
  combo_height: ParamVar,

  #[shard_param("ScrollBarFloating", "Use floating scroll bar.", [common_type::none, common_type::bool, common_type::bool_var])]
  scroll_bar_floating: ParamVar,

  #[shard_param("ScrollBarWidth", "Width of a scroll bar.", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_width: ParamVar,

  #[shard_param("ScrollBarFloatingWidth", "Width of a floating scroll bar (not hovering).", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_floating_width: ParamVar,

  #[shard_param("ScrollBarFloatingAllocatedWidth", "Allocated width of a floating scroll bar (not hovering).", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_floating_allocated_width: ParamVar,

  #[shard_param("ScrollHandleMinLength", "Make sure the scroll handle is at least this big", [common_type::none, common_type::float, common_type::float_var])]
  scroll_handle_min_length: ParamVar,

  #[shard_param("ScrollBarInnerMargin", "Margin between contents and scroll bar.", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_inner_margin: ParamVar,

  #[shard_param("ScrollBarOuterMargin", "Margin between scroll bar and the outer container (e.g. right of a vertical scroll bar).", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_outer_margin: ParamVar,

  #[shard_param("ScrollBarDormantOpacity", "Opacity of the scroll bar when dormant.", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_dormant_opacity: ParamVar,

  // ------ Visuals members ------
  #[shard_param("DarkMode", "If true, the visuals are overall dark with light text. If false, the visuals are overall light with dark text.", [common_type::none, common_type::bool, common_type::bool_var])]
  dark_mode: ParamVar,

  #[shard_param("OverrideTextColor", "Override default text color for all text.", [common_type::none, common_type::color, common_type::color_var])]
  override_text_color: ParamVar,

  #[shard_param("Selection", "", [common_type::none, common_type::any_table, common_type::any_table_var])]
  selection: ParamVar,

  #[shard_param("HyperlinkColor", "The color used for Hyperlink", [common_type::none, common_type::color, common_type::color_var])]
  hyperlink_color: ParamVar,

  #[shard_param("FaintBgColor", "Something just barely different from the background color. Used for Grid::striped.", [common_type::none, common_type::color, common_type::color_var])]
  faint_bg_color: ParamVar,

  #[shard_param("ExtremeBgColor", "Very dark or light color (for corresponding theme). Used as the background of text edits, scroll bars and others things that needs to look different from other interactive stuff.", [common_type::none, common_type::color, common_type::color_var])]
  extreme_bg_color: ParamVar,

  #[shard_param("CodeBgColor", "Background color behind code-styled monospaced labels.", [common_type::none, common_type::color, common_type::color_var])]
  code_bg_color: ParamVar,

  #[shard_param("WarnFgColor", "A good color for warning text (e.g. orange).", [common_type::none, common_type::color, common_type::color_var])]
  warn_fg_color: ParamVar,

  #[shard_param("ErrorFgColor", "A good color for error text (e.g. red).", [common_type::none, common_type::color, common_type::color_var])]
  error_fg_color: ParamVar,

  #[shard_param("WindowRounding", "Window corner rounding.", [common_type::none, common_type::float4, common_type::float4_var])]
  window_rounding: ParamVar,

  #[shard_param("WindowShadow", "Window shadow size.", [common_type::none, common_type::any_table, common_type::any_table_var])]
  window_shadow: ParamVar,

  #[shard_param("WindowFill", "Window background color.", [common_type::none, common_type::color, common_type::color_var])]
  window_fill: ParamVar,

  #[shard_param("WindowStroke", "Window stroke (border) color and thickness.", [common_type::none, common_type::any_table, common_type::any_table_var])]
  window_stroke: ParamVar,

  #[shard_param("MenuRounding", "Menu corner rounding.", [common_type::none, common_type::float4, common_type::float4_var])]
  menu_rounding: ParamVar,

  #[shard_param("PanelFill", "Panel background color.", [common_type::none, common_type::color, common_type::color_var])]
  panel_fill: ParamVar,

  #[shard_param("PopupShadow", "Popup shadow size.", [common_type::none, common_type::any_table, common_type::any_table_var])]
  popup_shadow: ParamVar,

  #[shard_param("ResizeCornerSize", "Corner rounding for resize handle rects.", [common_type::none, common_type::float, common_type::float_var])]
  resize_corner_size: ParamVar,

  #[shard_param("TextCursorWidth", "Width of the line cursor when hovering over InputText etc.", [common_type::none, common_type::float, common_type::float_var])]
  text_cursor_width: ParamVar,

  #[shard_param("TextCursorPreview", "Show where the text cursor would be if you clicked.", [common_type::none, common_type::bool, common_type::bool_var])]
  text_cursor_preview: ParamVar,

  #[shard_param("ClipRectMargin", "Allow child widgets to be just on the border and still have a stroke with some thickness", [common_type::none, common_type::float, common_type::float_var])]
  clip_rect_margin: ParamVar,

  #[shard_param("ButtonFrame", "Show a background behind buttons.", [common_type::none, common_type::bool, common_type::bool_var])]
  button_frame: ParamVar,

  #[shard_param("CollapsingHeaderFrame", "Show a background behind collapsing headers.", [common_type::none, common_type::bool, common_type::bool_var])]
  collapsing_header_frame: ParamVar,

  #[shard_param("IndentHasLeftVLine", "Draw a vertical lien left of indented region, in e.g. CollapsingHeader.", [common_type::none, common_type::bool, common_type::bool_var])]
  indent_has_left_vline: ParamVar,

  #[shard_param("Striped", "Whether or not Grids and Tables should be striped by default (have alternating rows differently colored).", [common_type::none, common_type::bool, common_type::bool_var])]
  striped: ParamVar,

  #[shard_param("SliderTrailingFill", "Show trailing color behind the circle of a Slider. Default is OFF. Enabling this will affect ALL sliders, and can be enabled/disabled per slider with Slider::trailing_fill.", [common_type::none, common_type::bool, common_type::bool_var])]
  slider_trailing_fill: ParamVar,
}

impl Default for StyleShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      inherit_default: false.into(),
      override_text_style: ParamVar::default(),
      font_id: ParamVar::default(),
      text_styles: ParamVar::default(),
      drag_value_text_style: ParamVar::default(),
      wrap: ParamVar::default(),
      animation_time: ParamVar::default(),
      explanation_tooltips: ParamVar::default(),
      resize_grab_radius_side: ParamVar::default(),
      resize_grab_radius_corner: ParamVar::default(),
      show_tooltips_only_when_still: ParamVar::default(),

      // ------ Spacing members ------
      item_spacing: ParamVar::default(),
      window_margin: ParamVar::default(),
      button_padding: ParamVar::default(),
      menu_margin: ParamVar::default(),
      indent: ParamVar::default(),
      interact_size: ParamVar::default(),
      slider_width: ParamVar::default(),
      combo_width: ParamVar::default(),
      text_edit_width: ParamVar::default(),
      icon_width: ParamVar::default(),
      icon_width_inner: ParamVar::default(),
      icon_spacing: ParamVar::default(),
      default_area_size: ParamVar::default(),
      tooltip_width: ParamVar::default(),
      indent_ends_with_horizontal_line: ParamVar::default(),
      combo_height: ParamVar::default(),

      // ------ Scroll bar ------
      scroll_bar_floating: ParamVar::default(),
      scroll_bar_width: ParamVar::default(),
      scroll_bar_floating_width: ParamVar::default(),
      scroll_bar_floating_allocated_width: ParamVar::default(),
      scroll_handle_min_length: ParamVar::default(),
      scroll_bar_inner_margin: ParamVar::default(),
      scroll_bar_outer_margin: ParamVar::default(),
      scroll_bar_dormant_opacity: ParamVar::default(),

      // ------ Visuals members ------
      dark_mode: ParamVar::default(),
      override_text_color: ParamVar::default(),
      hyperlink_color: ParamVar::default(),
      faint_bg_color: ParamVar::default(),
      extreme_bg_color: ParamVar::default(),
      code_bg_color: ParamVar::default(),
      warn_fg_color: ParamVar::default(),
      error_fg_color: ParamVar::default(),
      window_rounding: ParamVar::default(),
      window_shadow: ParamVar::default(),
      window_fill: ParamVar::default(),
      menu_rounding: ParamVar::default(),
      panel_fill: ParamVar::default(),
      resize_corner_size: ParamVar::default(),
      text_cursor_width: ParamVar::default(),
      text_cursor_preview: ParamVar::default(),
      clip_rect_margin: ParamVar::default(),
      button_frame: ParamVar::default(),
      collapsing_header_frame: ParamVar::default(),
      indent_has_left_vline: ParamVar::default(),
      striped: ParamVar::default(),
      slider_trailing_fill: ParamVar::default(),
      selection: ParamVar::default(),
      window_stroke: ParamVar::default(),
      popup_shadow: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for StyleShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    crate::util::require_context(&mut self.required);
    crate::util::require_parents(&mut self.required);
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, ctx: &Context, _input: &Var) -> Result<Var, &str> {
    let mut global_style = None;

    let no_default: bool = (&self.inherit_default.0).try_into()?;
    let dark_mode: bool = self.dark_mode.get().try_into().unwrap_or(true);

    let style = if let Ok(ui) = crate::util::get_parent_ui(self.parents.get()) {
      ui.style_mut()
    } else {
      global_style = Some(
        (*crate::util::get_current_context(&self.contexts)?
          .egui_ctx
          .style())
        .clone(),
      );
      global_style.as_mut().unwrap()
    };

    if no_default {
      *style = egui::Style::default();

      style.visuals = if dark_mode {
        egui::Visuals::dark()
      } else {
        egui::Visuals::light()
      };
    }

    when_set(&self.animation_time, |v| {
      Ok(style.animation_time = v.try_into()?)
    })?;

    let wrap_enum: Result<TextWrap, _> = self.wrap.get().try_into();
    let wrap_bool: Result<bool, _> = self.wrap.get().try_into();
    match (wrap_enum, wrap_bool) {
      (Ok(wrap), _) => match wrap {
        TextWrap::Extend => style.wrap_mode = Some(egui::TextWrapMode::Extend),
        TextWrap::Wrap => style.wrap_mode = Some(egui::TextWrapMode::Wrap),
        TextWrap::Truncate => style.wrap_mode = Some(egui::TextWrapMode::Truncate),
      },

      (_, Ok(true)) => style.wrap_mode = Some(egui::TextWrapMode::Wrap),
      (_, Ok(false)) => style.wrap_mode = Some(egui::TextWrapMode::Extend),
      _ => {}
    }

    when_set(&self.explanation_tooltips, |v| {
      Ok(style.explanation_tooltips = v.try_into()?)
    })?;

    when_set(&self.override_text_style, |v| {
      Ok(style.override_text_style = Some(into_text_style(v)?))
    })?;

    when_set(&self.font_id, |v| {
      Ok(style.override_font_id = Some(into_font_id(v, ctx)?))
    })?;

    when_set(&self.drag_value_text_style, |v| {
      Ok(style.drag_value_text_style = into_text_style(v)?)
    })?;

    when_set(&self.text_styles, |v| {
      let tbl: TableVar = v.try_into()?;
      for (k, v) in tbl {
        let text_style = into_text_style(&k)?;
        let font_id = into_font_id(&v, ctx)?;
        style
          .text_styles
          .entry(text_style)
          .and_modify(|x: &mut egui::FontId| *x = font_id.clone())
          .or_insert(font_id);
      }
      Ok(())
    })?;

    when_set(&self.resize_grab_radius_side, |v| {
      Ok(style.interaction.resize_grab_radius_side = v.try_into()?)
    })?;

    when_set(&self.resize_grab_radius_corner, |v| {
      Ok(style.interaction.resize_grab_radius_corner = v.try_into()?)
    })?;

    when_set(&self.show_tooltips_only_when_still, |v| {
      Ok(style.interaction.show_tooltips_only_when_still = v.try_into()?)
    })?;

    // Spacing stuff
    let spacing = &mut style.spacing;

    when_set(&self.item_spacing, |v| {
      Ok(spacing.item_spacing = into_vec2(v)?)
    })?;

    when_set(&self.window_margin, |v| {
      Ok(spacing.window_margin = into_margin(v)?)
    })?;

    when_set(&self.button_padding, |v| {
      Ok(spacing.button_padding = into_vec2(v)?)
    })?;

    when_set(&self.menu_margin, |v| {
      Ok(spacing.menu_margin = into_margin(v)?)
    })?;

    when_set(&self.indent, |v| Ok(spacing.indent = v.try_into()?))?;

    when_set(&self.interact_size, |v| {
      Ok(spacing.interact_size = into_vec2(v)?)
    })?;

    when_set(&self.slider_width, |v| {
      Ok(spacing.slider_width = v.try_into()?)
    })?;

    when_set(&self.combo_width, |v| {
      Ok(spacing.combo_width = v.try_into()?)
    })?;

    when_set(&self.text_edit_width, |v| {
      Ok(spacing.text_edit_width = v.try_into()?)
    })?;

    when_set(&self.icon_width, |v| Ok(spacing.icon_width = v.try_into()?))?;

    when_set(&self.icon_width_inner, |v| {
      Ok(spacing.icon_width_inner = v.try_into()?)
    })?;

    when_set(&self.icon_spacing, |v| {
      Ok(spacing.icon_spacing = v.try_into()?)
    })?;

    when_set(&self.default_area_size, |v| {
      Ok(spacing.default_area_size = into_vec2(v)?)
    })?;

    when_set(&self.tooltip_width, |v| {
      Ok(spacing.tooltip_width = v.try_into()?)
    })?;

    when_set(&self.indent_ends_with_horizontal_line, |v| {
      Ok(spacing.indent_ends_with_horizontal_line = v.try_into()?)
    })?;

    when_set(&self.combo_height, |v| {
      Ok(spacing.combo_height = v.try_into()?)
    })?;

    // Scroll bar
    when_set(&self.scroll_bar_floating, |v| {
      Ok(spacing.scroll.floating = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_width, |v| {
      Ok(spacing.scroll.bar_width = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_floating_width, |v| {
      Ok(spacing.scroll.floating_width = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_floating_allocated_width, |v| {
      Ok(spacing.scroll.floating_allocated_width = v.try_into()?)
    })?;

    when_set(&self.scroll_handle_min_length, |v| {
      Ok(spacing.scroll.handle_min_length = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_inner_margin, |v| {
      Ok(spacing.scroll.bar_inner_margin = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_outer_margin, |v| {
      Ok(spacing.scroll.bar_outer_margin = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_dormant_opacity, |v| {
      let v = v.try_into()?;
      spacing.scroll.dormant_background_opacity = v;
      Ok(spacing.scroll.dormant_handle_opacity = v)
    })?;

    // Visuals stuff
    let visuals = &mut style.visuals;

    when_set(&self.dark_mode, |v| Ok(visuals.dark_mode = dark_mode))?;

    when_set(&self.override_text_color, |v| {
      Ok(visuals.override_text_color = Some(into_color(v)?))
    })?;

    // Need to fix color ones these to use into_color instead:
    when_set(&self.hyperlink_color, |v| {
      Ok(visuals.hyperlink_color = into_color(v)?)
    })?;

    when_set(&self.faint_bg_color, |v| {
      Ok(visuals.faint_bg_color = into_color(v)?)
    })?;

    when_set(&self.extreme_bg_color, |v| {
      Ok(visuals.extreme_bg_color = into_color(v)?)
    })?;

    when_set(&self.code_bg_color, |v| {
      Ok(visuals.code_bg_color = into_color(v)?)
    })?;

    when_set(&self.warn_fg_color, |v| {
      Ok(visuals.warn_fg_color = into_color(v)?)
    })?;

    when_set(&self.error_fg_color, |v| {
      Ok(visuals.error_fg_color = into_color(v)?)
    })?;

    when_set(&self.window_rounding, |v| {
      Ok(visuals.window_rounding = into_rounding(v)?)
    })?;

    when_set(&self.window_shadow, |v| {
      Ok(visuals.window_shadow = into_shadow(v, ctx)?)
    })?;

    when_set(&self.window_fill, |v| {
      Ok(visuals.window_fill = into_color(v)?)
    })?;

    when_set(&self.menu_rounding, |v| {
      Ok(visuals.menu_rounding = into_rounding(v)?)
    })?;

    when_set(&self.panel_fill, |v| {
      Ok(visuals.panel_fill = into_color(v)?)
    })?;

    when_set(&self.resize_corner_size, |v| {
      Ok(visuals.resize_corner_size = v.try_into()?)
    })?;

    when_set(&self.text_cursor_width, |v| {
      Ok(visuals.text_cursor.stroke.width = v.try_into()?)
    })?;

    when_set(&self.text_cursor_preview, |v| {
      Ok(visuals.text_cursor.preview = v.try_into()?)
    })?;

    when_set(&self.clip_rect_margin, |v| {
      Ok(visuals.clip_rect_margin = v.try_into()?)
    })?;

    when_set(&self.button_frame, |v| {
      Ok(visuals.button_frame = v.try_into()?)
    })?;

    when_set(&self.collapsing_header_frame, |v| {
      Ok(visuals.collapsing_header_frame = v.try_into()?)
    })?;

    when_set(&self.indent_has_left_vline, |v| {
      Ok(visuals.indent_has_left_vline = v.try_into()?)
    })?;

    when_set(&self.striped, |v| Ok(visuals.striped = v.try_into()?))?;

    when_set(&self.slider_trailing_fill, |v| {
      Ok(visuals.slider_trailing_fill = v.try_into()?) 
    })?;

    when_set(&self.selection, |v| {
      let tbl: TableVar = v.try_into()?;
      let bg_fill = into_color(get_or_var(
        tbl.get_static("BGFill").ok_or("bg_fill missing")?,
        ctx,
      ))?;
      let stroke = into_stroke(
        get_or_var(tbl.get_static("Stroke").ok_or("stroke missing")?, ctx),
        ctx,
      )?;
      visuals.selection = egui::style::Selection { bg_fill, stroke };
      Ok(())
    })?;

    when_set(&self.window_stroke, |v| {
      Ok(visuals.window_stroke = into_stroke(v, ctx)?)
    })?;

    when_set(&self.popup_shadow, |v| {
      Ok(visuals.popup_shadow = into_shadow(v, ctx)?)
    })?;

    if let Some(global_style) = global_style.take() {
      crate::util::get_current_context(&self.contexts)?
        .egui_ctx
        .set_style(global_style);
    }

    Ok(Var::default())
  }
}

#[derive(shards::shard)]
#[shard_info("UI.WidgetStyle", "Apply style changes to the current UI scope.")]
struct WidgetStyleShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_warmup]
  parents: ParamVar,

  #[shard_warmup]
  contexts: ParamVar,

  #[shard_param("NonInteractive", "The style of a widget that you cannot interact with.", [common_type::none, common_type::any_table, common_type::any_table_var])]
  non_interactive: ParamVar,

  #[shard_param("Inactive", "The style of an interactive widget, such as a button, at rest.", [common_type::none, common_type::any_table, common_type::any_table_var])]
  inactive: ParamVar,

  #[shard_param("Hovered", "The style of an interactive widget while you hover it, or when it is highlighted.", [common_type::none, common_type::any_table, common_type::any_table_var])]
  hovered: ParamVar,

  #[shard_param("Active", "The style of an interactive widget as you are clicking or dragging it.", [common_type::none, common_type::any_table, common_type::any_table_var])]
  active: ParamVar,

  #[shard_param("Open", "The style of a button that has an open menu beneath it (e.g. a combo-box)", [common_type::none, common_type::any_table, common_type::any_table_var])]
  open: ParamVar,
}

impl Default for WidgetStyleShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      non_interactive: ParamVar::default(),
      inactive: ParamVar::default(),
      hovered: ParamVar::default(),
      active: ParamVar::default(),
      open: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for WidgetStyleShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    crate::util::require_context(&mut self.required);
    crate::util::require_parents(&mut self.required);
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, ctx: &Context, _input: &Var) -> Result<Var, &str> {
    let mut global_style = None;
    let style = if let Ok(ui) = crate::util::get_parent_ui(self.parents.get()) {
      ui.style_mut()
    } else {
      global_style = Some(
        (*crate::util::get_current_context(&self.contexts)?
          .egui_ctx
          .style())
        .clone(),
      );
      global_style.as_mut().unwrap()
    };

    let visuals = &mut style.visuals;

    when_set(&self.non_interactive, |v| {
      apply_widget_visuals(&mut visuals.widgets.noninteractive, v, ctx)
    })?;

    when_set(&self.inactive, |v| {
      apply_widget_visuals(&mut visuals.widgets.inactive, v, ctx)
    })?;

    when_set(&self.hovered, |v| {
      apply_widget_visuals(&mut visuals.widgets.hovered, v, ctx)
    })?;

    when_set(&self.active, |v| {
      apply_widget_visuals(&mut visuals.widgets.active, v, ctx)
    })?;

    when_set(&self.open, |v| {
      apply_widget_visuals(&mut visuals.widgets.open, v, ctx)
    })?;

    if let Some(global_style) = global_style.take() {
      crate::util::get_current_context(&self.contexts)?
        .egui_ctx
        .set_style(global_style);
    }

    Ok(Var::default())
  }
}

pub fn register_shards() {
  register_shard::<StyleShard>();
  register_shard::<WidgetStyleShard>();
  register_enum::<TextStyle>();
  register_enum::<FontFamily>();
}
