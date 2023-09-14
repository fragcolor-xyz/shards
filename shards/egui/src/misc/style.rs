/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

// use super::Style;
// use crate::EGUI_UI_TYPE;
// use crate::misc::style_util;
// use crate::util;
// use crate::CONTEXTS_NAME;
// use crate::HELP_OUTPUT_EQUAL_INPUT;
// use crate::PARENTS_UI_NAME;
// use shards::shard::LegacyShard;
// use shards::shardsc::SHColor;
// use shards::types::Context;
// use shards::types::ExposedInfo;
// use shards::types::ExposedTypes;
// use shards::types::OptionalString;
// use shards::types::ParamVar;
// use shards::types::Seq;
// use shards::types::Table;
// use shards::types::Types;
// use shards::types::Var;
// use shards::types::ANY_TABLE_VAR_TYPES;
// use shards::types::ANY_TYPES;

// macro_rules! apply_style {
//   ($table:ident, $name:literal, $type:ty, $style_path:expr) => {
//     if let Some(value) = $table.get_static($name) {
//       let value: $type = value.try_into().map_err(|e| {
//         shlog!("{}: {}", $name, e);
//         "Invalid attribute value received"
//       })?;
//       $style_path = value.into();
//     }
//   };
//   ($table:ident, $name:literal, $type:ty, $style_path:expr, $convert:expr) => {
//     if let Some(value) = $table.get_static($name) {
//       let value: $type = value.try_into().map_err(|e| {
//         shlog!("{}: {}", $name, e);
//         "Invalid attribute value received"
//       })?;
//       $style_path = $convert(value)?.into();
//     }
//   };
// }

// impl Default for Style {
//   fn default() -> Self {
//     let mut ctx = ParamVar::default();
//     ctx.set_name(CONTEXTS_NAME);
//     let mut parents = ParamVar::default();
//     parents.set_name(PARENTS_UI_NAME);
//     Self {
//       instance: ctx,
//       parents,
//       requiring: Vec::new(),
//     }
//   }
// }

// impl LegacyShard for Style {
//   fn registerName() -> &'static str
//   where
//     Self: Sized,
//   {
//     cstr!("UI.Style")
//   }

//   fn hash() -> u32
//   where
//     Self: Sized,
//   {
//     compile_time_crc32::crc32!("UI.Style-rust-0x20200101")
//   }

//   fn name(&mut self) -> &str {
//     "UI.Style"
//   }

//   fn help(&mut self) -> OptionalString {
//     OptionalString(shccstr!("Apply style changes to the current UI scope."))
//   }

//   fn inputTypes(&mut self) -> &Types {
//     &ANY_TABLE_VAR_TYPES
//   }

//   fn inputHelp(&mut self) -> OptionalString {
//     OptionalString(shccstr!("A table describing the style to apply."))
//   }

//   fn outputTypes(&mut self) -> &Types {
//     &ANY_TYPES
//   }

//   fn outputHelp(&mut self) -> OptionalString {
//     *HELP_OUTPUT_EQUAL_INPUT
//   }

//   fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
//     self.requiring.clear();
//     util::require_parents(&mut self.requiring);
//     Some(&self.requiring)
//   }

//   fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
//     self.instance.warmup(ctx);
//     self.parents.warmup(ctx);

//     Ok(())
//   }

//   fn cleanup(&mut self) -> Result<(), &str> {
//     self.parents.cleanup();
//     self.instance.cleanup();

//     Ok(())
//   }

//   fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
//     let mut style = if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
//       (**ui.style()).clone()
//     } else {
//       let gui_ctx = util::get_current_context(&self.instance)?;
//       (*gui_ctx.style()).clone()
//     };

//     let table: Table = input.try_into()?;
//     // note: follows declaration orders in egui structs

//     apply_style!(
//       table,
//       "override_text_style",
//       &str,
//       style.override_text_style,
//       style_util::get_text_style
//     );
//     apply_style!(
//       table,
//       "override_font_id",
//       Table,
//       style.override_font_id,
//       style_util::get_font_id
//     );

//     // text styles
//     if let Some(text_styles) = table.get_static("text_styles") {
//       let text_styles: Seq = text_styles.try_into()?;

//       for var in text_styles.iter() {
//         let text_style: Table = var.try_into()?;
//         if let Some(name) = text_style.get_static("name") {
//           let key: egui::TextStyle = style_util::get_text_style(name.try_into().map_err(|e| {
//             shlog!("{}: {}", "name", e);
//             "Invalid attribute value received"
//           })?)?;
//           let fontId: egui::FontId = style_util::get_font_id(text_style)?;
//           style
//             .text_styles
//             .entry(key)
//             .and_modify(|x| *x = fontId.clone())
//             .or_insert(fontId);
//         } else {
//           return Err("name is missing from a table provided in text_styles");
//         }
//       }
//     }

