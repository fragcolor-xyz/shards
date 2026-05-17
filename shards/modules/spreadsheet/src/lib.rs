/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2026 Fragcolor Pte. Ltd. */

//! shards-spreadsheet
//!
//! Read XLS/XLSX/XLSB/ODS spreadsheets into native Shards data via calamine.
//!
//! Shards exposed:
//! - `Spreadsheet.Load`     : path|bytes  -> Workbook (eagerly parses every sheet)
//! - `Spreadsheet.Sheets`   : Workbook    -> [string]
//! - `Spreadsheet.Read`     : Workbook    -> Seq[Table] (Dense) or Table[Int->Table[Int->Cell]] (Sparse)
//! - `Spreadsheet.ReadAll`  : path|bytes|Workbook -> {sheetName: <as Read>}
//!
//! Cell type mapping:
//!   Empty           -> None / absent (sparse skips it entirely)
//!   String          -> string
//!   Float           -> float
//!   Int             -> int
//!   Bool            -> bool
//!   DateTime, Iso   -> ISO 8601 string
//!   Duration Iso    -> string (passthrough)
//!   Error           -> string ("#REF!", "#VALUE!", etc.)
//!
//! Header dedup (Dense layout): empty header -> "col_<index>",
//!                              duplicate    -> "Foo", "Foo_2", "Foo_3", ...

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use std::collections::{HashMap, HashSet};
use std::convert::TryInto;
use std::io::Cursor;

use calamine::{open_workbook_auto, open_workbook_auto_from_rs, Data, Range, Reader, Sheets};

use shards::core::{register_enum, register_object_type, register_shard};
use shards::shard::Shard;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, Context, ExposedTypes, InstanceData, ParamVar,
  Type, Types, Var, FRAG_CC,
};
use shards::types::SEQ_OF_STRINGS_TYPES;
use shards::{fourCharacterCode, ref_counted_object_type_impl};

// ============================================================================
// Workbook ref-counted object
// ============================================================================
//
// Eager-load model: at Load time we parse every sheet into a HashMap of Range<Data>,
// then drop the underlying file/reader. This:
//   - gives us a single concrete type (no generic-over-reader gymnastics)
//   - matches calamine's actual cost profile (xlsx zip is parsed fully on open)
//   - lets the source File/bytes go out of scope safely

pub struct Workbook {
  sheets: HashMap<String, Range<Data>>,
  /// Per-sheet formula ranges. Each `Range<String>` is sparse: cells with no
  /// formula have empty strings. Used to overlay formula text on top of cached
  /// values when CellSource::Formulas is requested. A sheet may be absent from
  /// this map if the underlying calamine reader can't surface formulas for it
  /// (some ODS configurations); in that case formula mode silently degrades to
  /// values for that sheet.
  formulas: HashMap<String, Range<String>>,
  sheet_order: Vec<String>,
}

impl Workbook {
  fn from_path(path: &str) -> Result<Self, String> {
    let mut wb = open_workbook_auto(path).map_err(|e| format!("Failed to open '{}': {}", path, e))?;
    Self::collect(&mut wb)
  }

  fn from_bytes(bytes: &[u8]) -> Result<Self, String> {
    // calamine's auto reader needs Read+Seek; Cursor over an owned Vec gives us that.
    let cursor = Cursor::new(bytes.to_vec());
    let mut wb = open_workbook_auto_from_rs(cursor)
      .map_err(|e| format!("Failed to parse spreadsheet bytes: {}", e))?;
    Self::collect(&mut wb)
  }

