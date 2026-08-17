/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

//! Geospatial operations for drone mission planning.
//!
//! Provides shards for:
//! - `Geo.Polygon` - Creates a polygon from coordinate sequence
//! - `Geo.Buffer` - Expands or contracts a polygon by a distance in meters
//! - `Geo.GridFill` - Fills a polygon with grid points in boustrophedon pattern
//! - `Geo.FilterVisible` - Filters waypoints by camera visibility within a polygon
//! - `Geo.PathLength` - Calculates total path length in meters (for flight time estimation)
//! - `Geo.SimplifyPath` - Removes intermediate points, keeping only turn waypoints
//! - `Geo.ToDjiKmz` - Exports waypoints to DJI Fly compatible KMZ files
//! - `Geo.ToGoogleEarth` - Exports waypoints to Google Earth KML
//! - `Geo.ToGeoJSON` - Exports waypoints to GeoJSON format
//! - `Geo.ToLitchiCSV` - Exports waypoints to Litchi CSV format

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use geo::{AffineOps, AffineTransform, Area, BoundingRect, Centroid, Contains, Coord, MultiPolygon, Point, Polygon};
use geo_buffer::buffer_polygon;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, Context, ExposedTypes, InstanceData, ParamVar,
  Seq, Type, Types, Var, SEQ_OF_SEQ_OF_FLOAT_TYPES,
};
use std::io::Write;
use std::time::{SystemTime, UNIX_EPOCH};
use zip::write::SimpleFileOptions;
use zip::ZipWriter;

// ============================================================================
// Constants
// ============================================================================

const METERS_PER_DEG_LAT: f64 = 111_319.9;
const EARTH_RADIUS_METERS: f64 = 6_371_000.0;

// ============================================================================
// Type Definitions
// ============================================================================

lazy_static! {
  // Centroid table: {x: Float, y: Float}
  static ref CENTROID_KEYS: Vec<Var> = vec![
    shstr!("x").into(),
    shstr!("y").into(),
  ];
  static ref CENTROID_VALUE_TYPES: Vec<Type> = vec![
    common_type::float,
    common_type::float,
  ];
  static ref CENTROID_TABLE_TYPE: Type = Type::table(&CENTROID_KEYS, &CENTROID_VALUE_TYPES);

  // Polygon table: {type: String, coords: [[Float]], centroid: {x: Float, y: Float}}
  static ref POLYGON_KEYS: Vec<Var> = vec![
    shstr!("type").into(),
    shstr!("coords").into(),
    shstr!("centroid").into(),
  ];
  static ref POLYGON_VALUE_TYPES: Vec<Type> = vec![
    common_type::string,
    SEQ_OF_SEQ_OF_FLOAT_TYPES[0],  // [[Float]]
    *CENTROID_TABLE_TYPE,
  ];
  static ref POLYGON_TABLE_TYPE: Type = Type::table(&POLYGON_KEYS, &POLYGON_VALUE_TYPES);
  static ref POLYGON_OUTPUT_TYPES: Vec<Type> = vec![*POLYGON_TABLE_TYPE];

  // Grid point table: {x: Float, y: Float, index: Int, row: Int}
  static ref GRID_POINT_KEYS: Vec<Var> = vec![
    shstr!("x").into(),
    shstr!("y").into(),
    shstr!("index").into(),
    shstr!("row").into(),
  ];
  static ref GRID_POINT_VALUE_TYPES: Vec<Type> = vec![
    common_type::float,
    common_type::float,
    common_type::int,
    common_type::int,
  ];
  static ref GRID_POINT_TABLE_TYPE: Type = Type::table(&GRID_POINT_KEYS, &GRID_POINT_VALUE_TYPES);
  static ref GRID_POINT_TABLE_TYPES: Vec<Type> = vec![*GRID_POINT_TABLE_TYPE];

  // GridFill output: sequence of grid point tables
  static ref SEQ_OF_GRID_POINTS: Type = Type::seq(&GRID_POINT_TABLE_TYPES);
  static ref SEQ_OF_GRID_POINTS_TYPES: Vec<Type> = vec![*SEQ_OF_GRID_POINTS];

  // Parameter types
  static ref FLOAT_OR_VAR_TYPES: Vec<Type> = vec![common_type::float, common_type::float_var, common_type::none];
  static ref STRING_VAR_OR_NONE_TYPES: Vec<Type> = vec![common_type::string, common_type::string_var, common_type::none];

  // Polygon parameter types (for accepting polygon as parameter)
  static ref POLYGON_VAR_TYPE: Type = Type::context_variable(&POLYGON_OUTPUT_TYPES);
  static ref POLYGON_VAR_TYPES: Vec<Type> = vec![*POLYGON_TABLE_TYPE, *POLYGON_VAR_TYPE];

  // Waypoint input types - accepts any sequence of tables
  static ref SEQ_OF_ANY_TABLE: Type = Type::seq(&[common_type::any_table]);
  static ref SEQ_OF_ANY_TABLE_TYPES: Vec<Type> = vec![*SEQ_OF_ANY_TABLE];

  // Float output types
  static ref FLOAT_TYPES: Vec<Type> = vec![common_type::float];
}

// ============================================================================
// Helper Functions
// ============================================================================

/// Maximum latitude for equirectangular projection (breaks near poles)
const MAX_LATITUDE: f64 = 85.0;

/// Equirectangular projection: convert lon/lat to meters relative to origin.
///
/// This uses a fixed latitude (centroid) for longitude scaling, which introduces
/// ~0.8% error per degree of latitude span. For typical drone missions (<1km),
/// this error is negligible (<10m). For larger areas, consider using UTM projection.
fn lonlat_to_meters(lon: f64, lat: f64, origin_lon: f64, origin_lat: f64) -> (f64, f64) {
  let meters_per_deg_lon = METERS_PER_DEG_LAT * origin_lat.to_radians().cos();
  (
    (lon - origin_lon) * meters_per_deg_lon,
    (lat - origin_lat) * METERS_PER_DEG_LAT,
  )
}

/// Inverse equirectangular projection: convert meters back to lon/lat
///
/// Note: origin_lat must be within ±85° (validated by input coordinate checks).
/// At exactly ±90° (poles), cos(lat) = 0 which would cause division by zero.
fn meters_to_lonlat(x: f64, y: f64, origin_lon: f64, origin_lat: f64) -> (f64, f64) {
  let cos_lat = origin_lat.to_radians().cos();
  // Safety: prevent division by zero at poles (should never happen with validated input)
  let meters_per_deg_lon = if cos_lat.abs() < 1e-10 {
    1.0 // Fallback to avoid NaN/Inf - caller should validate latitude
  } else {
    METERS_PER_DEG_LAT * cos_lat
  };
  (origin_lon + x / meters_per_deg_lon, origin_lat + y / METERS_PER_DEG_LAT)
}

/// Validate latitude is within acceptable range for equirectangular projection
fn validate_latitude(lat: f64) -> Result<(), &'static str> {
  if lat.abs() > MAX_LATITUDE {
    return Err("Latitude must be between -85 and 85 degrees for equirectangular projection");
  }
  Ok(())
}

/// Validate longitude range and check for antimeridian crossing
fn validate_coordinates(coords: &[(f64, f64)]) -> Result<(), &'static str> {
  if coords.is_empty() {
    return Ok(());
  }

  for &(lon, lat) in coords {
    // Validate longitude range
    if lon < -180.0 || lon > 180.0 {
      return Err("Longitude must be between -180 and 180 degrees");
    }
    // Validate latitude for equirectangular projection
    validate_latitude(lat)?;
  }

  // Check for antimeridian crossing by examining consecutive edges
  // An edge crosses the antimeridian if the longitude difference > 180°
  for i in 0..coords.len() {
    let (lon1, _) = coords[i];
    let (lon2, _) = coords[(i + 1) % coords.len()];
    if (lon2 - lon1).abs() > 180.0 {
      return Err("Polygon edge crosses antimeridian (±180°), which is not supported");
    }
  }

  Ok(())
}

/// Calculate haversine distance between two lon/lat points in meters
fn haversine_distance(lon1: f64, lat1: f64, lon2: f64, lat2: f64) -> f64 {
  let lat1_rad = lat1.to_radians();
  let lat2_rad = lat2.to_radians();
  let delta_lat = (lat2 - lat1).to_radians();
  let delta_lon = (lon2 - lon1).to_radians();

  let a = (delta_lat / 2.0).sin().powi(2) + lat1_rad.cos() * lat2_rad.cos() * (delta_lon / 2.0).sin().powi(2);
  let c = 2.0 * a.sqrt().asin();

  EARTH_RADIUS_METERS * c
}

