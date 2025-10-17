/* SPDX-License-Identifier: BSD-3-Clause AND MIT */
/* Copyright (c) 2022 Fragcolor Pte. Ltd. */
/* Copyright (c) 2018-2021 Emil Ernerfeldt <emil.ernerfeldt@gmail.com> */

// Code partially extracted from egui_demo_lib
// https://github.com/emilk/egui/blob/master/crates/egui_demo_lib/src/syntax_highlighting.rs

use egui::text::LayoutJob;

use core::hash::Hash;
use std::collections::HashMap;
use std::sync::{Arc, LazyLock, Mutex, RwLock};
use std::thread;
use syntect::highlighting::Theme;
use syntect::highlighting::ThemeSet;
use syntect::parsing::{SyntaxDefinition, SyntaxReference, SyntaxSet};

// Cache key for highlighting results
#[derive(Debug, Clone, Hash, PartialEq, Eq)]
struct HighlightCacheKey {
  theme_name: String,
  dark_mode: bool,
  code: String,
  language: String,
}

impl HighlightCacheKey {
  fn new(theme: &CodeTheme, code: &str, language: &str) -> Self {
    Self {
      theme_name: theme
        .theme
        .name
        .as_ref()
        .map(|s| s.clone())
        .unwrap_or_else(|| "default".to_string()),
      dark_mode: theme.dark_mode,
      code: code.to_string(),
      language: language.to_string(),
    }
  }
}

/// Memoized Code highlighting for Shards language
pub(crate) fn highlight_shards(theme: &CodeTheme, code: &str) -> LayoutJob {
  highlight_generic(theme, code, "shards")
}

/// Memoized Code highlighting for any language
pub(crate) fn highlight_generic(theme: &CodeTheme, code: &str, language: &str) -> LayoutJob {
  let cache_key = HighlightCacheKey::new(theme, code, language);

  // Check static cache first
  if let Some(cached) = HighlightCache::get(&cache_key) {
    return cached;
  }

  // Try to highlight with async highlighter
  let result = if let Some(highlighter) = AsyncHighlighterCache::get_highlighter() {
    highlighter.highlight(theme, code, language)
  } else {
    // Return unhighlighted text while highlighter loads (never cache this!)
    return create_unhighlighted_layout(theme, code);
  };

  // Cache the result
  HighlightCache::insert(cache_key, result.clone());
  result
}

/*
base16-ocean.dark,base16-eighties.dark,base16-mocha.dark,base16-ocean.light
InspiredGitHub from here
Solarized (dark) and Solarized (light)
*/
lazy_static! {
  static ref DEFAULT_THEMES: ThemeSet = ThemeSet::load_defaults();
  static ref DARK_THEME: &'static Theme = &DEFAULT_THEMES.themes["base16-ocean.dark"];
  static ref LIGHT_THEME: &'static Theme = &DEFAULT_THEMES.themes["base16-ocean.light"];
}

/// Create a simple unhighlighted layout for text
fn create_unhighlighted_layout(theme: &CodeTheme, text: &str) -> LayoutJob {
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
}

// Static caches
static HIGHLIGHTER: LazyLock<Arc<Mutex<Option<Arc<Highlighter>>>>> =
  LazyLock::new(|| Arc::new(Mutex::new(None)));
static HIGHLIGHTER_LOADING: LazyLock<Arc<Mutex<bool>>> =
  LazyLock::new(|| Arc::new(Mutex::new(false)));
static HIGHLIGHT_CACHE: LazyLock<Arc<RwLock<HashMap<HighlightCacheKey, LayoutJob>>>> =
  LazyLock::new(|| Arc::new(RwLock::new(HashMap::new())));

struct HighlightCache;

impl HighlightCache {
  fn get(key: &HighlightCacheKey) -> Option<LayoutJob> {
    HIGHLIGHT_CACHE.read().ok()?.get(key).cloned()
  }

  fn insert(key: HighlightCacheKey, value: LayoutJob) {
    if let Ok(mut cache) = HIGHLIGHT_CACHE.write() {
      // Limit cache size to prevent memory leaks
      if cache.len() > 1000 {
        cache.clear();
      }
      cache.insert(key, value);
    }
  }
}