  fn collect<RS: std::io::Read + std::io::Seek>(wb: &mut Sheets<RS>) -> Result<Self, String> {
    let names: Vec<String> = wb.sheet_names().to_vec();
    let mut sheets = HashMap::with_capacity(names.len());
    let mut formulas = HashMap::with_capacity(names.len());
    for name in &names {
      let range = wb
        .worksheet_range(name)
        .map_err(|e| format!("Failed to read sheet '{}': {}", name, e))?;
      sheets.insert(name.clone(), range);
      // Best-effort: formula extraction can fail on some formats (ODS variants).
      // We don't want a missing-formula error to abort the whole load — the
      // common case is "I want values, formulas just for reference".
      if let Ok(frange) = wb.worksheet_formula(name) {
        formulas.insert(name.clone(), frange);
      }
    }
    Ok(Self {
      sheets,
      formulas,
      sheet_order: names,
    })
  }

  fn get_sheet_by_name(&self, name: &str) -> Option<(&Range<Data>, Option<&Range<String>>)> {
    let r = self.sheets.get(name)?;
    Some((r, self.formulas.get(name)))
  }

  fn get_sheet_by_index(
    &self,
    index: usize,
  ) -> Option<(&String, &Range<Data>, Option<&Range<String>>)> {
    let n = self.sheet_order.get(index)?;
    let r = self.sheets.get(n)?;
    Some((n, r, self.formulas.get(n)))
  }
}

ref_counted_object_type_impl!(Workbook);

lazy_static! {
  pub static ref WORKBOOK_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"xWkb"));
  pub static ref WORKBOOK_TYPE_VEC: Vec<Type> = vec![*WORKBOOK_TYPE];
  pub static ref WORKBOOK_VAR_TYPE: Type = Type::context_variable(&WORKBOOK_TYPE_VEC);

  /// Spreadsheet.Load input: a file path string, or in-memory bytes.
  pub static ref PATH_OR_BYTES_TYPES: Vec<Type> = vec![common_type::string, common_type::bytes];

  /// Spreadsheet.ReadAll input: also accepts an already-loaded Workbook.
  pub static ref PATH_BYTES_OR_WORKBOOK_TYPES: Vec<Type> =
    vec![common_type::string, common_type::bytes, *WORKBOOK_TYPE];

  /// Spreadsheet.Read 'Sheet' parameter: by name (string) or index (int).
  pub static ref SHEET_PARAM_TYPES: Vec<Type> = vec![
    common_type::string,
    common_type::int,
    common_type::string_var,
    common_type::int_var,
  ];

  pub static ref ANY_TABLE_TYPES: Vec<Type> = vec![common_type::any_table];
  /// Seq of header-keyed Tables — Dense layout output type.
  /// Defined locally because shards::types only re-exports `SEQ_OF_ANY_TABLE_TYPES`
  /// (the Vec wrapper), not the bare `Type` constant.
  pub static ref SEQ_OF_ANY_TABLE_TYPE: Type = Type::seq(&ANY_TABLE_TYPES);
  /// Spreadsheet.Read advertises both possible output shapes — `compose()` narrows
  /// to either `Seq[Table[Any]]` (Dense, header-keyed rows) or `Table[Any]` (Sparse).
  pub static ref READ_OUTPUT_TYPES: Vec<Type> = vec![*SEQ_OF_ANY_TABLE_TYPE, common_type::any_table];
}

// ============================================================================
// Layout enum (Dense vs Sparse)
// ============================================================================

#[derive(shards::shards_enum, Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
#[enum_info(
  b"sLay",
  "SpreadsheetLayout",
  "How to shape a parsed sheet: dense header-keyed rows for LLM consumption, or sparse Int-keyed grid preserving cell coordinates."
)]
pub enum Layout {
  #[enum_value("Dense: header-keyed Seq of row-Tables. First row is treated as headers (with HasHeaders: true). LLM-friendly shape.")]
  Dense = 0x1,
  #[enum_value("Sparse: Table[row_index -> Table[col_index -> Cell]]. Empty cells are absent. Preserves original coordinates.")]
  Sparse = 0x2,
}