/// Parse a sequence of [lon, lat] sequences into Vec of (lon, lat) tuples
/// Also validates all coordinates for longitude range, latitude range, and antimeridian crossing
fn parse_coords(input: &Var) -> Result<Vec<(f64, f64)>, &'static str> {
  let seq: Seq = input.try_into().map_err(|_| "Expected sequence of coordinates")?;
  let mut coords = Vec::with_capacity(seq.len());

  for item in seq.iter() {
    let coord_seq: Seq = item.try_into().map_err(|_| "Expected [lon, lat] sequence")?;
    if coord_seq.len() < 2 {
      return Err("Coordinate must have at least 2 elements [lon, lat]");
    }
    let lon: f64 = (&coord_seq[0]).try_into().map_err(|_| "Invalid longitude")?;
    let lat: f64 = (&coord_seq[1]).try_into().map_err(|_| "Invalid latitude")?;
    coords.push((lon, lat));
  }

  // Validate all coordinates (range, latitude limits, antimeridian)
  validate_coordinates(&coords)?;

  Ok(coords)
}

/// Build a geo::Polygon from coordinates (closes polygon if needed)
fn build_geo_polygon(coords: &[(f64, f64)]) -> Polygon<f64> {
  let mut exterior: Vec<Coord<f64>> = coords.iter().map(|&(x, y)| Coord { x, y }).collect();

  // Close polygon if not already closed
  // Use 1e-8 degrees (~1mm) as epsilon for geographic coordinates
  if let (Some(first), Some(last)) = (exterior.first(), exterior.last()) {
    if (first.x - last.x).abs() > 1e-8 || (first.y - last.y).abs() > 1e-8 {
      exterior.push(*first);
    }
  }

  Polygon::new(geo::LineString::new(exterior), vec![])
}

// ============================================================================
// Geo.Polygon Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info("Geo.Polygon", "Creates a polygon table from a sequence of [lon, lat] coordinates")]
struct PolygonShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for PolygonShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for PolygonShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_SEQ_OF_FLOAT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &POLYGON_OUTPUT_TYPES
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
    // Parse input coordinates
    let coords = parse_coords(input)?;
    if coords.len() < 3 {
      return Err("Polygon must have at least 3 coordinates");
    }

    // Build geo polygon and compute centroid
    let geo_poly = build_geo_polygon(&coords);
    let centroid = geo_poly.centroid().ok_or("Failed to compute centroid")?;

    // Build output table
    let mut table = AutoTableVar::new();

    // type: "polygon"
    table.0.insert_fast_static("type", &Var::ephemeral_string("polygon"));

    // coords: sequence of [lon, lat]
    let mut coords_seq = AutoSeqVar::new();
    for &(lon, lat) in &coords {
      let mut coord_seq = AutoSeqVar::new();
      coord_seq.0.push(&Var::from(lon));
      coord_seq.0.push(&Var::from(lat));
      coords_seq.0.emplace_seq(coord_seq);
    }
    table.0.emplace_seq(Var::ephemeral_string("coords"), coords_seq);

    // centroid: {x, y}
    let mut centroid_table = AutoTableVar::new();
    centroid_table.0.insert_fast_static("x", &Var::from(centroid.x()));
    centroid_table.0.insert_fast_static("y", &Var::from(centroid.y()));
    table.0.emplace_table(Var::ephemeral_string("centroid"), centroid_table);

    self.output = table.to_cloned();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Geo.Buffer Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Geo.Buffer",
  "Expands or contracts a polygon by a specified distance in meters"
)]
struct BufferShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Distance", "Buffer distance in meters (positive=expand, negative=contract)", FLOAT_OR_VAR_TYPES)]
  distance: ParamVar,

  output: ClonedVar,
}

impl Default for BufferShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      distance: ParamVar::new(0.0.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for BufferShard {
  fn input_types(&mut self) -> &Types {
    &POLYGON_OUTPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &POLYGON_OUTPUT_TYPES
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
    let distance: f64 = self.distance.get().try_into().map_err(|_| "Invalid Distance")?;

    if distance == 0.0 {
      // No buffer, return input as-is
      return Ok(Some(*input));
    }

    // Parse input polygon table
    let table = input.as_table().map_err(|_| "Expected polygon table")?;

    // Get coords from table
    let coords_var = table.get_static("coords").ok_or("Missing 'coords' in polygon")?;
    let coords = parse_coords(coords_var)?;
    if coords.len() < 3 {
      return Err("Polygon must have at least 3 coordinates");
    }

    // Get centroid for projection
    let centroid_table = table.get_static("centroid").ok_or("Missing 'centroid' in polygon")?;
    let centroid_tbl = centroid_table.as_table().map_err(|_| "Invalid centroid")?;
    let origin_lon: f64 = centroid_tbl
      .get_static("x")
      .ok_or("Missing centroid x")?
      .try_into()
      .map_err(|_| "Invalid centroid x")?;
    let origin_lat: f64 = centroid_tbl
      .get_static("y")
      .ok_or("Missing centroid y")?
      .try_into()
      .map_err(|_| "Invalid centroid y")?;

    // Convert to meters for buffering
    let coords_meters: Vec<Coord<f64>> = coords
      .iter()
      .map(|&(lon, lat)| {
        let (x, y) = lonlat_to_meters(lon, lat, origin_lon, origin_lat);
        Coord { x, y }
      })
      .collect();

    // Build polygon in meters and buffer it
    let poly_meters = Polygon::new(geo::LineString::new(coords_meters), vec![]);
    let buffered: MultiPolygon<f64> = buffer_polygon(&poly_meters, distance);

    // Get the largest polygon from the result (buffer can create multiple)
    let largest_poly = buffered
      .iter()
      .max_by(|a, b| {
        let area_a = a.unsigned_area();
        let area_b = b.unsigned_area();
        area_a.partial_cmp(&area_b).unwrap_or(std::cmp::Ordering::Equal)
      })
      .ok_or("Buffer operation produced no polygons")?;

    // Convert back to lon/lat
    let buffered_coords: Vec<(f64, f64)> = largest_poly
      .exterior()
      .coords()
      .map(|c| meters_to_lonlat(c.x, c.y, origin_lon, origin_lat))
      .collect();

    // Remove closing point if present (will be added by build_geo_polygon if needed)
    let buffered_coords: Vec<(f64, f64)> = if buffered_coords.len() > 1 {
      let first = buffered_coords.first().unwrap();
      let last = buffered_coords.last().unwrap();
      if (first.0 - last.0).abs() < 1e-8 && (first.1 - last.1).abs() < 1e-8 {
        buffered_coords[..buffered_coords.len() - 1].to_vec()
      } else {
        buffered_coords
      }
    } else {
      buffered_coords
    };

    // Build output polygon table
    let geo_poly = build_geo_polygon(&buffered_coords);
    let new_centroid = geo_poly.centroid().ok_or("Failed to compute centroid")?;

    let mut out_table = AutoTableVar::new();
    out_table.0.insert_fast_static("type", &Var::ephemeral_string("polygon"));

    let mut coords_seq = AutoSeqVar::new();
    for &(lon, lat) in &buffered_coords {
      let mut coord_seq = AutoSeqVar::new();
      coord_seq.0.push(&Var::from(lon));
      coord_seq.0.push(&Var::from(lat));
      coords_seq.0.emplace_seq(coord_seq);
    }
    out_table.0.emplace_seq(Var::ephemeral_string("coords"), coords_seq);

    let mut centroid_out = AutoTableVar::new();
    centroid_out.0.insert_fast_static("x", &Var::from(new_centroid.x()));
    centroid_out.0.insert_fast_static("y", &Var::from(new_centroid.y()));
    out_table.0.emplace_table(Var::ephemeral_string("centroid"), centroid_out);

    self.output = out_table.to_cloned();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Geo.GridFill Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Geo.GridFill",
  "Fills a polygon with grid points in boustrophedon (lawnmower) pattern"
)]
struct GridFillShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("SpacingX", "Grid spacing in X direction (meters)", FLOAT_OR_VAR_TYPES)]
  spacing_x: ParamVar,

  #[shard_param("SpacingY", "Grid spacing in Y direction (meters)", FLOAT_OR_VAR_TYPES)]
  spacing_y: ParamVar,

  #[shard_param("Direction", "Rotation angle in degrees (default 0)", FLOAT_OR_VAR_TYPES)]
  direction: ParamVar,

  output: ClonedVar,
}