//     if let Some(name) = table.get_static("override_text_style") {
//       if !style
//         .text_styles
//         .contains_key(&style_util::get_text_style(name.try_into()?)?)
//       {
//         return Err("Provided override_text_style does not match any existing text style");
//       }
//     }

//     apply_style!(table, "wrap", bool, style.wrap);

//     // spacing
//     if let Some(spacing) = table.get_static("spacing") {
//       let spacing: Table = spacing.try_into()?;

//       apply_style!(
//         spacing,
//         "item_spacing",
//         (f32, f32),
//         style.spacing.item_spacing
//       );
//       apply_style!(
//         spacing,
//         "window_margin",
//         (f32, f32, f32, f32),
//         style.spacing.window_margin,
//         style_util::get_margin
//       );
//       apply_style!(
//         spacing,
//         "button_padding",
//         (f32, f32),
//         style.spacing.button_padding
//       );
//       apply_style!(spacing, "indent", f32, style.spacing.indent);
//       apply_style!(
//         spacing,
//         "interact_size",
//         (f32, f32),
//         style.spacing.interact_size
//       );
//       apply_style!(spacing, "slider_width", f32, style.spacing.slider_width);
//       apply_style!(
//         spacing,
//         "text_edit_width",
//         f32,
//         style.spacing.text_edit_width
//       );
//       apply_style!(spacing, "icon_width", f32, style.spacing.icon_width);
//       apply_style!(
//         spacing,
//         "icon_width_inner",
//         f32,
//         style.spacing.icon_width_inner
//       );
//       apply_style!(spacing, "icon_spacing", f32, style.spacing.icon_spacing);
//       apply_style!(
//         spacing,
//         "indent_ends_with_horizontal_line",
//         bool,
//         style.spacing.indent_ends_with_horizontal_line
//       );
//       apply_style!(spacing, "combo_height", f32, style.spacing.combo_height);
//       apply_style!(
//         spacing,
//         "scroll_bar_width",
//         f32,
//         style.spacing.scroll_bar_width
//       );
//     }

//     // interaction
//     if let Some(interaction) = table.get_static("interaction") {
//       let interaction: Table = interaction.try_into()?;

//       apply_style!(
//         interaction,
//         "resize_grab_radius_side",
//         f32,
//         style.interaction.resize_grab_radius_side
//       );
//       apply_style!(
//         interaction,
//         "resize_grab_radius_corner",
//         f32,
//         style.interaction.resize_grab_radius_corner
//       );
//       apply_style!(
//         interaction,
//         "show_tooltips_only_when_still",
//         bool,
//         style.interaction.show_tooltips_only_when_still
//       );
//     }

//     // visuals
//     if let Some(visuals) = table.get_static("visuals") {
//       let visuals: Table = visuals.try_into().map_err(|e| {
//         shlog!("{}: {}", "visuals", e);
//         "Invalid attribute value received"
//       })?;

//       apply_style!(visuals, "dark_mode", bool, style.visuals.dark_mode);
//       apply_style!(
//         visuals,
//         "override_text_color",
//         SHColor,
//         style.visuals.override_text_color,
//         style_util::get_color
//       );

//       if let Some(widgets) = visuals.get_static("widgets") {
//         let widgets: Table = widgets.try_into().map_err(|e| {
//           shlog!("{}: {}", "widgets", e);
//           "Invalid attribute value received"
//         })?;

//         Style::apply_widget_visuals(
//           &mut style.visuals.widgets.noninteractive,
//           &widgets,
//           "noninteractive",
//         )?;
//         Style::apply_widget_visuals(
//           &mut style.visuals.widgets.inactive,
//           &widgets,
//           "inactive",
//         )?;
//         Style::apply_widget_visuals(
//           &mut style.visuals.widgets.hovered,
//           &widgets,
//           "hovered",
//         )?;
//         Style::apply_widget_visuals(&mut style.visuals.widgets.active, &widgets, "active")?;
//         Style::apply_widget_visuals(&mut style.visuals.widgets.open, &widgets, "open")?;
//       }

//       if let Some(selection) = visuals.get_static("selection") {
//         let selection: Table = selection.try_into().map_err(|e| {
//           shlog!("{}: {}", "selection", e);
//           "Invalid attribute value received"
//         })?;

