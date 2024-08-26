/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Console;
use crate::misc::style_util;
use crate::util;
use crate::EguiId;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::LegacyShard;

use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Table;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TABLE_VAR_TYPES;
use shards::types::BOOL_TYPES_SLICE;
use shards::types::STRING_TYPES;

lazy_static! {
  static ref CONSOLE_PARAMETERS: Parameters = vec![
    (
      cstr!("ShowFilters"),
      shccstr!("Whether to display filter controls."),
      BOOL_TYPES_SLICE,
    )
      .into(),
    (
      cstr!("Style"),
      shccstr!("The console style."),
      &ANY_TABLE_VAR_TYPES[..],
    )
      .into(),
  ];
}

impl Default for Console {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      show_filters: ParamVar::new(false.into()),
      style: ParamVar::default(),
      filters: (true, true, true, true, true),
    }
  }
}

impl LegacyShard for Console {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Console")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Console-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Console"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A console with formatted logs."))
  }

  fn inputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The raw logs."))
  }

  fn outputTypes(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&CONSOLE_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.show_filters.set_param(value),
      1 => self.style.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.show_filters.get_param(),
      1 => self.style.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();
    util::require_parents(&mut self.requiring);
    Some(&self.requiring)
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.parents.warmup(context);
    self.show_filters.warmup(context);
    self.style.warmup(context);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.style.cleanup(ctx);
    self.show_filters.cleanup(ctx);
    self.parents.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let style = self.style.get();
      let mut theme = LogTheme::default();
      if !style.is_none() {
        let style: Table = style.try_into()?;

        if let Some(trace) = style.get_static("trace") {
          let trace: Table = trace.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Trace], trace)?;
        }

        if let Some(debug) = style.get_static("debug") {
          let debug: Table = debug.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Debug], debug)?;
        }

        if let Some(error) = style.get_static("error") {
          let error: Table = error.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Error], error)?;
        }

        if let Some(warning) = style.get_static("warning") {
          let warning: Table = warning.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Warning], warning)?;
        }

        if let Some(info) = style.get_static("info") {
          let info: Table = info.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Info], info)?;
        }

        if let Some(text) = style.get_static("text") {
          let text: Table = text.try_into()?;
          style_util::update_text_format(&mut theme.formats[LogLevel::Text], text)?;
        }
      }

      if self.show_filters.get().try_into()? {
        ui.horizontal(|ui| {
          if cfg!(debug_assertions) {
            ui.checkbox(&mut self.filters.0, "trace");
          }
          ui.checkbox(&mut self.filters.1, "debug");
          ui.checkbox(&mut self.filters.2, "error");
          ui.checkbox(&mut self.filters.3, "warning");
          ui.checkbox(&mut self.filters.4, "info");
        });
      }

      let filters = self.filters.clone();
      let mut layouter = |ui: &egui::Ui, string: &str, wrap_width: f32| {
        let mut layout_job = Console::highlight(ui.ctx(), &theme, string, filters);
        layout_job.wrap.max_width = wrap_width;
        ui.fonts(|f| f.layout_job(layout_job))
      };

      let mut text: &str = input.try_into()?;
      let code_editor = egui::TextEdit::multiline(&mut text)
        .code_editor()
        .desired_width(f32::INFINITY)
        .layouter(&mut layouter);

      let id_source = EguiId::new(self, 0);
      egui::ScrollArea::vertical()
        .id_source(id_source)
        .show(ui, |ui| ui.centered_and_justified(|ui| ui.add(code_editor)))
        .inner
        .inner;

      Ok(None)
    } else {
      Err("No UI parent")
    }
  }
}