impl Default for GridFillShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      spacing_x: ParamVar::new(10.0.into()),
      spacing_y: ParamVar::new(10.0.into()),
      direction: ParamVar::new(0.0.into()),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for GridFillShard {
  fn input_types(&mut self) -> &Types {
    &POLYGON_OUTPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
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
    // Get parameters
    let spacing_x: f64 = self.spacing_x.get().try_into().map_err(|_| "Invalid SpacingX")?;
    let spacing_y: f64 = self.spacing_y.get().try_into().map_err(|_| "Invalid SpacingY")?;
    let direction: f64 = self.direction.get().try_into().unwrap_or(0.0);

    if spacing_x <= 0.0 || spacing_y <= 0.0 {
      return Err("Spacing must be positive");
    }

    // Parse input polygon table
    let table = input.as_table().map_err(|_| "Expected polygon table")?;

    // Get coords from table
    let coords_var = table.get_static("coords").ok_or("Missing 'coords' in polygon")?;
    let coords = parse_coords(coords_var)?;
    if coords.len() < 3 {
      return Err("Polygon must have at least 3 coordinates");
    }

    // Get centroid
    let centroid_table = table.get_static("centroid").ok_or("Missing 'centroid' in polygon")?;
    let centroid_tbl = centroid_table.as_table().map_err(|_| "Invalid centroid")?;
    let origin_lon: f64 = centroid_tbl
      .get_static("x")
      .ok_or("Missing centroid x")?
      .try_into()
      .map_err(|_| "Invalid centroid x")?;
    let origin_lat: f64 = centroid_tbl
      .get_static("y")
      .ok_or("Missing centroid y")?
      .try_into()
      .map_err(|_| "Invalid centroid y")?;

    // Note: All coordinates are already validated in parse_coords() including latitude

    // Convert coords to meters (equirectangular projection)
    let coords_meters: Vec<(f64, f64)> = coords
      .iter()
      .map(|&(lon, lat)| lonlat_to_meters(lon, lat, origin_lon, origin_lat))
      .collect();

    // Build polygon in meters
    let poly_meters = build_geo_polygon(&coords_meters);

    // Rotate polygon by -direction (so grid aligns with direction)
    // Note: AffineTransform::rotate expects radians
    let direction_rad = direction.to_radians();
    let rotation = AffineTransform::rotate(-direction_rad, Coord { x: 0.0, y: 0.0 });
    let rotated_poly = poly_meters.affine_transform(&rotation);

    // Get bounding box of rotated polygon
    let bbox = rotated_poly.bounding_rect().ok_or("Failed to compute bounding box")?;

    // Generate candidate grid points over the bounding box.
    // Uses integer-based iteration to avoid floating-point accumulation errors.
    // With floor(), points are generated from bbox.min at spacing intervals.
    // The last point is at bbox.min + floor(span/spacing) * spacing, which may be
    // slightly inside bbox.max. Points are then filtered by polygon containment.
    let mut rows: Vec<Vec<(f64, f64)>> = Vec::new();
    let y_steps = ((bbox.max().y - bbox.min().y) / spacing_y).floor() as usize;
    for i in 0..=y_steps {
      let y = bbox.min().y + (i as f64) * spacing_y;
      let mut row: Vec<(f64, f64)> = Vec::new();
      let x_steps = ((bbox.max().x - bbox.min().x) / spacing_x).floor() as usize;
      for j in 0..=x_steps {
        let x = bbox.min().x + (j as f64) * spacing_x;
        let point = Point::new(x, y);
        if rotated_poly.contains(&point) {
          row.push((x, y));
        }
      }
      if !row.is_empty() {
        rows.push(row);
      }
    }

    // Build output with boustrophedon ordering
    let mut output_seq = AutoSeqVar::new();
    let mut global_index: i64 = 0;

    // Inverse rotation to transform back
    let inv_rotation = AffineTransform::rotate(direction_rad, Coord { x: 0.0, y: 0.0 });

    for (row_idx, row) in rows.iter().enumerate() {
      // Reverse odd rows for boustrophedon pattern
      let row_iter: Box<dyn Iterator<Item = &(f64, f64)>> = if row_idx % 2 == 1 {
        Box::new(row.iter().rev())
      } else {
        Box::new(row.iter())
      };

      for &(x, y) in row_iter {
        // Rotate point back
        let rotated_point = Point::new(x, y).affine_transform(&inv_rotation);

        // Convert back to lon/lat
        let (lon, lat) = meters_to_lonlat(rotated_point.x(), rotated_point.y(), origin_lon, origin_lat);

        // Build output table for this point
        let mut point_table = AutoTableVar::new();
        point_table.0.insert_fast_static("x", &Var::from(lon));
        point_table.0.insert_fast_static("y", &Var::from(lat));
        point_table.0.insert_fast_static("index", &Var::from(global_index));
        point_table.0.insert_fast_static("row", &Var::from(row_idx as i64));

        output_seq.0.emplace_table(point_table);
        global_index += 1;
      }
    }

    self.output = output_seq.to_cloned();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Geo.ToDjiKmz Shard
// ============================================================================

const MAX_DJI_WAYPOINTS: usize = 200;

/// Waypoint data extracted from input table
#[derive(Clone)]
struct Waypoint {
  lon: f64,
  lat: f64,
  index: i64,
  altitude: f64,
  speed: f64,
  gimbal_pitch: f64,
  heading: f64,
  take_photo: bool,
}

#[derive(shards::shard)]
#[shard_info(
  "Geo.ToDjiKmz",
  "Exports waypoints to DJI Fly compatible KMZ file for drone missions"
)]
struct ToDjiKmzShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Path", "Output file path for the KMZ file", STRING_VAR_OR_NONE_TYPES)]
  path: ParamVar,

  #[shard_param("FinishAction", "Action after mission: goHome, noAction, or autoLand", STRING_VAR_OR_NONE_TYPES)]
  finish_action: ParamVar,

  #[shard_param("Speed", "Default flight speed in m/s", FLOAT_OR_VAR_TYPES)]
  speed: ParamVar,

  #[shard_param("Altitude", "Default altitude in meters (relative to takeoff)", FLOAT_OR_VAR_TYPES)]
  altitude: ParamVar,

  #[shard_param("GimbalPitch", "Default gimbal pitch angle (-90 to 0)", FLOAT_OR_VAR_TYPES)]
  gimbal_pitch: ParamVar,

  #[shard_param("DroneEnum", "DJI drone enum value (default 68)", FLOAT_OR_VAR_TYPES)]
  drone_enum: ParamVar,

  #[shard_param("DroneSubEnum", "DJI drone sub-enum value (default 0)", FLOAT_OR_VAR_TYPES)]
  drone_sub_enum: ParamVar,

  #[shard_param("TurnDampingDist", "Turn damping distance in meters for smooth curves (default 2.0, 0 may cause load errors)", FLOAT_OR_VAR_TYPES)]
  turn_damping_dist: ParamVar,

  #[shard_param("PhotoSpacing", "Insert a takePhoto waypoint every N meters along the path (0 = no photo actions, default 0). Consumer DJI Fly has no interval-trigger action, so photos are explicit densified waypoints.", FLOAT_OR_VAR_TYPES)]
  photo_spacing: ParamVar,

  #[shard_param("TakeOffSecurityHeight", "Safety climb height in meters before flying to first waypoint (default 20)", FLOAT_OR_VAR_TYPES)]
  takeoff_security_height: ParamVar,

  #[shard_param("PayloadEnumValue", "DJI payload enum value (default 68, community value for consumer drones)", FLOAT_OR_VAR_TYPES)]
  payload_enum: ParamVar,

  #[shard_param("MaxWaypoints", "Max waypoints per KMZ file; larger missions are split into name_1ofN.kmz files (default 150, hard cap 200)", FLOAT_OR_VAR_TYPES)]
  max_waypoints: ParamVar,

  output: ClonedVar,
}

impl Default for ToDjiKmzShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      path: ParamVar::default(),
      finish_action: ParamVar::new(Var::ephemeral_string("goHome")),
      speed: ParamVar::new(5.0.into()),
      altitude: ParamVar::new(30.0.into()),
      gimbal_pitch: ParamVar::new((-45.0).into()),
      drone_enum: ParamVar::new(68.0.into()),
      drone_sub_enum: ParamVar::new(0.0.into()),
      turn_damping_dist: ParamVar::new(2.0.into()),
      photo_spacing: ParamVar::new(0.0.into()),
      takeoff_security_height: ParamVar::new(20.0.into()),
      payload_enum: ParamVar::new(68.0.into()),
      max_waypoints: ParamVar::new(150.0.into()),
      output: ClonedVar::default(),
    }
  }
}

impl ToDjiKmzShard {
  fn get_timestamp_ms() -> u64 {
    SystemTime::now()
      .duration_since(UNIX_EPOCH)
      .map(|d| d.as_millis() as u64)
      .unwrap_or(0)
  }

  fn generate_mission_config(
    &self,
    finish_action: &str,
    speed: f64,
    drone_enum: i64,
    drone_sub_enum: i64,
    payload_enum: i64,
    takeoff_security_height: f64,
  ) -> String {
    format!(
      r#"    <wpml:missionConfig>
      <wpml:flyToWaylineMode>safely</wpml:flyToWaylineMode>
      <wpml:finishAction>{}</wpml:finishAction>
      <wpml:exitOnRCLost>executeLostAction</wpml:exitOnRCLost>
      <wpml:executeRCLostAction>goBack</wpml:executeRCLostAction>
      <wpml:takeOffSecurityHeight>{:.1}</wpml:takeOffSecurityHeight>
      <wpml:globalTransitionalSpeed>{:.1}</wpml:globalTransitionalSpeed>
      <wpml:droneInfo>
        <wpml:droneEnumValue>{}</wpml:droneEnumValue>
        <wpml:droneSubEnumValue>{}</wpml:droneSubEnumValue>
      </wpml:droneInfo>
      <wpml:payloadInfo>
        <wpml:payloadEnumValue>{}</wpml:payloadEnumValue>
        <wpml:payloadSubEnumValue>0</wpml:payloadSubEnumValue>
        <wpml:payloadPositionIndex>0</wpml:payloadPositionIndex>
      </wpml:payloadInfo>
    </wpml:missionConfig>"#,
      finish_action, takeoff_security_height, speed, drone_enum, drone_sub_enum, payload_enum
    )
  }

