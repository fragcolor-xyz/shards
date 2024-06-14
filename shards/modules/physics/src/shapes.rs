/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

// RigidBody and Collier are unique, Shapes can be shared

use rapier3d::prelude::Ball;
use shards::core::{register_enum, register_shard};
use shards::shard::Shard;
use shards::types::{common_type, NONE_TYPES};
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};

use crate::BaseShape;
use shards::core::register_legacy_shard;
use shards::types::FLOAT_OR_NONE_TYPES_SLICE;

use crate::POSITION_TYPES_SLICE;
use crate::ROTATION_TYPES_SLICE;
use crate::SHAPE_TYPE;
use crate::SHAPE_TYPES;

use shards::types::Parameters;

use shards::shard::LegacyShard;
use shards::types::FLOAT3_TYPES_SLICE;
use shards::types::FLOAT_TYPES_SLICE;

use rapier3d::geometry::SharedShape;

use rapier3d::na::{Isometry3, Quaternion, Translation, UnitQuaternion, Vector3};

use std::convert::TryInto;

lazy_static! {
  static ref MASS_TYPES: Types = vec![
    common_type::none,
    common_type::float,
    common_type::float_var
  ];
}

fn setup_base_shape(
  bs: &mut BaseShape,
  pos: &ParamVar,
  rot: &ParamVar,
  mass: &ParamVar,
) -> Result<(), &'static str> {
  let pos = {
    let p = pos.get();
    if p.is_none() {
      Vector3::new(0.0, 0.0, 0.0)
    } else {
      let (tx, ty, tz): (f32, f32, f32) = p.try_into()?;
      Vector3::new(tx, ty, tz)
    }
  };

  bs.mass = TryInto::<f32>::try_into(mass.get()).ok();

  bs.position = Some({
    let r = rot.get();
    if r.is_none() {
      Isometry3::new(pos, Vector3::new(0.0, 0.0, 0.0))
    } else {
      let quaternion: Result<(f32, f32, f32, f32), &str> = r.try_into();
      if let Ok(quaternion) = quaternion {
        let quaternion = Quaternion::new(quaternion.3, quaternion.0, quaternion.1, quaternion.2);
        let quaternion = UnitQuaternion::from_quaternion(quaternion);
        let pos = Translation::from(pos);
        Isometry3::from_parts(pos, quaternion)
      } else {
        // if setParam validation is correct this is impossible
        panic!("unexpected branch")
      }
    }
  });
  Ok(())
}

#[derive(shards::shard)]
#[shard_info("Physics.Ball", "Physics shape representing a ball")]
struct BallShape {
  #[shard_param("Position", "The position of the shape.", POSITION_TYPES_SLICE)]
  position: ParamVar,
  #[shard_param("Rotation", "The rotation of the shape.", ROTATION_TYPES_SLICE)]
  rotation: ParamVar,
  #[shard_param("Radius", "The radius of the sphere.", FLOAT_TYPES_SLICE)]
  radius: ParamVar,
  #[shard_param("Mass", "The mass of the shape.", MASS_TYPES)]
  mass: ParamVar,
  bs: BaseShape,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for BallShape {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      position: ParamVar::new((0.0, 0.0, 0.0).into()),
      rotation: ParamVar::new((0.0, 0.0, 0.0, 1.0).into()),
      radius: ParamVar::new(0.5.into()),
      mass: ParamVar::default(),
      bs: BaseShape::default(),
    }
  }
}

impl BallShape {
  fn make(&mut self) -> SharedShape {
    let radius = self.radius.get().try_into().unwrap();
    SharedShape::ball(radius)
  }
}

#[shards::shard_impl]
impl Shard for BallShape {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SHAPE_TYPES
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _: &Context, _: &Var) -> Result<Var, &str> {
    setup_base_shape(&mut self.bs, &self.position, &self.rotation, &self.mass)?;
    self.bs.shape = Some(self.make());
    Ok(unsafe { Var::new_object_from_ptr(&self.bs as *const BaseShape, &SHAPE_TYPE) })
  }
}

#[derive(shards::shard)]
#[shard_info("Physics.Cuboid", "Physics shape representing a cuboid")]
struct CuboidShape {
  #[shard_param("Position", "The position of the shape.", POSITION_TYPES_SLICE)]
  position: ParamVar,
  #[shard_param("Rotation", "The rotation of the shape.", ROTATION_TYPES_SLICE)]
  rotation: ParamVar,
  #[shard_param(
    "HalfExtents",
    "The half-extents of the cuboid shape.",
    FLOAT3_TYPES_SLICE
  )]
  half_extents: ParamVar,
  #[shard_param("Mass", "The mass of the shape.", MASS_TYPES)]
  mass: ParamVar,
  bs: BaseShape,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for CuboidShape {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      position: ParamVar::new((0.0, 0.0, 0.0).into()),
      rotation: ParamVar::new((0.0, 0.0, 0.0, 1.0).into()),
      half_extents: ParamVar::new((0.5, 0.5, 0.5).into()),
      mass: ParamVar::default(),
      bs: BaseShape::default(),
    }
  }
}

impl CuboidShape {
  fn make(&mut self) -> SharedShape {
    let (x, y, z) = self.half_extents.get().try_into().unwrap();
    SharedShape::cuboid(x, y, z)
  }
}

#[shards::shard_impl]
impl Shard for CuboidShape {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &SHAPE_TYPES
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _: &Context, _: &Var) -> Result<Var, &str> {
    setup_base_shape(&mut self.bs, &self.position, &self.rotation, &self.mass)?;
    self.bs.shape = Some(self.make());
    Ok(unsafe { Var::new_object_from_ptr(&self.bs as *const BaseShape, &SHAPE_TYPE) })
  }
}

pub fn register_shards() {
  register_shard::<BallShape>();
  register_shard::<CuboidShape>();
}