#[derive(shards::shards_enum, Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
#[enum_info(
  b"sCSr",
  "SpreadsheetCellSource",
  "Whether to emit cached values (default) or formula text where present."
)]
pub enum CellSource {
  #[enum_value("Values: emit the cached evaluation of each cell. Standard spreadsheet semantics.")]
  Values = 0x1,
  #[enum_value("Formulas: emit `=<formula>` strings for cells that contain a formula; cells without a formula fall back to their cached value.")]
  Formulas = 0x2,
}

// ============================================================================
// Cell value conversion
// ============================================================================

/// Format an Excel error code into an Excel-style "#NAME?" / "#REF!" / etc. string.
fn error_to_string(e: &calamine::CellErrorType) -> &'static str {
  use calamine::CellErrorType as E;
  match e {
    E::Div0 => "#DIV/0!",
    E::NA => "#N/A",
    E::Name => "#NAME?",
    E::Null => "#NULL!",
    E::Num => "#NUM!",
    E::Ref => "#REF!",
    E::Value => "#VALUE!",
    E::GettingData => "#GETTING_DATA",
  }
}

/// Convert a calamine `Data` cell into an owned String (only used for types
/// that need to allocate — the caller is responsible for pinning the String
/// before producing the corresponding `Var::ephemeral_string`).
fn data_to_owned_string(d: &Data) -> Option<String> {
  match d {
    Data::String(s) => Some(s.clone()),
    Data::DateTime(dt) => {
      // Native ExcelDateTime -> NaiveDateTime -> ISO 8601 (requires calamine `dates` feature).
      dt.as_datetime()
        .map(|ndt| ndt.format("%Y-%m-%dT%H:%M:%S").to_string())
    }
    Data::DateTimeIso(s) => Some(s.clone()),
    Data::DurationIso(s) => Some(s.clone()),
    Data::Error(e) => Some(error_to_string(e).to_string()),
    _ => None,
  }
}

/// Insert a Data cell into an AutoTableVar at the given key, optionally
/// overlaying a formula string. When `formula` is `Some` and non-empty, an
/// `=<formula>` string is inserted instead of the cell value — this is the
/// CellSource::Formulas path. When `formula` is `None` or empty, the cell's
/// cached value is inserted (CellSource::Values).
fn insert_cell_into_table(tbl: &mut AutoTableVar, key: Var, d: &Data, formula: Option<&str>) {
  if let Some(f) = formula {
    if !f.is_empty() {
      let s = format!("={}", f);
      tbl.0.insert_fast(key, &Var::ephemeral_string(&s));
      return;
    }
  }
  match d {
    Data::Empty => {
      // For dense (header-keyed) we explicitly insert None so all rows have all
      // keys. Sparse callers should skip empty cells *before* calling this.
      tbl.0.insert_fast(key, &Var::default());
    }
    Data::Float(f) => {
      tbl.0.insert_fast(key, &Var::from(*f));
    }
    Data::Int(i) => {
      tbl.0.insert_fast(key, &Var::from(*i));
    }
    Data::Bool(b) => {
      tbl.0.insert_fast(key, &Var::from(*b));
    }
    _ => {
      if let Some(s) = data_to_owned_string(d) {
        tbl.0.insert_fast(key, &Var::ephemeral_string(&s));
      } else {
        tbl.0.insert_fast(key, &Var::default());
      }
    }
  }
}

/// Look up the formula string at absolute (row, col) in `formulas` if provided.
/// Returns None if there's no overlay or no formula at that cell.
fn formula_at<'a>(
  formulas: Option<&'a Range<String>>,
  abs_row: u32,
  abs_col: u32,
) -> Option<&'a str> {
  let f = formulas?.get_value((abs_row, abs_col))?;
  if f.is_empty() {
    None
  } else {
    Some(f.as_str())
  }
}

// ============================================================================
// Header dedup
// ============================================================================

