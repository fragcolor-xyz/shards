use crate::blocks::physics::fill_seq_from_mat4;
use crate::blocks::physics::mat4_from_seq;
use crate::blocks::physics::Simulation;
use crate::blocks::physics::EXPOSED_SIMULATION;
use crate::blocks::physics::SIMULATION_TYPE;
use crate::core::registerBlock;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Type;
use crate::types::ANY_TYPES;
use crate::types::FLOAT4X2_TYPE;
use crate::types::FLOAT4X2_TYPES;
use crate::types::FLOAT4X4_TYPE;
use crate::types::FLOAT4X4_TYPES;
use crate::types::NONE_TYPES;
use crate::Block;
use crate::Types;
use crate::Var;
use rapier3d::dynamics::RigidBodyBuilder;
use rapier3d::dynamics::{
  BodyStatus, IntegrationParameters, JointSet, RigidBodyHandle, RigidBodySet,
};
use rapier3d::geometry::{
  BroadPhase, ColliderBuilder, ColliderHandle, ColliderSet, ContactEvent, IntersectionEvent,
  NarrowPhase, SharedShape,
};
use rapier3d::math::Real;
use rapier3d::na::try_convert;
use rapier3d::na::Isometry3;
use rapier3d::na::Projective3;
use rapier3d::na::Rotation3;
use rapier3d::na::Transform3;
use rapier3d::na::{
  convert_ref, Matrix, Matrix3, Matrix4, Quaternion, Similarity, Similarity3, UnitQuaternion,
  Vector3, U3,
};
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline};
use std::convert::TryInto;

struct RigidBody {
  simulation_var: ParamVar,
  rigid_body: Option<RigidBodyHandle>,
  default_collider: Option<ColliderHandle>,
}

impl Default for RigidBody {
  fn default() -> Self {
    let mut r = RigidBody {
      simulation_var: ParamVar::new(().into()),
      rigid_body: None,
      default_collider: None,
    };
    r.simulation_var.setName("Physics.Simulation");
    r
  }
}

struct DynamicRigidBody {
  rb: RigidBody,
  output: Seq,
}

impl Default for DynamicRigidBody {
  fn default() -> Self {
    let mut drb = DynamicRigidBody {
      rb: RigidBody::default(),
      output: Seq::new(),
    };
    drb.output.set_len(4);
    drb
  }
}

impl Block for DynamicRigidBody {
  fn registerName() -> &'static str {
    cstr!("Physics.DynamicRigidBody")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.DynamicRigidBody-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Physics.DynamicRigidBody"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &FLOAT4X4_TYPES
  }

  // fn parameters(&mut self) -> Option<&Parameters> {
  //   Some(&PARAMETERS)
  // }

  // fn setParam(&mut self, index: i32, value: &Var) {
  //   match index {
  //     0 => {
  //       let (x, y, z) = value.try_into().unwrap();
  //       self.gravity = Vector3::new(x, y, z);
  //     }
  //     _ => unreachable!(),
  //   }
  // }
  // fn getParam(&mut self, index: i32) -> Var {
  //   match index {
  //     0 => (self.gravity[0], self.gravity[1], self.gravity[2]).into(),
  //     _ => unreachable!(),
  //   }
  // }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn cleanup(&mut self) {
    self.rb.simulation_var.cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.rb.simulation_var.warmup(context);
    let sim_var = self.rb.simulation_var.get();
    unsafe {
      let simulation = Var::into_object_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
      let rigid_body = RigidBodyBuilder::new(BodyStatus::Dynamic).build();
      self.rb.rigid_body = Some(simulation.bodies.insert(rigid_body));

      // TODO
      let collider = ColliderBuilder::new(SharedShape::ball(0.5)).build();
      self.rb.default_collider = Some(simulation.colliders.insert(
        collider,
        self.rb.rigid_body.unwrap(),
        &mut simulation.bodies,
      ));
    }
    Ok(())
  }

  fn activate(&mut self, _: &Context, _input: &Var) -> Result<Var, &str> {
    let sim_var = self.rb.simulation_var.get();
    let rb = unsafe {
      let simulation = Var::into_object_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
      simulation.bodies.get(self.rb.rigid_body.unwrap()).unwrap() // TODO could this be dangerous?
    };
    let mat: Matrix4<f32> = rb.position().to_matrix();
    fill_seq_from_mat4(&mut self.output, &mat);
    Ok(self.output.as_ref().into())
  }
}

lazy_static! {
  static ref STATIC_PARAMETERS: Parameters = vec![
    (
      "Position",
      "The permanent position of this static rigid body.",
      vec![common_type::float3]
    )
      .into(),
    (
      "Rotation",
      "The permanent rotation of this static rigid body.",
      vec![common_type::float3]
    )
      .into()
  ];
}

#[derive(Default)]
struct StaticRigidBody {
  rb: RigidBody,
  position: ParamVar,
  rotation: ParamVar,
}

impl Block for StaticRigidBody {
  fn registerName() -> &'static str {
    cstr!("Physics.StaticRigidBody")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.StaticRigidBody-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Physics.StaticRigidBody"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&STATIC_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.position.setParam(value),
      1 => self.position.setParam(value),
      _ => unreachable!(),
    }
  }
  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.rotation.getParam(),
      1 => self.rotation.getParam(),
      _ => unreachable!(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn cleanup(&mut self) {
    self.rb.simulation_var.cleanup();
    self.position.cleanup();
    self.rotation.cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.rb.simulation_var.warmup(context);
    self.position.warmup(context);
    self.rotation.warmup(context);

    let sim_var = self.rb.simulation_var.get();
    unsafe {
      let pos = {
        let p = self.position.get();
        if p.is_none() {
          Vector3::new(0.0, 0.0, 0.0)
        } else {
          let (tx, ty, tz): (f32, f32, f32) = p.as_ref().try_into()?;
          Vector3::new(tx, ty, tz)
        }
      };
      let rot = {
        let r = self.rotation.get();
        if r.is_none() {
          Vector3::new(0.0, 0.0, 0.0)
        } else {
          let (tx, ty, tz): (f32, f32, f32) = r.as_ref().try_into()?;
          Vector3::new(tx, ty, tz)
        }
      };
      let simulation = Var::into_object_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
      let rigid_body = RigidBodyBuilder::new(BodyStatus::Static)
        .position(Isometry3::new(pos, rot))
        .build();
      self.rb.rigid_body = Some(simulation.bodies.insert(rigid_body));

      // TODO
      let collider = ColliderBuilder::new(SharedShape::cuboid(100.0, 0.1, 100.0)).build();
      self.rb.default_collider = Some(simulation.colliders.insert(
        collider,
        self.rb.rigid_body.unwrap(),
        &mut simulation.bodies,
      ));
    }
    Ok(())
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    // TODO noop block override during compose to avoid this call
    Ok(*input)
  }
}

pub fn registerBlocks() {
  registerBlock::<DynamicRigidBody>();
  registerBlock::<StaticRigidBody>();
}