  fn generate_template_kml(
    &self,
    waypoints: &[Waypoint],
    finish_action: &str,
    speed: f64,
    timestamp: u64,
    drone_enum: i64,
    drone_sub_enum: i64,
    payload_enum: i64,
    takeoff_security_height: f64,
  ) -> String {
    let mut placemarks = String::new();
    for (i, wp) in waypoints.iter().enumerate() {
      placemarks.push_str(&format!(
        r#"      <Placemark>
        <Point>
          <coordinates>{:.15},{:.15}</coordinates>
        </Point>
        <wpml:index>{}</wpml:index>
        <wpml:height>{}</wpml:height>
        <wpml:useGlobalHeight>0</wpml:useGlobalHeight>
        <wpml:gimbalPitchAngle>{}</wpml:gimbalPitchAngle>
      </Placemark>
"#,
        wp.lon,
        wp.lat,
        i,
        wp.altitude,
        wp.gimbal_pitch.round() as i64
      ));
    }

    format!(
      r#"<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2" xmlns:wpml="http://www.dji.com/wpmz/1.0.2">
  <Document>
    <wpml:author>Shards</wpml:author>
    <wpml:createTime>{}</wpml:createTime>
    <wpml:updateTime>{}</wpml:updateTime>
{}
    <Folder>
      <wpml:templateType>waypoint</wpml:templateType>
      <wpml:templateId>0</wpml:templateId>
      <wpml:waylineCoordinateSysParam>
        <wpml:coordinateMode>WGS84</wpml:coordinateMode>
        <wpml:heightMode>relativeToStartPoint</wpml:heightMode>
      </wpml:waylineCoordinateSysParam>
      <wpml:autoFlightSpeed>{:.1}</wpml:autoFlightSpeed>
{}    </Folder>
  </Document>
</kml>
"#,
      timestamp,
      timestamp,
      self.generate_mission_config(
        finish_action,
        speed,
        drone_enum,
        drone_sub_enum,
        payload_enum,
        takeoff_security_height
      ),
      speed,
      placemarks
    )
  }

  /// Expand the path so a takePhoto waypoint appears every `spacing` meters of
  /// arc length. Consumer DJI Fly's WPML dialect has no interval-trigger
  /// action (multipleTiming/multipleDistance are enterprise-only), so photo
  /// positions must be explicit waypoints with a takePhoto action. The path is
  /// walked as one continuous polyline so spacing carries across control
  /// points instead of restarting at each one.
  fn densify_photo_waypoints(waypoints: &[Waypoint], spacing: f64) -> Vec<Waypoint> {
    if waypoints.len() < 2 {
      return waypoints.to_vec();
    }
    let meters_per_deg_lon = METERS_PER_DEG_LAT * waypoints[0].lat.to_radians().cos();
    let to_meters = |wp: &Waypoint| (wp.lon * meters_per_deg_lon, wp.lat * METERS_PER_DEG_LAT);

    let mut out = Vec::with_capacity(waypoints.len() * 2);
    let mut walked = 0.0;
    let mut next_photo_at = spacing;
    for i in 0..waypoints.len() {
      out.push(waypoints[i].clone());
      if i + 1 == waypoints.len() {
        break;
      }
      let (ax, ay) = to_meters(&waypoints[i]);
      let (bx, by) = to_meters(&waypoints[i + 1]);
      let seg_len = ((bx - ax).powi(2) + (by - ay).powi(2)).sqrt();
      if seg_len > 0.0 {
        while next_photo_at > walked && next_photo_at <= walked + seg_len {
          let t = (next_photo_at - walked) / seg_len;
          let a = &waypoints[i];
          let b = &waypoints[i + 1];
          let mut wp = a.clone();
          wp.lon = a.lon + (b.lon - a.lon) * t;
          wp.lat = a.lat + (b.lat - a.lat) * t;
          wp.altitude = a.altitude + (b.altitude - a.altitude) * t;
          wp.take_photo = true;
          out.push(wp);
          next_photo_at += spacing;
        }
      }
      walked += seg_len;
    }
    out
  }

  fn generate_waylines_wpml(
    &self,
    waypoints: &[Waypoint],
    finish_action: &str,
    speed: f64,
    drone_enum: i64,
    drone_sub_enum: i64,
    payload_enum: i64,
    takeoff_security_height: f64,
    _turn_damping_dist: f64,
  ) -> String {
    let mut placemarks = String::new();
    let last_idx = waypoints.len().saturating_sub(1);

    for (i, wp) in waypoints.iter().enumerate() {
      // First waypoint stops, others pass through smoothly
      let turn_mode = if i == 0 {
        "toPointAndStopWithContinuityCurvature"
      } else {
        "toPointAndPassWithContinuityCurvature"
      };

      // Build the second action group for gimbal transition (all except last waypoint)
      let second_action_group = if i < last_idx {
        format!(
          r#"
        <wpml:actionGroup>
          <wpml:actionGroupId>{}</wpml:actionGroupId>
          <wpml:actionGroupStartIndex>{}</wpml:actionGroupStartIndex>
          <wpml:actionGroupEndIndex>{}</wpml:actionGroupEndIndex>
          <wpml:actionGroupMode>parallel</wpml:actionGroupMode>
          <wpml:actionTrigger>
            <wpml:actionTriggerType>betweenAdjacentPoints</wpml:actionTriggerType>
          </wpml:actionTrigger>
          <wpml:action>
            <wpml:actionId>0</wpml:actionId>
            <wpml:actionActuatorFunc>gimbalEvenlyRotate</wpml:actionActuatorFunc>
            <wpml:actionActuatorFuncParam>
              <wpml:gimbalPitchRotateAngle>{}</wpml:gimbalPitchRotateAngle>
              <wpml:payloadPositionIndex>0</wpml:payloadPositionIndex>
            </wpml:actionActuatorFuncParam>
          </wpml:action>
        </wpml:actionGroup>"#,
          i * 2 + 1, // actionGroupId: 1, 3, 5, ...
          i,         // startIndex
          i + 1,     // endIndex (next waypoint)
          wp.gimbal_pitch.round() as i64
        )
      } else {
        String::new()
      };

      // takePhoto fired at this waypoint (photo waypoints from densification)
      let take_photo_action = if wp.take_photo {
        format!(
          r#"
          <wpml:action>
            <wpml:actionId>1</wpml:actionId>
            <wpml:actionActuatorFunc>takePhoto</wpml:actionActuatorFunc>
            <wpml:actionActuatorFuncParam>
              <wpml:fileSuffix>wp{}</wpml:fileSuffix>
              <wpml:payloadPositionIndex>0</wpml:payloadPositionIndex>
            </wpml:actionActuatorFuncParam>
          </wpml:action>"#,
          i
        )
      } else {
        String::new()
      };
      // gimbal must settle before the shutter fires -> sequence when shooting
      let reach_group_mode = if wp.take_photo { "sequence" } else { "parallel" };

      placemarks.push_str(&format!(
        r#"      <Placemark>
        <Point>
          <coordinates>{:.15},{:.15}</coordinates>
        </Point>
        <wpml:index>{}</wpml:index>
        <wpml:executeHeight>{}</wpml:executeHeight>
        <wpml:waypointSpeed>{:.1}</wpml:waypointSpeed>
        <wpml:waypointHeadingParam>
          <wpml:waypointHeadingMode>smoothTransition</wpml:waypointHeadingMode>
          <wpml:waypointHeadingAngle>{}</wpml:waypointHeadingAngle>
          <wpml:waypointPoiPoint>0,0,0.000000</wpml:waypointPoiPoint>
          <wpml:waypointHeadingAngleEnable>1</wpml:waypointHeadingAngleEnable>
          <wpml:waypointHeadingPathMode>followBadArc</wpml:waypointHeadingPathMode>
        </wpml:waypointHeadingParam>
        <wpml:waypointTurnParam>
          <wpml:waypointTurnMode>{}</wpml:waypointTurnMode>
          <wpml:waypointTurnDampingDist>0</wpml:waypointTurnDampingDist>
        </wpml:waypointTurnParam>
        <wpml:useStraightLine>0</wpml:useStraightLine>
        <wpml:actionGroup>
          <wpml:actionGroupId>{}</wpml:actionGroupId>
          <wpml:actionGroupStartIndex>{}</wpml:actionGroupStartIndex>
          <wpml:actionGroupEndIndex>{}</wpml:actionGroupEndIndex>
          <wpml:actionGroupMode>{}</wpml:actionGroupMode>
          <wpml:actionTrigger>
            <wpml:actionTriggerType>reachPoint</wpml:actionTriggerType>
          </wpml:actionTrigger>
          <wpml:action>
            <wpml:actionId>0</wpml:actionId>
            <wpml:actionActuatorFunc>gimbalRotate</wpml:actionActuatorFunc>
            <wpml:actionActuatorFuncParam>
              <wpml:gimbalHeadingYawBase>aircraft</wpml:gimbalHeadingYawBase>
              <wpml:gimbalRotateMode>absoluteAngle</wpml:gimbalRotateMode>
              <wpml:gimbalPitchRotateEnable>1</wpml:gimbalPitchRotateEnable>
              <wpml:gimbalPitchRotateAngle>{}</wpml:gimbalPitchRotateAngle>
              <wpml:gimbalRollRotateEnable>0</wpml:gimbalRollRotateEnable>
              <wpml:gimbalRollRotateAngle>0</wpml:gimbalRollRotateAngle>
              <wpml:gimbalYawRotateEnable>0</wpml:gimbalYawRotateEnable>
              <wpml:gimbalYawRotateAngle>0</wpml:gimbalYawRotateAngle>
              <wpml:gimbalRotateTimeEnable>0</wpml:gimbalRotateTimeEnable>
              <wpml:gimbalRotateTime>0</wpml:gimbalRotateTime>
              <wpml:payloadPositionIndex>0</wpml:payloadPositionIndex>
            </wpml:actionActuatorFuncParam>
          </wpml:action>{}
        </wpml:actionGroup>{}
      </Placemark>
"#,
        wp.lon,
        wp.lat,
        i,
        wp.altitude,
        wp.speed,
        wp.heading.round() as i64,
        turn_mode,
        i * 2, // actionGroupId: 0, 2, 4, ...
        i,
        i,
        reach_group_mode,
        wp.gimbal_pitch.round() as i64,
        take_photo_action,
        second_action_group
      ));
    }

    format!(
      r#"<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2" xmlns:wpml="http://www.dji.com/wpmz/1.0.2">
  <Document>
{}
    <Folder>
      <wpml:templateId>0</wpml:templateId>
      <wpml:executeHeightMode>relativeToStartPoint</wpml:executeHeightMode>
      <wpml:waylineId>0</wpml:waylineId>
      <wpml:distance>0</wpml:distance>
      <wpml:duration>0</wpml:duration>
      <wpml:autoFlightSpeed>{:.1}</wpml:autoFlightSpeed>
{}    </Folder>
  </Document>
</kml>
"#,
      self.generate_mission_config(
        finish_action,
        speed,
        drone_enum,
        drone_sub_enum,
        payload_enum,
        takeoff_security_height
      ),
      speed,
      placemarks
    )
  }
}