/// Build header names from the first row, replacing empties with `col_<idx>`
/// and disambiguating duplicates with `Foo`, `Foo_2`, `Foo_3` ... .
///
/// Collision-safe: a synthesized suffix (e.g. `Foo_2`) is also checked against
/// already-taken names, so a sheet with headers like `["Score", "Score", "Score_2"]`
/// produces `["Score", "Score_2", "Score_3"]` rather than silently colliding on
/// the second `Score_2`.
fn build_headers(first_row: &[Data]) -> Vec<String> {
  let mut taken: HashSet<String> = HashSet::new();
  let mut out: Vec<String> = Vec::with_capacity(first_row.len());

  for (idx, cell) in first_row.iter().enumerate() {
    let raw: String = match cell {
      Data::Empty => String::new(),
      Data::String(s) => s.trim().to_string(),
      Data::Float(f) => f.to_string(),
      Data::Int(i) => i.to_string(),
      Data::Bool(b) => b.to_string(),
      _ => data_to_owned_string(cell).unwrap_or_default(),
    };
    let base = if raw.is_empty() {
      format!("col_{}", idx)
    } else {
      raw
    };

    let final_name = if !taken.contains(&base) {
      base
    } else {
      let mut n = 2usize;
      loop {
        let candidate = format!("{}_{}", base, n);
        if !taken.contains(&candidate) {
          break candidate;
        }
        n += 1;
      }
    };
    taken.insert(final_name.clone());
    out.push(final_name);
  }
  out
}

// ============================================================================
// Range -> Shards data
// ============================================================================

/// Dense: Seq of header-keyed row Tables.
/// Cells in the first row become headers; remaining rows become row-Tables.
/// If has_headers is false, headers default to col_0, col_1, ...
/// When `formulas` is Some and a cell has a formula, the cell's formula text
/// (prefixed with `=`) is emitted instead of its cached value.
fn range_to_dense(
  range: &Range<Data>,
  formulas: Option<&Range<String>>,
  has_headers: bool,
) -> ClonedVar {
  let mut out = AutoSeqVar::new();

  let (row0, col0) = range.start().unwrap_or((0, 0));

  let mut iter = range.rows().enumerate();
  let headers: Vec<String> = if has_headers {
    if let Some((_, first)) = iter.next() {
      build_headers(first)
    } else {
      return out.to_cloned();
    }
  } else {
    // Use widest row to size headers; calamine pads rows so first row width works.
    let width = range.get_size().1;
    (0..width).map(|i| format!("col_{}", i)).collect()
  };

  for (r_off, row) in iter {
    let abs_row = row0 + r_off as u32;
    let mut row_tbl = AutoTableVar::new();
    for (i, cell) in row.iter().enumerate() {
      // The key string must outlive insert_fast (which reads the str pointer
      // when cloning the key into the table). Bind any synthesized String to a
      // named local so its lifetime extends past the call.
      let spillover: String;
      let key = if i < headers.len() {
        Var::ephemeral_string(&headers[i])
      } else {
        // Row wider than header: synthesize spillover key.
        spillover = format!("col_{}", i);
        Var::ephemeral_string(&spillover)
      };
      let abs_col = col0 + i as u32;
      let f = formula_at(formulas, abs_row, abs_col);
      insert_cell_into_table(&mut row_tbl, key, cell, f);
    }
    out.0.emplace_table(row_tbl);
  }

  out.to_cloned()
}