//         apply_style!(
//           selection,
//           "bg_fill",
//           SHColor,
//           style.visuals.selection.bg_fill,
//           style_util::get_color
//         );
//         apply_style!(
//           selection,
//           "stroke",
//           Table,
//           style.visuals.selection.stroke,
//           style_util::get_stroke
//         );
//       }

//       apply_style!(
//         visuals,
//         "hyperlink_color",
//         SHColor,
//         style.visuals.hyperlink_color,
//         style_util::get_color
//       );
//       apply_style!(
//         visuals,
//         "faint_bg_color",
//         SHColor,
//         style.visuals.faint_bg_color,
//         style_util::get_color
//       );
//       apply_style!(
//         visuals,
//         "extreme_bg_color",
//         SHColor,
//         style.visuals.extreme_bg_color,
//         style_util::get_color
//       );
//       apply_style!(
//         visuals,
//         "code_bg_color",
//         SHColor,
//         style.visuals.code_bg_color,
//         style_util::get_color
//       );
//       apply_style!(
//         visuals,
//         "warn_fg_color",
//         SHColor,
//         style.visuals.warn_fg_color,
//         style_util::get_color
//       );
//       apply_style!(
//         visuals,
//         "error_fg_color",
//         SHColor,
//         style.visuals.error_fg_color,
//         style_util::get_color
//       );
//       apply_style!(
//         visuals,
//         "window_rounding",
//         (f32, f32, f32, f32),
//         style.visuals.window_rounding,
//         style_util::get_rounding
//       );
//       apply_style!(
//         visuals,
//         "window_shadow",
//         Table,
//         style.visuals.window_shadow,
//         style_util::get_shadow
//       );
//       apply_style!(
//         visuals,
//         "window_fill",
//         SHColor,
//         style.visuals.window_fill,
//         style_util::get_color
//       );
//       apply_style!(
//         visuals,
//         "window_stroke",
//         Table,
//         style.visuals.window_stroke,
//         style_util::get_stroke
//       );
//       apply_style!(
//         visuals,
//         "popup_shadow",
//         Table,
//         style.visuals.popup_shadow,
//         style_util::get_shadow
//       );
//       apply_style!(
//         visuals,
//         "resize_corner_size",
//         f32,
//         style.visuals.resize_corner_size
//       );
//       apply_style!(
//         visuals,
//         "text_cursor_width",
//         f32,
//         style.visuals.text_cursor_width
//       );
//       apply_style!(
//         visuals,
//         "text_cursor_preview",
//         bool,
//         style.visuals.text_cursor_preview
//       );
//       apply_style!(
//         visuals,
//         "clip_rect_margin",
//         f32,
//         style.visuals.clip_rect_margin
//       );
//       apply_style!(visuals, "button_frame", bool, style.visuals.button_frame);
//       apply_style!(
//         visuals,
//         "collapsing_header_frame",
//         bool,
//         style.visuals.collapsing_header_frame
//       );
//     }

//     apply_style!(table, "animation_time", f32, style.animation_time);

//     // debug
//     if let Some(debug) = table.get_static("debug") {
//       let debug: Table = debug.try_into().map_err(|e| {
//         shlog!("{}: {}", "debug", e);
//         "Invalid attribute value received"
//       })?;

//       apply_style!(debug, "debug_on_hover", bool, style.debug.debug_on_hover);
//       apply_style!(
//         debug,
//         "show_expand_width",
//         bool,
//         style.debug.show_expand_width
//       );
//       apply_style!(
//         debug,
//         "show_expand_height",
//         bool,
//         style.debug.show_expand_height
//       );
//       apply_style!(debug, "show_resize", bool, style.debug.show_resize);
//     }

//     apply_style!(
//       table,
//       "explanation_tooltips",
//       bool,
//       style.explanation_tooltips
//     );

//     if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
//       ui.set_style(style);
//     } else {
//       let gui_ctx = util::get_current_context(&self.instance)?;
//       gui_ctx.set_style(style);
//     }

//     Ok(*input)
//   }
// }