#[shards::shard_impl]
impl Shard for ToDjiKmzShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
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
    // Passthrough - output same type as input
    Ok(data.inputType)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    // Get parameters (defaults are already set in struct, unwrap_or handles invalid values)
    let path: &str = self.path.get().try_into().map_err(|_| "Invalid Path parameter")?;

    let finish_action: &str = self.finish_action.get().try_into().unwrap_or("goHome");
    let finish_action = match finish_action {
      "goHome" | "noAction" | "autoLand" => finish_action,
      _ => return Err("FinishAction must be 'goHome', 'noAction', or 'autoLand'"),
    };

    let default_speed: f64 = self.speed.get().try_into().unwrap_or(5.0);
    let default_altitude: f64 = self.altitude.get().try_into().unwrap_or(30.0);
    let default_gimbal_pitch: f64 = self.gimbal_pitch.get().try_into().unwrap_or(-45.0);
    if default_gimbal_pitch < -90.0 || default_gimbal_pitch > 0.0 {
      return Err("GimbalPitch must be between -90 and 0 degrees");
    }
    let drone_enum: i64 = self.drone_enum.get().try_into().unwrap_or(68.0) as i64;
    let drone_sub_enum: i64 = self.drone_sub_enum.get().try_into().unwrap_or(0.0) as i64;
    let turn_damping_dist: f64 = self.turn_damping_dist.get().try_into().unwrap_or(2.0);

    let photo_spacing: f64 = self.photo_spacing.get().try_into().unwrap_or(0.0);
    if photo_spacing < 0.0 || (photo_spacing > 0.0 && photo_spacing < 1.0) {
      return Err("PhotoSpacing must be 0 (off) or at least 1 meter");
    }
    let takeoff_security_height: f64 = self.takeoff_security_height.get().try_into().unwrap_or(20.0);
    if takeoff_security_height < 1.2 || takeoff_security_height > 1500.0 {
      return Err("TakeOffSecurityHeight must be between 1.2 and 1500 meters");
    }
    let payload_enum: i64 = self.payload_enum.get().try_into().unwrap_or(68.0) as i64;
    let max_waypoints: usize = {
      let v: f64 = self.max_waypoints.get().try_into().unwrap_or(150.0);
      let v = v as usize;
      if v < 2 || v > MAX_DJI_WAYPOINTS {
        return Err("MaxWaypoints must be between 2 and 200 (DJI Fly hard cap)");
      }
      v
    };

    // Parse input waypoints
    let seq: Seq = input.try_into().map_err(|_| "Expected sequence of waypoints")?;

    if seq.is_empty() {
      return Err("No waypoints provided");
    }

    let mut waypoints = Vec::with_capacity(seq.len());
    for item in seq.iter() {
      let table = item.as_table().map_err(|_| "Expected waypoint table")?;

      // Required fields
      let lon: f64 = table
        .get_static("x")
        .ok_or("Missing 'x' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'x' value")?;
      let lat: f64 = table
        .get_static("y")
        .ok_or("Missing 'y' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'y' value")?;
      let index: i64 = table
        .get_static("index")
        .ok_or("Missing 'index' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'index' value")?;

      // Optional fields with defaults
      let altitude: f64 = table
        .get_static("altitude")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_altitude);
      let speed: f64 = table
        .get_static("speed")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_speed);
      // Validate gimbal pitch is in valid range, fall back to default if invalid
      let gimbal_pitch: f64 = table
        .get_static("gimbal_pitch")
        .and_then(|v| {
          let val: f64 = v.try_into().ok()?;
          if val >= -90.0 && val <= 0.0 {
            Some(val)
          } else {
            None
          }
        })
        .unwrap_or(default_gimbal_pitch);
      let heading: f64 = table
        .get_static("heading")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(0.0);

      waypoints.push(Waypoint {
        lon,
        lat,
        index,
        altitude,
        speed,
        gimbal_pitch,
        heading,
        take_photo: false,
      });
    }

    // Expand photo positions into explicit takePhoto waypoints (consumer DJI
    // Fly has no interval-trigger action)
    let waypoints = if photo_spacing > 0.0 {
      Self::densify_photo_waypoints(&waypoints, photo_spacing)
    } else {
      waypoints
    };

    // Split into files of at most max_waypoints (DJI Fly caps missions at 200
    // waypoints and real RCs degrade well below that)
    let n_files = (waypoints.len() + max_waypoints - 1) / max_waypoints;
    if n_files > 10 {
      return Err("Mission too large: would split into more than 10 KMZ files; increase PhotoSpacing or reduce waypoints");
    }

    let timestamp = Self::get_timestamp_ms();
    let base = path.strip_suffix(".kmz").unwrap_or(path);

    for (file_idx, chunk) in waypoints.chunks(max_waypoints).enumerate() {
      let file_path = if n_files == 1 {
        format!("{}.kmz", base)
      } else {
        format!("{}_{}of{}.kmz", base, file_idx + 1, n_files)
      };

      let template_kml = self.generate_template_kml(
        chunk,
        finish_action,
        default_speed,
        timestamp,
        drone_enum,
        drone_sub_enum,
        payload_enum,
        takeoff_security_height,
      );
      let waylines_wpml = self.generate_waylines_wpml(
        chunk,
        finish_action,
        default_speed,
        drone_enum,
        drone_sub_enum,
        payload_enum,
        takeoff_security_height,
        turn_damping_dist,
      );

      // Create KMZ file (ZIP with wpmz/ folder)
      let file = std::fs::File::create(&file_path).map_err(|_| "Failed to create KMZ file")?;
      let mut zip = ZipWriter::new(file);
      let options = SimpleFileOptions::default();

      zip
        .start_file("wpmz/template.kml", options)
        .map_err(|_| "Failed to create template.kml in KMZ")?;
      zip
        .write_all(template_kml.as_bytes())
        .map_err(|_| "Failed to write template.kml")?;

      zip
        .start_file("wpmz/waylines.wpml", options)
        .map_err(|_| "Failed to create waylines.wpml in KMZ")?;
      zip
        .write_all(waylines_wpml.as_bytes())
        .map_err(|_| "Failed to write waylines.wpml")?;

      zip.finish().map_err(|_| "Failed to finalize KMZ file")?;
    }

    // Passthrough input
    Ok(Some(*input))
  }
}

