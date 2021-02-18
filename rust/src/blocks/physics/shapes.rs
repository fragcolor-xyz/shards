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
        self.get_parameters()
      }

      fn setParam(&mut self, index: i32, value: &Var) {
        self.set_param(index, value);
      }

      fn getParam(&mut self, index: i32) -> Var {
        self.get_param(index)
      }

      fn cleanup(&mut self) {
        self.shape = None;
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
  static ref BALL_PARAMETERS: Parameters = vec![(
    "Radius",
    "The radius of the sphere.",
    vec![common_type::float]
  )
    .into()];
}

#[derive(Default)]
struct BallShape {
  shape: Option<SharedShape>,
  radius: f32,
}

impl BallShape {
  fn make(&mut self) -> SharedShape {
    SharedShape::ball(self.radius)
  }

  fn get_parameters(&mut self) -> Option<&Parameters> {
    Some(&BALL_PARAMETERS)
  }

  fn set_param(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.radius = value.as_ref().try_into().unwrap(),
      _ => unreachable!(),
    }
  }

  fn get_param(&mut self, index: i32) -> Var {
    match index {
      0 => self.radius.into(),
      _ => unreachable!(),
    }
  }
}

shape!(
  BallShape,
  "Physics.Shape.Ball",
  "Physics.Shape.Ball-rust-0x20200101"
);

lazy_static! {
  static ref CUBE_PARAMETERS: Parameters = vec![(
    "HalfExtents",
    "The half-extents of the cuboid shape.",
    vec![common_type::float3]
  )
    .into()];
}

#[derive(Default)]
struct CubeShape {
  shape: Option<SharedShape>,
  hx: f32,
  hy: f32,
  hz: f32,
}

impl CubeShape {
  fn make(&mut self) -> SharedShape {
    SharedShape::cuboid(self.hx, self.hy, self.hz)
  }

  fn get_parameters(&mut self) -> Option<&Parameters> {
    Some(&CUBE_PARAMETERS)
  }

  fn set_param(&mut self, index: i32, value: &Var) {
    match index {
      0 => {
        let (hx, hy, hz) = value.try_into().unwrap();
        self.hx = hx;
        self.hy = hy;
        self.hz = hz;
      }
      _ => unreachable!(),
    }
  }

  fn get_param(&mut self, index: i32) -> Var {
    match index {
      0 => (self.hx, self.hy, self.hz).into(),
      _ => unreachable!(),
    }
  }
}

shape!(
  CubeShape,
  "Physics.Shape.Cuboid",
  "Physics.Shape.Cuboid-rust-0x20200101"
);

pub fn registerBlocks() {
  registerBlock::<BallShape>();
  registerBlock::<CubeShape>();
}