/// Sparse: Table[row_index -> Table[col_index -> Cell]].
/// Empty cells are simply absent. Indices are zero-based and use the
/// range's `start` offset so coordinates match the original spreadsheet.
/// When `formulas` is Some, a cell that *only* has a formula (no cached value)
/// is still included — the formula text serves as the "data".
fn range_to_sparse(range: &Range<Data>, formulas: Option<&Range<String>>) -> ClonedVar {
  let mut out = AutoTableVar::new();

  let (row0, col0) = range.start().unwrap_or((0, 0));

  for (r_off, row) in range.rows().enumerate() {
    let abs_row = row0 + r_off as u32;
    let mut row_tbl = AutoTableVar::new();
    let mut row_has_data = false;
    for (c_off, cell) in row.iter().enumerate() {
      let abs_col = col0 + c_off as u32;
      let f = formula_at(formulas, abs_row, abs_col);
      // A cell is "present" in sparse mode if it has a non-empty cached value
      // OR (in Formulas mode) a formula. Empty cells with no formula are skipped.
      if matches!(cell, Data::Empty) && f.is_none() {
        continue;
      }
      let col_index = abs_col as i64;
      insert_cell_into_table(&mut row_tbl, Var::from(col_index), cell, f);
      row_has_data = true;
    }
    if row_has_data {
      let row_index = abs_row as i64;
      out.0.emplace_table(Var::from(row_index), row_tbl);
    }
  }

  out.to_cloned()
}

/// Dispatch on layout. `formulas` is the per-sheet formula overlay (Some when
/// CellSource::Formulas was requested AND the sheet has a formula range).
fn range_to_var(
  range: &Range<Data>,
  formulas: Option<&Range<String>>,
  layout: Layout,
  has_headers: bool,
) -> ClonedVar {
  match layout {
    Layout::Dense => range_to_dense(range, formulas, has_headers),
    Layout::Sparse => range_to_sparse(range, formulas),
  }
}

// ============================================================================
// Helpers shared by Read / ReadAll / Sheets
// ============================================================================

/// Convert a path|bytes input into an owned Workbook.
fn workbook_from_path_or_bytes_input(input: &Var) -> Result<Workbook, &'static str> {
  if let Ok(path) = TryInto::<&str>::try_into(input) {
    Workbook::from_path(path).map_err(|e| {
      shlog_error!("Spreadsheet load failed: {}", e);
      "Failed to load spreadsheet from path"
    })
  } else if let Ok(bytes) = TryInto::<&[u8]>::try_into(input) {
    Workbook::from_bytes(bytes).map_err(|e| {
      shlog_error!("Spreadsheet load failed: {}", e);
      "Failed to load spreadsheet from bytes"
    })
  } else {
    Err("Spreadsheet input must be a string path or bytes")
  }
}

// ============================================================================
// Spreadsheet.Load
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Spreadsheet.Load",
  "Loads an Excel/ODS workbook from a file path or in-memory bytes. Returns a Workbook object whose sheets are eagerly parsed and cached for subsequent Spreadsheet.Read / Spreadsheet.Sheets calls."
)]
pub struct SpreadsheetLoadShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for SpreadsheetLoadShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SpreadsheetLoadShard {
  fn input_types(&mut self) -> &Types {
    &PATH_OR_BYTES_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &WORKBOOK_TYPE_VEC
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output = ClonedVar::default();
    Ok(())
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(*WORKBOOK_TYPE)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let wb = workbook_from_path_or_bytes_input(input)?;
    self.output = Var::new_ref_counted(wb, &*WORKBOOK_TYPE).into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Spreadsheet.Sheets
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Spreadsheet.Sheets",
  "Returns the sequence of sheet names from a loaded Workbook, in the original order."
)]
pub struct SpreadsheetSheetsShard {
  #[shard_required]
  required: ExposedTypes,

  output: AutoSeqVar,
}

impl Default for SpreadsheetSheetsShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: AutoSeqVar::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SpreadsheetSheetsShard {
  fn input_types(&mut self) -> &Types {
    &WORKBOOK_TYPE_VEC
  }
  fn output_types(&mut self) -> &Types {
    &SEQ_OF_STRINGS_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output.0.clear();
    Ok(())
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let wb = unsafe {
      &*Var::from_ref_counted_object::<Workbook>(input, &*WORKBOOK_TYPE).map_err(|e| {
        shlog_error!("Failed to get Workbook: {}", e);
        e
      })?
    };
    self.output.0.clear();
    for name in &wb.sheet_order {
      self.output.0.push(&Var::ephemeral_string(name));
    }
    Ok(Some(self.output.0 .0))
  }
}

