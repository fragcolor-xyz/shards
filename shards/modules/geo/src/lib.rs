/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

//! Geospatial operations for drone mission planning.
//!
//! Provides shards for:
//! - `Geo.Polygon` - Creates a polygon from coordinate sequence
//! - `Geo.GridFill` - Fills a polygon with grid points in boustrophedon pattern
//! - `Geo.ToDjiKmz` - Exports waypoints to DJI Fly compatible KMZ files

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use geo::{AffineOps, AffineTransform, BoundingRect, Centroid, Contains, Coord, Point, Polygon};
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{
  common_type, AutoSeqVar, AutoTableVar, ClonedVar, Context, ExposedTypes, InstanceData, ParamVar,
  Seq, Type, Types, Var, FLOAT_TYPES_SLICE, SEQ_OF_SEQ_OF_FLOAT_TYPES, STRING_TYPES,
};
use std::io::Write;
use std::time::{SystemTime, UNIX_EPOCH};
use zip::write::SimpleFileOptions;
use zip::ZipWriter;

// ============================================================================
// Constants
// ============================================================================

const METERS_PER_DEG_LAT: f64 = 111_319.9;

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

  // Waypoint input types - accepts any sequence of tables
  static ref SEQ_OF_ANY_TABLE: Type = Type::seq(&[common_type::any_table]);
  static ref SEQ_OF_ANY_TABLE_TYPES: Vec<Type> = vec![*SEQ_OF_ANY_TABLE];
}

// ============================================================================
// Helper Functions
// ============================================================================

/// Maximum latitude for equirectangular projection (breaks near poles)
const MAX_LATITUDE: f64 = 85.0;

/// Equirectangular projection: convert lon/lat to meters relative to origin
/// Note: This projection is only accurate for latitudes < 85 degrees
fn lonlat_to_meters(lon: f64, lat: f64, origin_lon: f64, origin_lat: f64) -> (f64, f64) {
  let meters_per_deg_lon = METERS_PER_DEG_LAT * origin_lat.to_radians().cos();
  (
    (lon - origin_lon) * meters_per_deg_lon,
    (lat - origin_lat) * METERS_PER_DEG_LAT,
  )
}