// impl Style {
//   fn apply_widget_visuals(
//     visuals: &mut egui::style::WidgetVisuals,
//     widgets: &Table,
//     name: &'static str,
//   ) -> Result<(), &'static str> {
//     if let Some(var) = widgets.get_static(name) {
//       let table: Table = var.try_into()?;
//       apply_style!(
//         table,
//         "bg_fill",
//         SHColor,
//         visuals.bg_fill,
//         style_util::get_color
//       );
//       apply_style!(
//         table,
//         "bg_stroke",
//         Table,
//         visuals.bg_stroke,
//         style_util::get_stroke
//       );
//       apply_style!(
//         table,
//         "rounding",
//         (f32, f32, f32, f32),
//         visuals.rounding,
//         style_util::get_rounding
//       );
//       apply_style!(
//         table,
//         "fg_stroke",
//         Table,
//         visuals.fg_stroke,
//         style_util::get_stroke
//       );
//       apply_style!(table, "expansion", f32, visuals.expansion);
//     }

//     Ok(())
//   }
// }

use crate::util::{into_color, into_margin, into_rounding, into_shadow, into_vec2, into_stroke};
use crate::PARENTS_UI_NAME;
use shards::core::{register_enum, register_shard};
use shards::shard::Shard;
use shards::types::{common_type, TableVar};
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var, NONE_TYPES};

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

pub fn into_font_id(v: &Var) -> Result<egui::FontId, &'static str> {
  let tbl: TableVar = v.try_into()?;
  let size: f32 = tbl.get_static("Size").ok_or("Missing Size")?.try_into()?;
  let family = into_font_family(tbl.get_static("Family").ok_or("Missing Family")?)?;
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
  static ref FONT_ID_TYPES: Types = vec![common_type::none, common_type::any_table];

  // static ref SPACING_TYPES: Types = vec![common_type::none, common_type::any_table];
  // static ref INTERACTION_TYPES: Types = vec![common_type::none, common_type::any_table];
  // static ref VISUALS_TYPES: Types = vec![common_type::none, common_type::any_table];
  // static ref DEBUG_TYPES: Types = vec![common_type::none, common_type::any_table];
}

#[derive(shards::shard)]
#[shard_info("UI.Style", "Apply style changes to the current UI scope.")]
struct StyleShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_param(
    "TextStyle",
    "If set this will change the default TextStyle for all widgets.",
    TEXT_STYLE_TYPES
  )]
  text_style: ParamVar,
  #[shard_param(
    "FontId",
    "If set this will change the font family and size for all widgets.",
    FONT_ID_TYPES
  )]
  font_id: ParamVar,
  #[shard_param(
    "TextStyles",
    "The FontFamily and size you want to use for a specific TextStyle.",
    TEXT_STYLE_TYPES
  )]
  text_styles: ParamVar,
  #[shard_param(
    "DragValueTextStyle",
    "The style to use for DragValue text.",
    TEXT_STYLE_TYPES
  )]
  drag_value_text_style: ParamVar,
  #[shard_param("Wrap", "If set, labels buttons wtc will use this to determine whether or not to wrap the text at the right edge of the Ui they are in.", [common_type::none, common_type::bool])]
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
}

impl Default for StyleShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      text_style: ParamVar::default(),
      font_id: ParamVar::default(),
      text_styles: ParamVar::default(),
      drag_value_text_style: ParamVar::default(),
      wrap: ParamVar::default(),
      animation_time: ParamVar::default(),
      explanation_tooltips: ParamVar::default(),
      resize_grab_radius_side: ParamVar::default(),
      resize_grab_radius_corner: ParamVar::default(),
      show_tooltips_only_when_still: ParamVar::default(),
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

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    crate::util::require_parents(&mut self.required);
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let ui = crate::util::get_parent_ui(self.parents.get())?;
    let style: &mut egui::Style = ui.style_mut();

    when_set(&self.animation_time, |v| {
      Ok(style.animation_time = v.try_into()?)
    })?;

    when_set(&self.wrap, |v| Ok(style.wrap = Some(v.try_into()?)))?;

    when_set(&self.explanation_tooltips, |v| {
      Ok(style.explanation_tooltips = v.try_into()?)
    })?;

    when_set(&self.text_style, |v| {
      Ok(style.override_text_style = Some(into_text_style(v)?))
    })?;

    when_set(&self.font_id, |v| {
      Ok(style.override_font_id = Some(into_font_id(v)?))
    })?;

    when_set(&self.drag_value_text_style, |v| {
      Ok(style.drag_value_text_style = into_text_style(v)?)
    })?;

    when_set(&self.text_styles, |v| {
      let tbl: TableVar = v.try_into()?;
      for (k, v) in tbl {
        let text_style = into_text_style(&k)?;
        let font_id = into_font_id(&v)?;
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

    Ok(Var::default())
  }
}

#[derive(shards::shard)]
#[shard_info("UI.SpacingStyle", "")]
struct SpacingStyleShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_warmup]
  parents: ParamVar,

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

  #[shard_param("TooltipWidth", "Width of a tooltip (on_hover_ui, on_hover_text etc).", [common_type::none, common_type::float, common_type::float_var])]
  tooltip_width: ParamVar,

  #[shard_param("IndentEndsWithHorizontalLine", "End indented regions with a horizontal line", [common_type::none, common_type::bool])]
  indent_ends_with_horizontal_line: ParamVar,

  #[shard_param("ComboHeight", "Height of a combo-box before showing scroll bars.", [common_type::none, common_type::float, common_type::float_var])]
  combo_height: ParamVar,

  #[shard_param("ScrollBarWidth", "Width of a scroll bar.", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_width: ParamVar,

  #[shard_param("ScrollHandleMinLength", "Make sure the scroll handle is at least this big", [common_type::none, common_type::float, common_type::float_var])]
  scroll_handle_min_length: ParamVar,

  #[shard_param("ScrollBarInnerMargin", "Margin between contents and scroll bar.", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_inner_margin: ParamVar,

  #[shard_param("ScrollBarOuterMargin", "Margin between scroll bar and the outer container (e.g. right of a vertical scroll bar).", [common_type::none, common_type::float, common_type::float_var])]
  scroll_bar_outer_margin: ParamVar,
}