impl Console {
  /// Memoized console highlighting
  fn highlight(
    ctx: &egui::Context,
    theme: &LogTheme,
    code: &str,
    filters: (bool, bool, bool, bool, bool),
  ) -> egui::text::LayoutJob {
    impl
      egui::util::cache::ComputerMut<
        (&LogTheme, &str, (bool, bool, bool, bool, bool)),
        egui::text::LayoutJob,
      > for Highlighter
    {
      fn compute(
        &mut self,
        (theme, code, filters): (&LogTheme, &str, (bool, bool, bool, bool, bool)),
      ) -> egui::text::LayoutJob {
        self.highlight(theme, code, filters)
      }
    }

    type HighlightCache = egui::util::cache::FrameCache<egui::text::LayoutJob, Highlighter>;

    ctx.memory_mut(|mem| {
      let highlight_cache = mem.caches.cache::<HighlightCache>();
      highlight_cache.get((theme, code, filters))
    })
  }
}

#[derive(Default)]
struct Highlighter {}

impl Highlighter {
  fn highlight(
    &self,
    theme: &LogTheme,
    mut text: &str,
    filters: (bool, bool, bool, bool, bool),
  ) -> egui::text::LayoutJob {
    // Extremely simple syntax highlighter for when we compile without syntect

    let mut job = egui::text::LayoutJob::default();

    while !text.is_empty() {
      let mut end;
      if text.starts_with("[trace]") {
        end = text.find('\n').unwrap_or(text.len());
        if filters.0 {
          job.append(&text[..end], 0.0, theme.formats[LogLevel::Trace].clone());
        } else {
          end += 1;
        }
      } else if text.starts_with("[debug]") {
        end = text.find('\n').unwrap_or(text.len());
        if filters.1 {
          job.append(&text[..end], 0.0, theme.formats[LogLevel::Debug].clone());
        } else {
          end += 1;
        }
      } else if text.starts_with("[error]") {
        end = text.find('\n').unwrap_or(text.len());
        if filters.2 {
          job.append(&text[..end], 0.0, theme.formats[LogLevel::Error].clone());
        } else {
          end += 1;
        }
      } else if text.starts_with("[warning]") {
        end = text.find('\n').unwrap_or(text.len());
        if filters.3 {
          job.append(&text[..end], 0.0, theme.formats[LogLevel::Warning].clone());
        } else {
          end += 1;
        }
      } else if text.starts_with("[info]") {
        end = text.find('\n').unwrap_or(text.len());
        if filters.4 {
          job.append(&text[..end], 0.0, theme.formats[LogLevel::Info].clone());
        } else {
          end += 1;
        }
      } else {
        let mut it = text.char_indices();
        it.next();
        end = it.next().map_or(text.len(), |(idx, _chr)| idx);
        job.append(&text[..end], 0.0, theme.formats[LogLevel::Text].clone());
      }
      end = std::cmp::min(end, text.len());
      text = &text[end..];
    }

    job
  }
}

#[derive(Hash)]
struct LogTheme {
  pub(crate) formats: enum_map::EnumMap<LogLevel, egui::TextFormat>,
}

impl Default for LogTheme {
  fn default() -> Self {
    let font_id = egui::FontId::monospace(12.0);
    Self {
      formats: enum_map::enum_map![
        LogLevel::Trace => egui::TextFormat::simple(font_id.clone(), egui::Color32::DARK_GRAY),
        LogLevel::Debug => egui::TextFormat::simple(font_id.clone(), egui::Color32::LIGHT_BLUE),
        LogLevel::Error => egui::TextFormat::simple(font_id.clone(), egui::Color32::LIGHT_RED),
        LogLevel::Warning => egui::TextFormat::simple(font_id.clone(), egui::Color32::LIGHT_YELLOW),
        LogLevel::Info => egui::TextFormat::simple(font_id.clone(), egui::Color32::LIGHT_GREEN),
        LogLevel::Text => egui::TextFormat::simple(font_id.clone(), egui::Color32::GRAY),
      ],
    }
  }
}

#[derive(enum_map::Enum)]
enum LogLevel {
  Trace,
  Debug,
  Error,
  Warning,
  Info,
  Text,
}