// ============================================================================
// Geo.ToGoogleEarth Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Geo.ToGoogleEarth",
  "Exports waypoints to Google Earth KML file for mission preview"
)]
struct ToGoogleEarthShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Path", "Output file path for the KML file", STRING_VAR_OR_NONE_TYPES)]
  path: ParamVar,

  #[shard_param("Altitude", "Default altitude in meters (relative to ground)", FLOAT_OR_VAR_TYPES)]
  altitude: ParamVar,

  output: ClonedVar,
}

impl Default for ToGoogleEarthShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      path: ParamVar::default(),
      altitude: ParamVar::new(30.0.into()),
      output: ClonedVar::default(),
    }
  }
}

impl ToGoogleEarthShard {
  fn generate_kml(&self, waypoints: &[Waypoint]) -> String {
    // Build placemarks for each waypoint
    let mut placemarks = String::new();
    for wp in waypoints {
      placemarks.push_str(&format!(
        r#"    <Placemark>
      <name>WP {}</name>
      <description>Altitude: {:.1}m, Speed: {:.1}m/s</description>
      <Style>
        <IconStyle>
          <Icon>
            <href>http://maps.google.com/mapfiles/kml/paddle/ylw-circle.png</href>
          </Icon>
        </IconStyle>
      </Style>
      <Point>
        <altitudeMode>relativeToGround</altitudeMode>
        <coordinates>{:.8},{:.8},{:.1}</coordinates>
      </Point>
    </Placemark>
"#,
        wp.index, wp.altitude, wp.speed, wp.lon, wp.lat, wp.altitude
      ));
    }

    // Build LineString for flight path
    let mut path_coords = String::new();
    for wp in waypoints {
      path_coords.push_str(&format!("{:.8},{:.8},{:.1}\n        ", wp.lon, wp.lat, wp.altitude));
    }

    format!(
      r#"<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2">
  <Document>
    <name>Mission Preview</name>
    <description>Generated by Shards Geo module</description>
    <Style id="flightPath">
      <LineStyle>
        <color>ff0000ff</color>
        <width>3</width>
      </LineStyle>
    </Style>
    <Placemark>
      <name>Flight Path</name>
      <styleUrl>#flightPath</styleUrl>
      <LineString>
        <altitudeMode>relativeToGround</altitudeMode>
        <coordinates>
        {}
        </coordinates>
      </LineString>
    </Placemark>
{}  </Document>
</kml>
"#,
      path_coords.trim(),
      placemarks
    )
  }
}

#[shards::shard_impl]
impl Shard for ToGoogleEarthShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
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
    Ok(data.inputType)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let path: &str = self.path.get().try_into().map_err(|_| "Invalid Path parameter")?;
    let default_altitude: f64 = self.altitude.get().try_into().unwrap_or(30.0);

    // Parse input waypoints
    let seq: Seq = input.try_into().map_err(|_| "Expected sequence of waypoints")?;

    if seq.is_empty() {
      return Err("No waypoints provided");
    }

    let mut waypoints = Vec::with_capacity(seq.len());
    for item in seq.iter() {
      let table = item.as_table().map_err(|_| "Expected waypoint table")?;

      let lon: f64 = table
        .get_static("x")
        .ok_or("Missing 'x' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'x' value")?;
      let lat: f64 = table
        .get_static("y")
        .ok_or("Missing 'y' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'y' value")?;
      let index: i64 = table
        .get_static("index")
        .ok_or("Missing 'index' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'index' value")?;

      let altitude: f64 = table
        .get_static("altitude")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_altitude);
      let speed: f64 = table
        .get_static("speed")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(5.0);

      waypoints.push(Waypoint {
        lon,
        lat,
        index,
        altitude,
        speed,
        gimbal_pitch: 0.0,
        heading: 0.0,
        take_photo: false,
      });
    }

    // Generate KML content
    let kml = self.generate_kml(&waypoints);

    // Write to file
    std::fs::write(path, kml).map_err(|_| "Failed to write KML file")?;

    // Passthrough input
    Ok(Some(*input))
  }
}

// ============================================================================
// Geo.ToGeoJSON Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Geo.ToGeoJSON",
  "Exports waypoints to GeoJSON format for use with GIS tools"
)]
struct ToGeoJsonShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Path", "Output file path for the GeoJSON file", STRING_VAR_OR_NONE_TYPES)]
  path: ParamVar,

  #[shard_param("Altitude", "Default altitude in meters", FLOAT_OR_VAR_TYPES)]
  altitude: ParamVar,

  output: ClonedVar,
}

impl Default for ToGeoJsonShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      path: ParamVar::default(),
      altitude: ParamVar::new(30.0.into()),
      output: ClonedVar::default(),
    }
  }
}

impl ToGeoJsonShard {
  fn generate_geojson(&self, waypoints: &[Waypoint]) -> String {
    // Build point features for each waypoint
    let mut features = String::new();
    for (i, wp) in waypoints.iter().enumerate() {
      if i > 0 {
        features.push_str(",\n");
      }
      features.push_str(&format!(
        r#"    {{
      "type": "Feature",
      "geometry": {{
        "type": "Point",
        "coordinates": [{:.8}, {:.8}, {:.1}]
      }},
      "properties": {{
        "index": {},
        "altitude": {:.1},
        "speed": {:.1}
      }}
    }}"#,
        wp.lon, wp.lat, wp.altitude, wp.index, wp.altitude, wp.speed
      ));
    }

    // Build LineString for flight path
    let mut path_coords = String::new();
    for (i, wp) in waypoints.iter().enumerate() {
      if i > 0 {
        path_coords.push_str(", ");
      }
      path_coords.push_str(&format!("[{:.8}, {:.8}, {:.1}]", wp.lon, wp.lat, wp.altitude));
    }

    format!(
      r#"{{
  "type": "FeatureCollection",
  "features": [
    {{
      "type": "Feature",
      "geometry": {{
        "type": "LineString",
        "coordinates": [{}]
      }},
      "properties": {{
        "name": "Flight Path"
      }}
    }},
{}
  ]
}}
"#,
      path_coords, features
    )
  }
}

#[shards::shard_impl]
impl Shard for ToGeoJsonShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
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
    Ok(data.inputType)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let path: &str = self.path.get().try_into().map_err(|_| "Invalid Path parameter")?;
    let default_altitude: f64 = self.altitude.get().try_into().unwrap_or(30.0);

    let seq: Seq = input.try_into().map_err(|_| "Expected sequence of waypoints")?;

    if seq.is_empty() {
      return Err("No waypoints provided");
    }

    let mut waypoints = Vec::with_capacity(seq.len());
    for item in seq.iter() {
      let table = item.as_table().map_err(|_| "Expected waypoint table")?;

      let lon: f64 = table
        .get_static("x")
        .ok_or("Missing 'x' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'x' value")?;
      let lat: f64 = table
        .get_static("y")
        .ok_or("Missing 'y' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'y' value")?;
      let index: i64 = table
        .get_static("index")
        .ok_or("Missing 'index' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'index' value")?;

      let altitude: f64 = table
        .get_static("altitude")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_altitude);
      let speed: f64 = table
        .get_static("speed")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(5.0);

      waypoints.push(Waypoint {
        lon,
        lat,
        index,
        altitude,
        speed,
        gimbal_pitch: 0.0,
        heading: 0.0,
        take_photo: false,
      });
    }

    let geojson = self.generate_geojson(&waypoints);
    std::fs::write(path, geojson).map_err(|_| "Failed to write GeoJSON file")?;

    Ok(Some(*input))
  }
}

// ============================================================================
// Geo.ToLitchiCSV Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Geo.ToLitchiCSV",
  "Exports waypoints to Litchi CSV format for drone missions"
)]
struct ToLitchiCsvShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Path", "Output file path for the CSV file", STRING_VAR_OR_NONE_TYPES)]
  path: ParamVar,

  #[shard_param("Altitude", "Default altitude in meters", FLOAT_OR_VAR_TYPES)]
  altitude: ParamVar,

  #[shard_param("Speed", "Default speed in m/s (0 = default)", FLOAT_OR_VAR_TYPES)]
  speed: ParamVar,

  #[shard_param("GimbalPitch", "Gimbal pitch angle (-90 to 0)", FLOAT_OR_VAR_TYPES)]
  gimbal_pitch: ParamVar,

  #[shard_param("Heading", "Default heading in degrees", FLOAT_OR_VAR_TYPES)]
  heading: ParamVar,

  #[shard_param("CurveSize", "Curve size for smooth turns (meters)", FLOAT_OR_VAR_TYPES)]
  curve_size: ParamVar,

  output: ClonedVar,
}