struct AsyncHighlighterCache;

impl AsyncHighlighterCache {
  fn get_highlighter() -> Option<Arc<Highlighter>> {
    // Check if already loaded
    if let Ok(guard) = HIGHLIGHTER.lock() {
      if let Some(highlighter) = guard.as_ref() {
        return Some(Arc::clone(highlighter));
      }
    }

    // Check if loading is in progress
    if let Ok(mut loading_guard) = HIGHLIGHTER_LOADING.lock() {
      if !*loading_guard {
        *loading_guard = true;

        // Spawn background thread to load highlighter
        let cache = HIGHLIGHTER.clone();
        let loading_flag = HIGHLIGHTER_LOADING.clone();

        thread::spawn(move || {
          let highlighter = Arc::new(Highlighter::default());

          if let Ok(mut guard) = cache.lock() {
            *guard = Some(highlighter);
          }

          if let Ok(mut loading_guard) = loading_flag.lock() {
            *loading_guard = false;
          }
        });
      }
    }

    None
  }
}

pub(crate) struct CodeTheme {
  dark_mode: bool,
  pub theme: &'static Theme,
}

impl Hash for CodeTheme {
  fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
    self.dark_mode.hash(state);
    self.theme.name.hash(state);
  }
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
      theme: &DARK_THEME,
    }
  }

  pub fn light() -> Self {
    Self {
      dark_mode: false,
      theme: &LIGHT_THEME,
    }
  }
}

#[derive(Clone)]
struct Highlighter {
  syntaxes: SyntaxSet,
  syntax_cache: Arc<RwLock<HashMap<String, Option<usize>>>>, // Cache syntax indices by language
}

impl Default for Highlighter {
  fn default() -> Self {
    let mut builder = SyntaxSet::load_defaults_newlines().into_builder();
    builder.add(
      SyntaxDefinition::load_from_str(include_str!("sublime-syntax.yml"), true, None).unwrap(),
    );
    let syntaxes = builder.build();

    Highlighter {
      syntaxes,
      syntax_cache: Arc::new(RwLock::new(HashMap::new())),
    }
  }
}

impl Highlighter {
  fn get_cached_syntax(&self, language: &str) -> Option<&SyntaxReference> {
    // Check cache first
    if let Ok(cache) = self.syntax_cache.read() {
      if let Some(&Some(index)) = cache.get(language) {
        return self.syntaxes.syntaxes().get(index);
      } else if cache.contains_key(language) {
        // We've already tried this language and it doesn't exist
        return None;
      }
    }

    // Not in cache, look it up
    let syntax = self
      .syntaxes
      .find_syntax_by_name(language)
      .or_else(|| self.syntaxes.find_syntax_by_extension(language));

    // Cache the result
    if let Ok(mut cache) = self.syntax_cache.write() {
      if let Some(syntax_ref) = syntax {
        // Find the index of this syntax in the syntaxes vector
        if let Some(index) = self
          .syntaxes
          .syntaxes()
          .iter()
          .position(|s| std::ptr::eq(s, syntax_ref))
        {
          cache.insert(language.to_string(), Some(index));
        }
      } else {
        cache.insert(language.to_string(), None);
      }
    }

    syntax
  }

  fn highlight(&self, theme: &CodeTheme, text: &str, language: &str) -> LayoutJob {
    self
      .highlight_impl(theme, text, language)
      .unwrap_or_else(|| create_unhighlighted_layout(theme, text))
  }

  fn highlight_impl(&self, theme: &CodeTheme, text: &str, language: &str) -> Option<LayoutJob> {
    use syntect::easy::HighlightLines;
    use syntect::highlighting::FontStyle;
    use syntect::util::LinesWithEndings;

    let syntax = self.get_cached_syntax(language)?;

    let mut h = HighlightLines::new(syntax, theme.theme);

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

fn as_byte_range(whole: &str, range: &str) -> std::ops::Range<usize> {
  let whole_start = whole.as_ptr() as usize;
  let range_start = range.as_ptr() as usize;
  assert!(whole_start <= range_start);
  assert!(range_start + range.len() <= whole_start + whole.len());
  let offset = range_start - whole_start;
  offset..(offset + range.len())
}
