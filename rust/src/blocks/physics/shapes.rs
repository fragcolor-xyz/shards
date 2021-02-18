// RigidBody and Collier are unique, Shapes can be shared

use crate::blocks::physics::Simulation;
use crate::blocks::physics::EXPOSED_SIMULATION;
use crate::blocks::physics::SHAPE_TYPE;
use crate::blocks::physics::SHAPE_TYPES;
use crate::blocks::physics::SIMULATION_TYPE;
use crate::core::registerBlock;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Type;
use crate::types::ANY_TYPES;
use crate::types::NONE_TYPES;
use crate::Block;
use crate::Types;
use crate::Var;
use rapier3d::dynamics::{IntegrationParameters, JointSet, RigidBodySet};
use rapier3d::geometry::SharedShape;
use rapier3d::geometry::{BroadPhase, ColliderSet, ContactEvent, IntersectionEvent, NarrowPhase};
use rapier3d::na::Vector3;
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline};
use std::convert::TryInto;

macro_rules! shape {
  ($block_name:ident, $name_str:literal, $hash:literal) => {
    impl Block for $block_name {
      fn registerName() -> &'static str {
        cstr!($name_str)
      }

      fn hash() -> u32 {
        compile_time_crc32::crc32!($hash)
      }

      fn name(&mut self) -> &str {
        $name_str
      }

      fn inputTypes(&mut self) -> &Types {
        &NONE_TYPES
      }

      fn outputTypes(&mut self) -> &Types {
        &SHAPE_TYPES
      }

      fn parameters(&mut self) -> Option<&Parameters> {
        self._parameters()
      }

      fn setParam(&mut self, index: i32, value: &Var) {
        match index {
          0 => self.position.setParam(value),
          1 => self.rotation.setParam(value),
          _ => self._set_param(index, value),
        }
      }

      fn getParam(&mut self, index: i32) -> Var {
        match index {
          0 => self.position.getParam(),
          1 => self.rotation.getParam(),
          _ => self._get_param(index),
        }
      }

      fn cleanup(&mut self) {
        self.shape = None;
        self.position.cleanup();
        self.rotation.cleanup();
        self._cleanup();
      }

      fn warmup(&mut self, context: &Context) -> Result<(), &str> {
        self.position.warmup(context);
        self.rotation.warmup(context);
        self._warmup(context)
      }

      fn activate(&mut self, _: &Context, _: &Var) -> Result<Var, &str> {
        self.shape = Some(self.make());
        Ok(unsafe {
          Var::new_object_from_ptr(
            self.shape.as_ref().unwrap() as *const SharedShape,
            &SHAPE_TYPE,
          )
        })
      }
    }
  };
}

lazy_static! {
  static ref BALL_PARAMETERS: Parameters = vec![
    (
      "Position",
      "The position wrt. the body it is attached to.",
      vec![common_type::float3, common_type::float3_var]
    )
      .into(),
    (
      "Rotation",
      "The rotation  wrt. the body it is attached to",
      vec![common_type::float3, common_type::float3_var, common_type::float4, common_type::float4_var]
    )
      .into(),
    (
      "Radius",
      "The radius of the sphere.",
      vec![common_type::float]
    )
    .into()
  ];
}

struct BallShape {
  shape: Option<SharedShape>,
  position: ParamVar,
  rotation: ParamVar,
  radius: ParamVar,
}

impl Default for BallShape {
  fn default() -> Self {
    BallShape {
      shape: None,
      position: ParamVar::new((0.0, 0.0, 0.0).into()),
      rotation: ParamVar::new((0.0, 0.0, 0.0, 1.0).into()),
      radius: ParamVar::new(0.5.into()),
    }
  }
}

impl BallShape {
  fn _cleanup(&mut self) {
    self.radius.cleanup();
  }

  fn _warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.radius.warmup(context);
    Ok(())
  }

  fn make(&mut self) -> SharedShape {
    let radius = self.radius.get().as_ref().try_into().unwrap();
    SharedShape::ball(radius)
  }

  fn _parameters(&mut self) -> Option<&Parameters> {
    Some(&BALL_PARAMETERS)
  }

  fn _set_param(&mut self, index: i32, value: &Var) {
    match index {
      2 => self.radius.setParam(value),
      _ => unreachable!(),
    }
  }

  fn _get_param(&mut self, index: i32) -> Var {
    match index {
      2 => self.radius.getParam(),
      _ => unreachable!(),
    }
  }
}

shape!(BallShape, "Physics.Ball", "Physics.Ball-rust-0x20200101");

lazy_static! {
  static ref CUBE_PARAMETERS: Parameters = vec![
    (
      "Position",
      "The position wrt. the body it is attached to.",
      vec![common_type::float3, common_type::float3_var]
    )
      .into(),
    (
      "Rotation",
      "The rotation  wrt. the body it is attached to",
      vec![common_type::float3, common_type::float3_var, common_type::float4, common_type::float4_var]
    )
      .into(),
    (
      "HalfExtents",
      "The half-extents of the cuboid shape.",
      vec![common_type::float3]
    )
    .into()
  ];
}

struct CubeShape {
  shape: Option<SharedShape>,
  position: ParamVar,
  rotation: ParamVar,
  half_extents: ParamVar,
}

impl Default for CubeShape {
  fn default() -> Self {
    CubeShape {
      shape: None,
      position: ParamVar::new((0.0, 0.0, 0.0).into()),
      rotation: ParamVar::new((0.0, 0.0, 0.0, 1.0).into()),
      half_extents: ParamVar::new((0.5, 0.5, 0.5).into()),
    }
  }
}

impl CubeShape {
  fn _cleanup(&mut self) {
    self.half_extents.cleanup();
  }

  fn _warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.half_extents.warmup(context);
    Ok(())
  }

  fn make(&mut self) -> SharedShape {
    let (x, y, z) = self.half_extents.get().as_ref().try_into().unwrap();
    SharedShape::cuboid(x, y, z)
  }

  fn _parameters(&mut self) -> Option<&Parameters> {
    Some(&CUBE_PARAMETERS)
  }

  fn _set_param(&mut self, index: i32, value: &Var) {
    match index {
      2 => self.half_extents.setParam(value),
      _ => unreachable!(),
    }
  }

  fn _get_param(&mut self, index: i32) -> Var {
    match index {
      2 => self.half_extents.getParam(),
      _ => unreachable!(),
    }
  }
}

shape!(
  CubeShape,
  "Physics.Cuboid",
  "Physics.Cuboid-rust-0x20200101"
);

pub fn registerBlocks() {
  registerBlock::<BallShape>();
  registerBlock::<CubeShape>();
}