/// Inverse equirectangular projection: convert meters back to lon/lat
fn meters_to_lonlat(x: f64, y: f64, origin_lon: f64, origin_lat: f64) -> (f64, f64) {
  let meters_per_deg_lon = METERS_PER_DEG_LAT * origin_lat.to_radians().cos();
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

  let mut min_lon = f64::INFINITY;
  let mut max_lon = f64::NEG_INFINITY;

  for &(lon, lat) in coords {
    // Validate longitude range
    if lon < -180.0 || lon > 180.0 {
      return Err("Longitude must be between -180 and 180 degrees");
    }
    // Validate latitude for equirectangular projection
    validate_latitude(lat)?;

    min_lon = min_lon.min(lon);
    max_lon = max_lon.max(lon);
  }

  // Check for antimeridian crossing (simple heuristic: lon span > 180° indicates crossing)
  if max_lon - min_lon > 180.0 {
    return Err("Polygon crosses antimeridian (±180°), which is not supported");
  }

  Ok(())
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

  #[shard_param("SpacingX", "Grid spacing in X direction (meters)", FLOAT_TYPES_SLICE)]
  spacing_x: ParamVar,

  #[shard_param("SpacingY", "Grid spacing in Y direction (meters)", FLOAT_TYPES_SLICE)]
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
    let direction: f64 = if self.direction.get().is_none() {
      0.0
    } else {
      self.direction.get().try_into().unwrap_or(0.0)
    };

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

    // Generate grid points within bounding box
    // Use integer-based iteration to avoid floating-point accumulation errors
    // floor() ensures we don't generate points beyond the bounding box
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
struct Waypoint {
  lon: f64,
  lat: f64,
  index: i64,
  altitude: f64,
  speed: f64,
  gimbal_pitch: f64,
  heading: f64,
}

#[derive(shards::shard)]
#[shard_info(
  "Geo.ToDjiKmz",
  "Exports waypoints to DJI Fly compatible KMZ file for drone missions"
)]
struct ToDjiKmzShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Path", "Output file path for the KMZ file", STRING_TYPES)]
  path: ParamVar,

  #[shard_param("FinishAction", "Action after mission: goHome, noAction, or autoLand", STRING_VAR_OR_NONE_TYPES)]
  finish_action: ParamVar,

  #[shard_param("Speed", "Default flight speed in m/s", FLOAT_OR_VAR_TYPES)]
  speed: ParamVar,

  #[shard_param("Altitude", "Default altitude in meters (relative to takeoff)", FLOAT_OR_VAR_TYPES)]
  altitude: ParamVar,

  #[shard_param("GimbalPitch", "Default gimbal pitch angle (-90 to 0)", FLOAT_OR_VAR_TYPES)]
  gimbal_pitch: ParamVar,

  #[shard_param("DroneEnum", "DJI drone enum value (default 68 for Mini 4 Pro)", FLOAT_OR_VAR_TYPES)]
  drone_enum: ParamVar,

  #[shard_param("DroneSubEnum", "DJI drone sub-enum value (default 0)", FLOAT_OR_VAR_TYPES)]
  drone_sub_enum: ParamVar,

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

  fn generate_template_kml(
    &self,
    finish_action: &str,
    speed: f64,
    timestamp: u64,
    drone_enum: i64,
    drone_sub_enum: i64,
  ) -> String {
    format!(
      r#"<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2" xmlns:wpml="http://www.dji.com/wpmz/1.0.2">
  <Document>
    <wpml:author>Shards</wpml:author>
    <wpml:createTime>{}</wpml:createTime>
    <wpml:updateTime>{}</wpml:updateTime>
    <wpml:missionConfig>
      <wpml:flyToWaylineMode>safely</wpml:flyToWaylineMode>
      <wpml:finishAction>{}</wpml:finishAction>
      <wpml:exitOnRCLost>executeLostAction</wpml:exitOnRCLost>
      <wpml:executeRCLostAction>goBack</wpml:executeRCLostAction>
      <wpml:globalTransitionalSpeed>{:.1}</wpml:globalTransitionalSpeed>
      <wpml:droneInfo>
        <wpml:droneEnumValue>{}</wpml:droneEnumValue>
        <wpml:droneSubEnumValue>{}</wpml:droneSubEnumValue>
      </wpml:droneInfo>
    </wpml:missionConfig>
  </Document>
</kml>
"#,
      timestamp, timestamp, finish_action, speed, drone_enum, drone_sub_enum
    )
  }

  fn generate_waylines_wpml(
    &self,
    waypoints: &[Waypoint],
    finish_action: &str,
    speed: f64,
    drone_enum: i64,
    drone_sub_enum: i64,
  ) -> String {
    let mut placemarks = String::new();
    for wp in waypoints {
      placemarks.push_str(&format!(
        r#"      <Placemark>
        <Point>
          <coordinates>{:.15},{:.15}</coordinates>
        </Point>
        <wpml:index>{}</wpml:index>
        <wpml:executeHeight>{:.4}</wpml:executeHeight>
        <wpml:waypointSpeed>{:.1}</wpml:waypointSpeed>
        <wpml:waypointHeadingParam>
          <wpml:waypointHeadingMode>smoothTransition</wpml:waypointHeadingMode>
          <wpml:waypointHeadingAngle>{}</wpml:waypointHeadingAngle>
          <wpml:waypointPoiPoint>0,0,0.000000</wpml:waypointPoiPoint>
          <wpml:waypointHeadingAngleEnable>1</wpml:waypointHeadingAngleEnable>
          <wpml:waypointHeadingPathMode>followBadArc</wpml:waypointHeadingPathMode>
        </wpml:waypointHeadingParam>
        <wpml:waypointTurnParam>
          <wpml:waypointTurnMode>toPointAndStopWithDiscontinuityCurvature</wpml:waypointTurnMode>
          <wpml:waypointTurnDampingDist>0</wpml:waypointTurnDampingDist>
        </wpml:waypointTurnParam>
        <wpml:useStraightLine>0</wpml:useStraightLine>
        <wpml:actionGroup>
          <wpml:actionGroupId>{}</wpml:actionGroupId>
          <wpml:actionGroupStartIndex>{}</wpml:actionGroupStartIndex>
          <wpml:actionGroupEndIndex>{}</wpml:actionGroupEndIndex>
          <wpml:actionGroupMode>sequence</wpml:actionGroupMode>
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
          </wpml:action>
        </wpml:actionGroup>
      </Placemark>
"#,
        wp.lon,
        wp.lat,
        wp.index,
        wp.altitude,
        wp.speed,
        wp.heading.round() as i64,
        wp.index,
        wp.index,
        wp.index,
        wp.gimbal_pitch.round() as i64
      ));
    }

    format!(
      r#"<?xml version="1.0" encoding="UTF-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2" xmlns:wpml="http://www.dji.com/wpmz/1.0.2">
  <Document>
    <wpml:missionConfig>
      <wpml:flyToWaylineMode>safely</wpml:flyToWaylineMode>
      <wpml:finishAction>{}</wpml:finishAction>
      <wpml:exitOnRCLost>executeLostAction</wpml:exitOnRCLost>
      <wpml:executeRCLostAction>goBack</wpml:executeRCLostAction>
      <wpml:globalTransitionalSpeed>{:.1}</wpml:globalTransitionalSpeed>
      <wpml:droneInfo>
        <wpml:droneEnumValue>{}</wpml:droneEnumValue>
        <wpml:droneSubEnumValue>{}</wpml:droneSubEnumValue>
      </wpml:droneInfo>
    </wpml:missionConfig>
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
      finish_action, speed, drone_enum, drone_sub_enum, speed, placemarks
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

    // Parse input waypoints
    let seq: Seq = input.try_into().map_err(|_| "Expected sequence of waypoints")?;

    if seq.len() > MAX_DJI_WAYPOINTS {
      return Err("Too many waypoints (max 200 for DJI Fly)");
    }

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
      });
    }

    // Generate KML/WPML content
    let timestamp = Self::get_timestamp_ms();
    let template_kml = self.generate_template_kml(finish_action, default_speed, timestamp, drone_enum, drone_sub_enum);
    let waylines_wpml = self.generate_waylines_wpml(&waypoints, finish_action, default_speed, drone_enum, drone_sub_enum);

    // Create KMZ file (ZIP with wpmz/ folder)
    let file = std::fs::File::create(path).map_err(|_| "Failed to create KMZ file")?;
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

    // Passthrough input
    Ok(Some(*input))
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
  register_shard::<GridFillShard>();
  register_shard::<ToDjiKmzShard>();
}
