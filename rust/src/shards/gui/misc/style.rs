/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Style;
use crate::shard::Shard;
use crate::shards::gui::misc::style_util;
use crate::shards::gui::util;
use crate::shards::gui::ANY_TABLE_SLICE;
use crate::shards::gui::CONTEXTS_NAME;
use crate::shards::gui::EGUI_UI_SEQ_TYPE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc::SHColor;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Seq;
use crate::types::Table;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANY_TABLE_VAR_TYPES;
use crate::types::ANY_TYPES;

macro_rules! apply_style {
  ($table:ident, $name:literal, $type:ty, $style_path:expr) => {
    if let Some(value) = $table.get_static(cstr!($name)) {
      let value: $type = value.try_into()?;
      $style_path = value.into();
    }
  };
  ($table:ident, $name:literal, $type:ty, $style_path:expr, $convert:expr) => {
    if let Some(value) = $table.get_static(cstr!($name)) {
      let value: $type = value.try_into()?;
      $style_path = $convert(value).into();
    }
  };
}

impl Default for Style {
  fn default() -> Self {
    let mut ctx = ParamVar::default();
    ctx.set_name(CONTEXTS_NAME);
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      instance: ctx,
      parents,
      requiring: Vec::new(),
    }
  }
}

impl Shard for Style {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Style")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Style-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Style"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TABLE_VAR_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    let exp_info = ExposedInfo {
      exposedType: EGUI_UI_SEQ_TYPE,
      name: self.parents.get_name(),
      help: cstr!("The parent UI objects.").into(),
      ..ExposedInfo::default()
    };
    self.requiring.push(exp_info);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.instance.warmup(ctx);
    self.parents.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.parents.cleanup();
    self.instance.cleanup();

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let mut style = if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      (**ui.style()).clone()
    } else {
      let gui_ctx = util::get_current_context(&self.instance)?;
      (*gui_ctx.style()).clone()
    };

    let table: Table = input.try_into()?;
    // note: follows declaration orders in egui structs

    apply_style!(
      table,
      "override_text_style",
      &str,
      style.override_text_style,
      style_util::get_text_style
    );
    apply_style!(
      table,
      "override_font_id",
      Table,
      style.override_font_id,
      style_util::get_font_id
    );

    // text styles
    if let Some(text_styles) = table.get_static(cstr!("text_styles")) {
      let text_styles: Seq = text_styles.try_into()?;

      for var in text_styles {
        let text_style: Table = var.as_ref().try_into()?;
        if let Some(name) = text_style.get_static(cstr!("name")) {
          if let Some(key) = style_util::get_text_style(name.try_into()?) {
            if let Some(fontId) = style_util::get_font_id(text_style) {
              style
                .text_styles
                .entry(key)
                .and_modify(|x| *x = fontId.clone())
                .or_insert(fontId);
            }
          }
        }
      }
    }

    apply_style!(table, "wrap", bool, style.wrap);

    // spacing
    if let Some(spacing) = table.get_static(cstr!("spacing")) {
      let spacing: Table = spacing.try_into()?;

      apply_style!(
        spacing,
        "item_spacing",
        (f32, f32),
        style.spacing.item_spacing
      );
      apply_style!(
        spacing,
        "window_margin",
        (f32, f32, f32, f32),
        style.spacing.window_margin,
        style_util::get_margin
      );
      apply_style!(
        spacing,
        "button_padding",
        (f32, f32),
        style.spacing.button_padding
      );
      apply_style!(spacing, "indent", f32, style.spacing.indent);
      apply_style!(
        spacing,
        "interact_size",
        (f32, f32),
        style.spacing.interact_size
      );
      apply_style!(spacing, "slider_width", f32, style.spacing.slider_width);
      apply_style!(
        spacing,
        "text_edit_width",
        f32,
        style.spacing.text_edit_width
      );
      apply_style!(spacing, "icon_width", f32, style.spacing.icon_width);
      apply_style!(
        spacing,
        "icon_width_inner",
        f32,
        style.spacing.icon_width_inner
      );
      apply_style!(spacing, "icon_spacing", f32, style.spacing.icon_spacing);
      apply_style!(
        spacing,
        "indent_ends_with_horizontal_line",
        bool,
        style.spacing.indent_ends_with_horizontal_line
      );
      apply_style!(spacing, "combo_height", f32, style.spacing.combo_height);
      apply_style!(
        spacing,
        "scroll_bar_width",
        f32,
        style.spacing.scroll_bar_width
      );
    }

    // interaction
    if let Some(interaction) = table.get_static(cstr!("interaction")) {
      let interaction: Table = interaction.try_into()?;

      apply_style!(
        interaction,
        "resize_grab_radius_side",
        f32,
        style.interaction.resize_grab_radius_side
      );
      apply_style!(
        interaction,
        "resize_grab_radius_corner",
        f32,
        style.interaction.resize_grab_radius_corner
      );
      apply_style!(
        interaction,
        "show_tooltips_only_when_still",
        bool,
        style.interaction.show_tooltips_only_when_still
      );
    }

    // visuals
    if let Some(visuals) = table.get_static(cstr!("visuals")) {
      let visuals: Table = visuals.try_into()?;

      apply_style!(visuals, "dark_mode", bool, style.visuals.dark_mode);
      apply_style!(
        visuals,
        "override_text_color",
        SHColor,
        style.visuals.override_text_color,
        style_util::get_color
      );

      if let Some(widgets) = visuals.get_static(cstr!("widgets")) {
        let widgets: Table = widgets.try_into()?;

        Style::apply_widget_visuals(
          &mut style.visuals.widgets.noninteractive,
          &widgets,
          cstr!("noninteractive"),
        )?;
        Style::apply_widget_visuals(
          &mut style.visuals.widgets.inactive,
          &widgets,
          cstr!("inactive"),
        )?;
        Style::apply_widget_visuals(
          &mut style.visuals.widgets.hovered,
          &widgets,
          cstr!("hovered"),
        )?;
        Style::apply_widget_visuals(&mut style.visuals.widgets.active, &widgets, cstr!("active"))?;
        Style::apply_widget_visuals(&mut style.visuals.widgets.open, &widgets, cstr!("open"))?;
      }

      if let Some(selection) = visuals.get_static(cstr!("selection")) {
        let selection: Table = selection.try_into()?;

        apply_style!(
          selection,
          "bg_fill",
          SHColor,
          style.visuals.selection.bg_fill,
          style_util::get_color
        );
        apply_style!(
          selection,
          "stroke",
          Table,
          style.visuals.selection.stroke,
          style_util::get_stroke
        );
      }

      apply_style!(
        visuals,
        "hyperlink_color",
        SHColor,
        style.visuals.hyperlink_color,
        style_util::get_color
      );
      apply_style!(
        visuals,
        "faint_bg_color",
        SHColor,
        style.visuals.faint_bg_color,
        style_util::get_color
      );
      apply_style!(
        visuals,
        "extreme_bg_color",
        SHColor,
        style.visuals.extreme_bg_color,
        style_util::get_color
      );
      apply_style!(
        visuals,
        "code_bg_color",
        SHColor,
        style.visuals.code_bg_color,
        style_util::get_color
      );
      apply_style!(
        visuals,
        "warn_fg_color",
        SHColor,
        style.visuals.warn_fg_color,
        style_util::get_color
      );
      apply_style!(
        visuals,
        "error_fg_color",
        SHColor,
        style.visuals.error_fg_color,
        style_util::get_color
      );
      apply_style!(
        visuals,
        "window_rounding",
        (f32, f32, f32, f32),
        style.visuals.window_rounding,
        style_util::get_rounding
      );
      apply_style!(
        visuals,
        "window_shadow",
        Table,
        style.visuals.window_shadow,
        style_util::get_shadow
      );
      apply_style!(
        visuals,
        "popup_shadow",
        Table,
        style.visuals.popup_shadow,
        style_util::get_shadow
      );
      apply_style!(
        visuals,
        "resize_corner_size",
        f32,
        style.visuals.resize_corner_size
      );
      apply_style!(
        visuals,
        "text_cursor_width",
        f32,
        style.visuals.text_cursor_width
      );
      apply_style!(
        visuals,
        "text_cursor_preview",
        bool,
        style.visuals.text_cursor_preview
      );
      apply_style!(
        visuals,
        "clip_rect_margin",
        f32,
        style.visuals.clip_rect_margin
      );
      apply_style!(visuals, "button_frame", bool, style.visuals.button_frame);
      apply_style!(
        visuals,
        "collapsing_header_frame",
        bool,
        style.visuals.collapsing_header_frame
      );
    }

    apply_style!(table, "animation_time", f32, style.animation_time);

    // debug
    if let Some(debug) = table.get_static(cstr!("debug")) {
      let debug: Table = debug.try_into()?;

      apply_style!(debug, "debug_on_hover", bool, style.debug.debug_on_hover);
      apply_style!(
        debug,
        "show_expand_width",
        bool,
        style.debug.show_expand_width
      );
      apply_style!(
        debug,
        "show_expand_height",
        bool,
        style.debug.show_expand_height
      );
      apply_style!(debug, "show_resize", bool, style.debug.show_resize);
    }

    apply_style!(
      table,
      "explanation_tooltips",
      bool,
      style.explanation_tooltips
    );

    if let Some(ui) = util::get_current_parent(*self.parents.get())? {
      ui.set_style(style);
    } else {
      let gui_ctx = util::get_current_context(&self.instance)?;
      gui_ctx.set_style(style);
    }

    Ok(*input)
  }
}

impl Style {
  fn apply_widget_visuals(
    visuals: &mut egui::style::WidgetVisuals,
    widgets: &Table,
    name: &'static str,
  ) -> Result<(), &'static str> {
    if let Some(var) = widgets.get_static(name) {
      let table: Table = var.try_into()?;

      apply_style!(
        table,
        "bg_fill",
        SHColor,
        visuals.bg_fill,
        style_util::get_color
      );
      apply_style!(
        table,
        "bg_stroke",
        Table,
        visuals.bg_stroke,
        style_util::get_stroke
      );
      apply_style!(
        table,
        "rounding",
        (f32, f32, f32, f32),
        visuals.rounding,
        style_util::get_rounding
      );
      apply_style!(
        table,
        "fg_stroke",
        Table,
        visuals.fg_stroke,
        style_util::get_stroke
      );
      apply_style!(table, "expansion", f32, visuals.expansion);
    }

    Ok(())
  }
}
