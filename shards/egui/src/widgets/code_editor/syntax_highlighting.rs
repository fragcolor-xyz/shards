/* SPDX-License-Identifier: BSD-3-Clause AND MIT */
/* Copyright (c) 2022 Fragcolor Pte. Ltd. */
/* Copyright (c) 2018-2021 Emil Ernerfeldt <emil.ernerfeldt@gmail.com> */

// Code partially extracted from egui_demo_lib
// https://github.com/emilk/egui/blob/master/crates/egui_demo_lib/src/syntax_highlighting.rs

use egui::text::LayoutJob;

use syntect::highlighting::ThemeSet;
use syntect::parsing::SyntaxDefinition;
use syntect::parsing::SyntaxSet;

impl egui::util::cache::ComputerMut<(&CodeTheme, &str, &str), LayoutJob> for Highlighter<true> {
  fn compute(&mut self, (theme, code, language): (&CodeTheme, &str, &str)) -> LayoutJob {
    self.highlight(theme, code, language)
  }
}

impl egui::util::cache::ComputerMut<(&CodeTheme, &str), LayoutJob> for Highlighter<false> {
  fn compute(&mut self, (theme, code): (&CodeTheme, &str)) -> LayoutJob {
    self.highlight(theme, code, "Shards")
  }
}

/// Memoized Code highlighting
pub(crate) fn highlight_shards(ctx: &egui::Context, theme: &CodeTheme, code: &str) -> LayoutJob {
  type HighlightCache<'a> = egui::util::cache::FrameCache<LayoutJob, Highlighter<false>>;

  ctx.memory_mut(|mem| {
    let highlight_cache = mem.caches.cache::<HighlightCache<'_>>();
    highlight_cache.get((theme, code))
  })
}

/// Memoized Code highlighting
pub(crate) fn highlight_generic(
  ctx: &egui::Context,
  theme: &CodeTheme,
  code: &str,
  language: &str,
) -> LayoutJob {
  type HighlightCache<'a> = egui::util::cache::FrameCache<LayoutJob, Highlighter<true>>;

  ctx.memory_mut(|mem| {
    let highlight_cache = mem.caches.cache::<HighlightCache<'_>>();
    highlight_cache.get((theme, code, language))
  })
}

#[derive(Hash)]
pub(crate) struct CodeTheme {
  dark_mode: bool,
  syntect_theme: SyntectTheme,
}

impl Default for CodeTheme {
  fn default() -> Self {
    Self::dark()
  }
}

impl CodeTheme {
  pub fn dark() -> Self {
    Self {
      dark_mode: true,
      syntect_theme: SyntectTheme::SolarizedDark,
    }
  }

  pub fn light() -> Self {
    Self {
      dark_mode: false,
      syntect_theme: SyntectTheme::SolarizedLight,
    }
  }
}

struct Highlighter<const FULL_LOAD: bool> {
  syntaxes: SyntaxSet,
  themes: ThemeSet,
}

impl<const FULL_LOAD: bool> Default for Highlighter<FULL_LOAD> {
  fn default() -> Self {
    let syntaxes = if FULL_LOAD {
      let mut builder = SyntaxSet::load_defaults_newlines().into_builder();
      builder.add(
        SyntaxDefinition::load_from_str(include_str!("sublime-syntax.yml"), true, None).unwrap(),
      );
      builder.build()
    } else {
      let mut builder = SyntaxSet::new().into_builder();
      builder.add(
        SyntaxDefinition::load_from_str(include_str!("sublime-syntax.yml"), true, None).unwrap(),
      );
      builder.build()
    };

    Highlighter {
      syntaxes,
      themes: ThemeSet::load_defaults(),
    }
  }
}

impl<const FULL_LOAD: bool> Highlighter<FULL_LOAD> {
  fn highlight(&self, theme: &CodeTheme, text: &str, language: &str) -> LayoutJob {
    self
      .highlight_impl(theme, text, language)
      .unwrap_or_else(|| {
        LayoutJob::simple(
          text.into(),
          egui::FontId::monospace(12.0),
          if theme.dark_mode {
            egui::Color32::LIGHT_GRAY
          } else {
            egui::Color32::DARK_GRAY
          },
          f32::INFINITY,
        )
      })
  }

  fn highlight_impl(&self, theme: &CodeTheme, text: &str, language: &str) -> Option<LayoutJob> {
    use syntect::easy::HighlightLines;
    use syntect::highlighting::FontStyle;
    use syntect::util::LinesWithEndings;

    let syntax = self
      .syntaxes
      .find_syntax_by_name(language)
      .or_else(|| self.syntaxes.find_syntax_by_extension(language))?;

    let theme = theme.syntect_theme.syntect_key_name();
    let mut h = HighlightLines::new(syntax, &self.themes.themes[theme]);

    use egui::text::{LayoutSection, TextFormat};
    let mut job = LayoutJob {
      text: text.into(),
      ..Default::default()
    };

    for line in LinesWithEndings::from(text) {
      for (style, range) in h.highlight_line(line, &self.syntaxes).ok()? {
        let fg = style.foreground;
        let text_color = egui::Color32::from_rgb(fg.r, fg.g, fg.b);
        let italics = style.font_style.contains(FontStyle::ITALIC);
        let underline = style.font_style.contains(FontStyle::ITALIC);
        let underline = if underline {
          egui::Stroke::new(1.0, text_color)
        } else {
          egui::Stroke::NONE
        };

        job.sections.push(LayoutSection {
          leading_space: 0.0,
          byte_range: as_byte_range(text, range),
          format: TextFormat {
            font_id: egui::FontId::monospace(12.0),
            color: text_color,
            italics,
            underline,
            ..Default::default()
          },
        });
      }
    }

    Some(job)
  }
}

#[derive(Hash)]
enum SyntectTheme {
  Base16EightiesDark,
  Base16MochaDark,
  Base16OceanDark,
  Base16OceanLight,
  InspiredGitHub,
  SolarizedDark,
  SolarizedLight,
}

impl SyntectTheme {
  fn syntect_key_name(&self) -> &'static str {
    match self {
      Self::Base16EightiesDark => "base16-eighties.dark",
      Self::Base16MochaDark => "base16-mocha.dark",
      Self::Base16OceanDark => "base16-ocean.dark",
      Self::Base16OceanLight => "base16-ocean.light",
      Self::InspiredGitHub => "InspiredGitHub",
      Self::SolarizedDark => "Solarized (dark)",
      Self::SolarizedLight => "Solarized (light)",
    }
  }
}

fn as_byte_range(whole: &str, range: &str) -> std::ops::Range<usize> {
  let whole_start = whole.as_ptr() as usize;
  let range_start = range.as_ptr() as usize;
  assert!(whole_start <= range_start);
  assert!(range_start + range.len() <= whole_start + whole.len());
  let offset = range_start - whole_start;
  offset..(offset + range.len())
}