impl Default for ToLitchiCsvShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      path: ParamVar::default(),
      altitude: ParamVar::new(30.0.into()),
      speed: ParamVar::new(0.0.into()),
      gimbal_pitch: ParamVar::new((-45.0).into()),
      heading: ParamVar::new(0.0.into()),
      curve_size: ParamVar::new(0.0.into()),
      output: ClonedVar::default(),
    }
  }
}

impl ToLitchiCsvShard {
  fn generate_csv(&self, waypoints: &[Waypoint], curve_size: f64) -> String {
    let mut csv = String::from("latitude,longitude,altitude(m),heading(deg),curvesize(m),rotationdir,gimbalmode,gimbalpitchangle,actiontype1,actionparam1,altitudemode,speed(m/s),poi_latitude,poi_longitude,poi_altitude(m),poi_altitudemode,photo_timeinterval,photo_distinterval\n");

    for wp in waypoints {
      // Use waypoint values directly - defaults already applied during parsing
      csv.push_str(&format!(
        "{:.15},{:.15},{:.0},{:.0},{:.1},0,2,{:.0},5,{:.0},0,{:.1},0,0,-1,0,-1,-1\n",
        wp.lat,
        wp.lon,
        wp.altitude,
        wp.heading.round(),
        curve_size,
        wp.gimbal_pitch.round(),
        wp.gimbal_pitch.round(),
        wp.speed
      ));
    }

    csv
  }
}

#[shards::shard_impl]
impl Shard for ToLitchiCsvShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
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
    Ok(data.inputType)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let path: &str = self.path.get().try_into().map_err(|_| "Invalid Path parameter")?;
    let default_altitude: f64 = self.altitude.get().try_into().unwrap_or(30.0);
    let default_speed: f64 = self.speed.get().try_into().unwrap_or(0.0);
    let default_gimbal: f64 = self.gimbal_pitch.get().try_into().unwrap_or(-45.0);
    let default_heading: f64 = self.heading.get().try_into().unwrap_or(0.0);
    let curve_size: f64 = self.curve_size.get().try_into().unwrap_or(0.0);

    let seq: Seq = input.try_into().map_err(|_| "Expected sequence of waypoints")?;

    if seq.is_empty() {
      return Err("No waypoints provided");
    }

    let mut waypoints = Vec::with_capacity(seq.len());
    for item in seq.iter() {
      let table = item.as_table().map_err(|_| "Expected waypoint table")?;

      let lon: f64 = table
        .get_static("x")
        .ok_or("Missing 'x' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'x' value")?;
      let lat: f64 = table
        .get_static("y")
        .ok_or("Missing 'y' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'y' value")?;
      let index: i64 = table
        .get_static("index")
        .ok_or("Missing 'index' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'index' value")?;

      let altitude: f64 = table
        .get_static("altitude")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_altitude);
      let speed: f64 = table
        .get_static("speed")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_speed);
      let gimbal_pitch: f64 = table
        .get_static("gimbal_pitch")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_gimbal);
      let heading: f64 = table
        .get_static("heading")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_heading);

      waypoints.push(Waypoint {
        lon,
        lat,
        index,
        altitude,
        speed,
        gimbal_pitch,
        heading,
        take_photo: false,
      });
    }

    let csv = self.generate_csv(&waypoints, curve_size);
    std::fs::write(path, csv).map_err(|_| "Failed to write CSV file")?;

    Ok(Some(*input))
  }
}

// ============================================================================
// Geo.FilterVisible Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Geo.FilterVisible",
  "Filters waypoints to keep only those where the camera's look point falls inside a polygon"
)]
struct FilterVisibleShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Polygon", "Reference polygon to check visibility against", POLYGON_VAR_TYPES)]
  polygon: ParamVar,

  #[shard_param("Altitude", "Default altitude in meters (used if waypoint has no altitude)", FLOAT_OR_VAR_TYPES)]
  altitude: ParamVar,

  #[shard_param("GimbalPitch", "Gimbal pitch angle in degrees (-90 to 0, negative = looking down)", FLOAT_OR_VAR_TYPES)]
  gimbal_pitch: ParamVar,

  output: ClonedVar,
}

impl Default for FilterVisibleShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      polygon: ParamVar::default(),
      altitude: ParamVar::new(30.0.into()),
      gimbal_pitch: ParamVar::new((-45.0).into()),
      output: ClonedVar::default(),
    }
  }
}

impl FilterVisibleShard {
  /// Calculate where the camera is looking on the ground
  /// Returns (look_lon, look_lat) or None if gimbal is pointing straight down
  fn calculate_look_point(
    waypoint_lon: f64,
    waypoint_lat: f64,
    altitude: f64,
    gimbal_pitch: f64,
    heading: f64,
  ) -> Option<(f64, f64)> {
    // Safety: validate latitude to avoid division issues near poles
    // Waypoints may come from user data without validation
    if waypoint_lat.abs() > MAX_LATITUDE {
      return Some((waypoint_lon, waypoint_lat)); // Return waypoint position as fallback
    }

    // Gimbal pitch is negative (e.g., -45° means 45° below horizontal)
    // At -90° (straight down), the look point is directly below the drone
    // At 0° (horizontal), the look point is at infinity

    // Calculate horizontal distance to look point
    // For gimbal_pitch = -45°: tan(45°) = 1, so distance = altitude
    // For gimbal_pitch = -90°: tan(0°) = 0, so distance = 0 (straight down)
    let pitch_from_horizontal = gimbal_pitch.abs(); // 0 to 90 degrees
    let pitch_from_vertical = 90.0 - pitch_from_horizontal; // 90 to 0 degrees

    // If looking straight down or nearly so, look point is at waypoint position
    if pitch_from_vertical < 1.0 {
      return Some((waypoint_lon, waypoint_lat));
    }

    // Calculate horizontal distance to look point using trigonometry
    // tan(pitch_from_vertical) = horizontal_distance / altitude
    let look_distance_meters = altitude * pitch_from_vertical.to_radians().tan();

    // Convert heading to radians (0° = North, 90° = East)
    let heading_rad = heading.to_radians();

    // Calculate offset in meters
    // sin(heading) gives east component, cos(heading) gives north component
    let offset_x_meters = look_distance_meters * heading_rad.sin();
    let offset_y_meters = look_distance_meters * heading_rad.cos();

    // Convert meters to degrees using equirectangular approximation
    let meters_per_deg_lon = METERS_PER_DEG_LAT * waypoint_lat.to_radians().cos();
    let offset_lon = offset_x_meters / meters_per_deg_lon;
    let offset_lat = offset_y_meters / METERS_PER_DEG_LAT;

    Some((waypoint_lon + offset_lon, waypoint_lat + offset_lat))
  }
}

#[shards::shard_impl]
impl Shard for FilterVisibleShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
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
    // Get polygon parameter
    let polygon_var = self.polygon.get();
    let polygon_table = polygon_var.as_table().map_err(|_| "Invalid Polygon parameter")?;

    // Parse polygon coordinates
    let coords_var = polygon_table
      .get_static("coords")
      .ok_or("Missing 'coords' in polygon")?;
    let coords = parse_coords(coords_var)?;
    if coords.len() < 3 {
      return Err("Polygon must have at least 3 coordinates");
    }

    // Build geo polygon for containment checks
    let geo_poly = build_geo_polygon(&coords);

    // Get default parameters
    let default_altitude: f64 = self.altitude.get().try_into().unwrap_or(30.0);
    let default_gimbal_pitch: f64 = self.gimbal_pitch.get().try_into().unwrap_or(-45.0);

    // Validate gimbal pitch
    if default_gimbal_pitch < -90.0 || default_gimbal_pitch > 0.0 {
      return Err("GimbalPitch must be between -90 and 0 degrees");
    }

    // Parse input waypoints
    let seq: Seq = input.try_into().map_err(|_| "Expected sequence of waypoints")?;

    // Filter waypoints
    let mut output_seq = AutoSeqVar::new();
    let mut new_index: i64 = 0;

    for item in seq.iter() {
      let table = item.as_table().map_err(|_| "Expected waypoint table")?;

      // Get waypoint position
      let lon: f64 = table
        .get_static("x")
        .ok_or("Missing 'x' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'x' value")?;
      let lat: f64 = table
        .get_static("y")
        .ok_or("Missing 'y' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'y' value")?;

      // Get optional fields
      let altitude: f64 = table
        .get_static("altitude")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_altitude);
      let gimbal_pitch: f64 = table
        .get_static("gimbal_pitch")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(default_gimbal_pitch);
      let heading: f64 = table
        .get_static("heading")
        .and_then(|v| v.try_into().ok())
        .unwrap_or(0.0);

      // Calculate look point
      if let Some((look_lon, look_lat)) = Self::calculate_look_point(lon, lat, altitude, gimbal_pitch, heading) {
        // Check if look point is inside polygon
        let look_point = Point::new(look_lon, look_lat);
        if geo_poly.contains(&look_point) {
          // Keep this waypoint - clone it with updated index
          let mut point_table = AutoTableVar::new();
          point_table.0.insert_fast_static("x", &Var::from(lon));
          point_table.0.insert_fast_static("y", &Var::from(lat));
          point_table.0.insert_fast_static("index", &Var::from(new_index));

          // Preserve row if present
          if let Some(row_var) = table.get_static("row") {
            if let Ok(row) = TryInto::<i64>::try_into(row_var) {
              point_table.0.insert_fast_static("row", &Var::from(row));
            }
          }

          // Preserve other optional fields
          if let Some(alt_var) = table.get_static("altitude") {
            point_table.0.insert_fast_static("altitude", alt_var);
          }
          if let Some(speed_var) = table.get_static("speed") {
            point_table.0.insert_fast_static("speed", speed_var);
          }
          if let Some(heading_var) = table.get_static("heading") {
            point_table.0.insert_fast_static("heading", heading_var);
          }
          if let Some(gimbal_var) = table.get_static("gimbal_pitch") {
            point_table.0.insert_fast_static("gimbal_pitch", gimbal_var);
          }

          output_seq.0.emplace_table(point_table);
          new_index += 1;
        }
      }
    }

    self.output = output_seq.to_cloned();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Geo.PathLength Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Geo.PathLength",
  "Calculates the total path length between consecutive waypoints in meters"
)]
struct PathLengthShard {
  #[shard_required]
  required: ExposedTypes,