// ============================================================================
// Spreadsheet.Read
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Spreadsheet.Read",
  "Reads one sheet from a Workbook into Shards data. Pick the sheet via the Sheet param (name or zero-based index). Layout selects between dense header-keyed rows (Dense, default) and a sparse Int-keyed grid (Sparse)."
)]
pub struct SpreadsheetReadShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param(
    "Sheet",
    "Sheet to read: name (string) or zero-based index (int). Defaults to index 0.",
    SHEET_PARAM_TYPES
  )]
  sheet: ParamVar,

  #[shard_param(
    "HasHeaders",
    "Dense layout only: treat the first non-empty row as column headers. Default true. Ignored in Sparse.",
    [common_type::bool]
  )]
  has_headers: ClonedVar,

  #[shard_param(
    "Layout",
    "Dense (header-keyed Seq of Tables, LLM-friendly) or Sparse (Int-keyed Table preserving cell coordinates).",
    LAYOUT_TYPES
  )]
  layout: ClonedVar,

  #[shard_param(
    "CellSource",
    "Values (default): emit cached cell evaluations. Formulas: emit `=<formula>` strings where a cell contains a formula, otherwise the cached value. Useful for auditing or for files (e.g. Xero exports) whose cached values are stale.",
    CELLSOURCE_TYPES
  )]
  cell_source: ClonedVar,

  output: ClonedVar,
}

impl Default for SpreadsheetReadShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      sheet: ParamVar::new(0i64.into()),
      has_headers: true.into(),
      layout: Layout::Dense.into(),
      cell_source: CellSource::Values.into(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SpreadsheetReadShard {
  fn input_types(&mut self) -> &Types {
    &WORKBOOK_TYPE_VEC
  }
  fn output_types(&mut self) -> &Types {
    // Output is either Seq[Any] (Dense) or {Any} (Sparse); compose() narrows
    // to the specific shape based on the static Layout param value.
    &READ_OUTPUT_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output = ClonedVar::default();
    Ok(())
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    let layout: Layout = self.layout.0.as_ref().try_into().unwrap_or(Layout::Dense);
    Ok(match layout {
      Layout::Dense => *SEQ_OF_ANY_TABLE_TYPE,  // [{Any}] : seq of header-keyed row tables
      Layout::Sparse => common_type::any_table, // {Any}   : table of int-keyed row tables
    })
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let wb = unsafe {
      &*Var::from_ref_counted_object::<Workbook>(input, &*WORKBOOK_TYPE).map_err(|e| {
        shlog_error!("Failed to get Workbook: {}", e);
        e
      })?
    };

    let sheet_var = self.sheet.get();
    let resolved: Option<(&Range<Data>, Option<&Range<String>>)> =
      if let Ok(name) = TryInto::<&str>::try_into(sheet_var) {
        wb.get_sheet_by_name(name)
      } else if let Ok(idx) = TryInto::<i64>::try_into(sheet_var) {
        if idx < 0 {
          return Err("Sheet index must be non-negative");
        }
        wb.get_sheet_by_index(idx as usize).map(|(_, r, f)| (r, f))
      } else {
        return Err("Sheet param must be a string or int");
      };

    let (range, formulas) = resolved.ok_or_else(|| {
      shlog_error!("Sheet not found in workbook");
      "Sheet not found in workbook"
    })?;

    let has_headers: bool = self.has_headers.0.as_ref().try_into().unwrap_or(true);
    let layout: Layout = self.layout.0.as_ref().try_into().unwrap_or(Layout::Dense);
    let source: CellSource = self.cell_source.0.as_ref().try_into().unwrap_or(CellSource::Values);
    // Only pass the formula overlay through when explicitly requested.
    let formula_overlay = match source {
      CellSource::Values => None,
      CellSource::Formulas => formulas,
    };

    self.output = range_to_var(range, formula_overlay, layout, has_headers);
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Spreadsheet.ReadAll
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Spreadsheet.ReadAll",
  "One-shot: read every sheet of a workbook (path|bytes|Workbook) into a single Table keyed by sheet name. Each sheet's value is shaped by the Layout param (Dense or Sparse), same semantics as Spreadsheet.Read."
)]
pub struct SpreadsheetReadAllShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param(
    "HasHeaders",
    "Dense layout only: treat the first non-empty row of each sheet as column headers. Default true.",
    [common_type::bool]
  )]
  has_headers: ClonedVar,

  #[shard_param(
    "Layout",
    "Dense (header-keyed Seq of Tables) or Sparse (Int-keyed Table preserving cell coordinates).",
    LAYOUT_TYPES
  )]
  layout: ClonedVar,

  #[shard_param(
    "CellSource",
    "Values (default): emit cached cell evaluations. Formulas: emit `=<formula>` strings where present, falling back to cached value.",
    CELLSOURCE_TYPES
  )]
  cell_source: ClonedVar,

  output: ClonedVar,
}