impl Default for SpacingStyleShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      parents: ParamVar::default(),
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
      tooltip_width: ParamVar::default(),
      indent_ends_with_horizontal_line: ParamVar::default(),
      combo_height: ParamVar::default(),
      scroll_bar_width: ParamVar::default(),
      scroll_handle_min_length: ParamVar::default(),
      scroll_bar_inner_margin: ParamVar::default(),
      scroll_bar_outer_margin: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SpacingStyleShard {
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

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    crate::util::require_parents(&mut self.required);
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let ui: &mut egui::Ui = crate::util::get_parent_ui(self.parents.get())?;
    let spacing = &mut ui.style_mut().spacing;

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

    when_set(&self.tooltip_width, |v| {
      Ok(spacing.tooltip_width = v.try_into()?)
    })?;

    when_set(&self.indent_ends_with_horizontal_line, |v| {
      Ok(spacing.indent_ends_with_horizontal_line = v.try_into()?)
    })?;

    when_set(&self.combo_height, |v| {
      Ok(spacing.combo_height = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_width, |v| {
      Ok(spacing.scroll_bar_width = v.try_into()?)
    })?;

    when_set(&self.scroll_handle_min_length, |v| {
      Ok(spacing.scroll_handle_min_length = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_inner_margin, |v| {
      Ok(spacing.scroll_bar_inner_margin = v.try_into()?)
    })?;

    when_set(&self.scroll_bar_outer_margin, |v| {
      Ok(spacing.scroll_bar_outer_margin = v.try_into()?)
    })?;

    Ok(Var::default())
  }
}

#[derive(shards::shard)]
#[shard_info("UI.VisualsStyle", "")]
struct VisualsStyleShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_warmup]
  parents: ParamVar,

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

impl Default for VisualsStyleShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      parents: ParamVar::default(),
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
impl Shard for VisualsStyleShard {
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

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    crate::util::require_parents(&mut self.required);
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let ui: &mut egui::Ui = crate::util::get_parent_ui(self.parents.get())?;
    let visuals = &mut ui.style_mut().visuals;

    when_set(&self.dark_mode, |v| Ok(visuals.dark_mode = v.try_into()?))?;

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
      Ok(visuals.window_shadow = into_shadow(v)?)
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
      Ok(visuals.text_cursor_width = v.try_into()?)
    })?;

    when_set(&self.text_cursor_preview, |v| {
      Ok(visuals.text_cursor_preview = v.try_into()?)
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
      let bg_fill = into_color(tbl.get_static("BGFill").ok_or("bg_fill missing")?)?;
      let stroke = into_stroke(tbl.get_static("Stroke").ok_or("stroke missing")?)?;
      visuals.selection = egui::style::Selection { bg_fill, stroke };
      Ok(())
    })?;

    when_set(&self.window_stroke, |v| {
      Ok(visuals.window_stroke = into_stroke(v)?)
    })?;

    when_set(&self.popup_shadow, |v| {
      Ok(visuals.popup_shadow = into_shadow(v)?)
    })?;

    Ok(Var::default())
  }
}

pub fn register_shards() {
  register_shard::<StyleShard>();
  register_shard::<SpacingStyleShard>();
  register_shard::<VisualsStyleShard>();
}