  output: ClonedVar,
}

impl Default for PathLengthShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for PathLengthShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &FLOAT_TYPES
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
    let seq: Seq = input.try_into().map_err(|_| "Expected sequence of waypoints")?;

    if seq.len() < 2 {
      // Single waypoint or empty - zero distance
      self.output = 0.0f64.into();
      return Ok(Some(self.output.0));
    }

    let mut total_distance: f64 = 0.0;
    let mut prev_lon: Option<f64> = None;
    let mut prev_lat: Option<f64> = None;

    for item in seq.iter() {
      let table = item.as_table().map_err(|_| "Expected waypoint table")?;

      let lon: f64 = table
        .get_static("x")
        .ok_or("Missing 'x' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'x' value")?;
      let lat: f64 = table
        .get_static("y")
        .ok_or("Missing 'y' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'y' value")?;

      if let (Some(prev_x), Some(prev_y)) = (prev_lon, prev_lat) {
        total_distance += haversine_distance(prev_x, prev_y, lon, lat);
      }

      prev_lon = Some(lon);
      prev_lat = Some(lat);
    }

    self.output = total_distance.into();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Geo.SimplifyPath Shard
// ============================================================================

#[derive(shards::shard)]
#[shard_info(
  "Geo.SimplifyPath",
  "Simplifies a waypoint path by removing intermediate points on straight segments, keeping only turn points"
)]
struct SimplifyPathShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Tolerance", "Angle tolerance in degrees for detecting turns (default 1.0)", FLOAT_OR_VAR_TYPES)]
  tolerance: ParamVar,

  output: ClonedVar,
}

impl Default for SimplifyPathShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      tolerance: ParamVar::new(1.0.into()),
      output: ClonedVar::default(),
    }
  }
}

impl SimplifyPathShard {
  /// Calculate bearing from point 1 to point 2 in degrees (0-360)
  fn calculate_bearing(lon1: f64, lat1: f64, lon2: f64, lat2: f64) -> f64 {
    let lat1_rad = lat1.to_radians();
    let lat2_rad = lat2.to_radians();
    let delta_lon = (lon2 - lon1).to_radians();

    let y = delta_lon.sin() * lat2_rad.cos();
    let x = lat1_rad.cos() * lat2_rad.sin() - lat1_rad.sin() * lat2_rad.cos() * delta_lon.cos();

    let bearing = y.atan2(x).to_degrees();
    (bearing + 360.0) % 360.0
  }

  /// Calculate the angular difference between two bearings
  fn bearing_diff(bearing1: f64, bearing2: f64) -> f64 {
    let diff = (bearing2 - bearing1 + 180.0).rem_euclid(360.0) - 180.0;
    diff.abs()
  }
}

#[shards::shard_impl]
impl Shard for SimplifyPathShard {
  fn input_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SEQ_OF_GRID_POINTS_TYPES
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
    let tolerance: f64 = self.tolerance.get().try_into().unwrap_or(1.0);
    let seq: Seq = input.try_into().map_err(|_| "Expected sequence of waypoints")?;

    // Need at least 3 points to simplify
    if seq.len() < 3 {
      // Return input as-is
      return Ok(Some(*input));
    }

    // Parse all waypoints first
    struct WaypointData {
      lon: f64,
      lat: f64,
      heading: Option<f64>,
      table_idx: usize,
    }

    let mut waypoints: Vec<WaypointData> = Vec::with_capacity(seq.len());
    for (idx, item) in seq.iter().enumerate() {
      let table = item.as_table().map_err(|_| "Expected waypoint table")?;

      let lon: f64 = table
        .get_static("x")
        .ok_or("Missing 'x' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'x' value")?;
      let lat: f64 = table
        .get_static("y")
        .ok_or("Missing 'y' in waypoint")?
        .try_into()
        .map_err(|_| "Invalid 'y' value")?;
      let heading: Option<f64> = table.get_static("heading").and_then(|v| v.try_into().ok());

      waypoints.push(WaypointData {
        lon,
        lat,
        heading,
        table_idx: idx,
      });
    }

    // Determine which points to keep
    let mut keep_indices: Vec<usize> = Vec::new();

    // Always keep first point
    keep_indices.push(0);

    for i in 1..waypoints.len() - 1 {
      let prev = &waypoints[i - 1];
      let curr = &waypoints[i];
      let next = &waypoints[i + 1];

      // Check if heading changes (if heading field exists)
      let heading_changed = match (prev.heading, curr.heading) {
        (Some(h1), Some(h2)) => Self::bearing_diff(h1, h2) > tolerance,
        _ => false,
      };

      // Check if path direction changes (turn detection)
      let bearing_to_curr = Self::calculate_bearing(prev.lon, prev.lat, curr.lon, curr.lat);
      let bearing_to_next = Self::calculate_bearing(curr.lon, curr.lat, next.lon, next.lat);
      let direction_changed = Self::bearing_diff(bearing_to_curr, bearing_to_next) > tolerance;

      if heading_changed || direction_changed {
        keep_indices.push(i);
      }
    }

    // Always keep last point
    keep_indices.push(waypoints.len() - 1);

    // Build output sequence with kept waypoints
    let mut output_seq = AutoSeqVar::new();
    let mut new_index: i64 = 0;

    for &orig_idx in &keep_indices {
      let orig_item = &seq[orig_idx];
      let orig_table = orig_item.as_table().map_err(|_| "Expected waypoint table")?;

      let mut point_table = AutoTableVar::new();

      // Copy x, y with new index
      let lon: f64 = orig_table.get_static("x").unwrap().try_into().unwrap();
      let lat: f64 = orig_table.get_static("y").unwrap().try_into().unwrap();
      point_table.0.insert_fast_static("x", &Var::from(lon));
      point_table.0.insert_fast_static("y", &Var::from(lat));
      point_table.0.insert_fast_static("index", &Var::from(new_index));

      // Preserve optional fields
      if let Some(row_var) = orig_table.get_static("row") {
        if let Ok(row) = TryInto::<i64>::try_into(row_var) {
          point_table.0.insert_fast_static("row", &Var::from(row));
        }
      }
      if let Some(alt_var) = orig_table.get_static("altitude") {
        point_table.0.insert_fast_static("altitude", alt_var);
      }
      if let Some(speed_var) = orig_table.get_static("speed") {
        point_table.0.insert_fast_static("speed", speed_var);
      }
      if let Some(heading_var) = orig_table.get_static("heading") {
        point_table.0.insert_fast_static("heading", heading_var);
      }
      if let Some(gimbal_var) = orig_table.get_static("gimbal_pitch") {
        point_table.0.insert_fast_static("gimbal_pitch", gimbal_var);
      }

      output_seq.0.emplace_table(point_table);
      new_index += 1;
    }

    self.output = output_seq.to_cloned();
    Ok(Some(self.output.0))
  }
}

// ============================================================================
// Registration
// ============================================================================

#[no_mangle]
pub extern "C" fn shardsRegister_geo_geo(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<PolygonShard>();
  register_shard::<BufferShard>();
  register_shard::<GridFillShard>();
  register_shard::<ToDjiKmzShard>();
  register_shard::<ToGoogleEarthShard>();
  register_shard::<ToGeoJsonShard>();
  register_shard::<ToLitchiCsvShard>();
  register_shard::<FilterVisibleShard>();
  register_shard::<PathLengthShard>();
  register_shard::<SimplifyPathShard>();
}