impl Default for SpreadsheetReadAllShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      has_headers: true.into(),
      layout: Layout::Dense.into(),
      cell_source: CellSource::Values.into(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SpreadsheetReadAllShard {
  fn input_types(&mut self) -> &Types {
    &PATH_BYTES_OR_WORKBOOK_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &ANY_TABLE_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output = ClonedVar::default();
    Ok(())
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let has_headers: bool = self.has_headers.0.as_ref().try_into().unwrap_or(true);
    let layout: Layout = self.layout.0.as_ref().try_into().unwrap_or(Layout::Dense);
    let source: CellSource = self.cell_source.0.as_ref().try_into().unwrap_or(CellSource::Values);

    // Resolve to a &Workbook regardless of input type.
    let owned_wb_storage: Option<Workbook>;
    let wb_ref: &Workbook;

    if TryInto::<&str>::try_into(input).is_ok() || TryInto::<&[u8]>::try_into(input).is_ok() {
      owned_wb_storage = Some(workbook_from_path_or_bytes_input(input)?);
      wb_ref = owned_wb_storage.as_ref().unwrap();
    } else {
      owned_wb_storage = None;
      wb_ref = unsafe {
        &*Var::from_ref_counted_object::<Workbook>(input, &*WORKBOOK_TYPE).map_err(|e| {
          shlog_error!("ReadAll input must be path, bytes, or Workbook: {}", e);
          "ReadAll input must be path, bytes, or Workbook"
        })?
      };
    }

    let mut all = AutoTableVar::new();
    for name in &wb_ref.sheet_order {
      let range = match wb_ref.sheets.get(name) {
        Some(r) => r,
        None => continue,
      };
      let formula_overlay = match source {
        CellSource::Values => None,
        CellSource::Formulas => wb_ref.formulas.get(name),
      };
      let key = Var::ephemeral_string(name);
      // Convert sheet to Var (Dense seq or Sparse table) and emplace under sheet name.
      let cv = range_to_var(range, formula_overlay, layout, has_headers);
      all.0.insert_fast(key, &cv.0);
    }

    // Drop the locally-owned workbook (if any) only after `all` no longer refs it.
    drop(owned_wb_storage);

    self.output = all.to_cloned();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Module registration
// ============================================================================

#[no_mangle]
pub extern "C" fn shardsRegister_spreadsheet_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_object_type::<Workbook>(FRAG_CC, fourCharacterCode(*b"xWkb"));
  register_enum::<Layout>();
  register_enum::<CellSource>();

  register_shard::<SpreadsheetLoadShard>();
  register_shard::<SpreadsheetSheetsShard>();
  register_shard::<SpreadsheetReadShard>();
  register_shard::<SpreadsheetReadAllShard>();
}
