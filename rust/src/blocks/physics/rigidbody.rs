use crate::blocks::physics::fill_seq_from_mat4;
use crate::blocks::physics::mat4_from_seq;
use crate::blocks::physics::BaseShape;
use crate::blocks::physics::Simulation;
use crate::blocks::physics::EXPOSED_SIMULATION;
use crate::blocks::physics::SHAPES_TYPE;
use crate::blocks::physics::SHAPES_VAR_TYPE;
use crate::blocks::physics::SHAPE_TYPE;
use crate::blocks::physics::SHAPE_VAR_TYPE;
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
use rapier3d::na::Isometry3;
use rapier3d::na::Projective3;
use rapier3d::na::Rotation3;
use rapier3d::na::Transform3;
use rapier3d::na::{
  Matrix, Matrix3, Matrix4, Quaternion, Similarity, Similarity3, Translation, UnitQuaternion,
  Vector3, U3,
};
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline};
use std::convert::TryInto;

struct RigidBody {
  simulation_var: ParamVar,
  shape_var: ParamVar,
  rigid_body: Option<RigidBodyHandle>,
  collider: Option<ColliderHandle>,
  position: ParamVar,
  rotation: ParamVar,
}

impl Default for RigidBody {
  fn default() -> Self {
    let mut r = RigidBody {
      simulation_var: ParamVar::new(().into()),
      shape_var: ParamVar::new(().into()),
      rigid_body: None,
      collider: None,
      position: ParamVar::new((0.0, 0.0, 0.0).into()),
      rotation: ParamVar::new((0.0, 0.0, 0.0, 1.0).into()),
    };
    r.simulation_var.setName("Physics.Simulation");
    r
  }
}

impl RigidBody {
  fn _set_param(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.shape_var.setParam(value),
      1 => self.position.setParam(value),
      2 => self.rotation.setParam(value),
      _ => unreachable!(),
    }
  }

  fn _get_param(&mut self, index: i32) -> Var {
    match index {
      0 => self.shape_var.getParam(),
      1 => self.position.getParam(),
      2 => self.rotation.getParam(),
      _ => unreachable!(),
    }
  }

  fn _cleanup(&mut self) {
    if let Some(rigid_body) = self.rigid_body {
      let sim_var = self.simulation_var.get();
      let simulation = Var::into_object_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE).unwrap();
      simulation.bodies.remove(
        rigid_body,
        &mut simulation.colliders,
        &mut simulation.joints,
      );
      self.rigid_body = None;
    }

    self.simulation_var.cleanup();
    self.shape_var.cleanup();
    self.position.cleanup();
    self.rotation.cleanup();
  }

  fn _warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.simulation_var.warmup(context);
    self.shape_var.warmup(context);
    self.position.warmup(context);
    self.rotation.warmup(context);
    Ok(())
  }

  fn _populate(&mut self, status: BodyStatus) -> Result<(RigidBodyHandle, Var, Var), &str> {
    let p = self.position.get();
    let r = self.rotation.get();
    if self.rigid_body.is_none() {
      let pos = {
        if p.is_none() {
          Vector3::new(0.0, 0.0, 0.0)
        } else {
          let (tx, ty, tz): (f32, f32, f32) = p.as_ref().try_into()?;
          Vector3::new(tx, ty, tz)
        }
      };

      let iso = {
        if r.is_none() {
          Isometry3::new(pos, Vector3::new(0.0, 0.0, 0.0))
        } else {
          let quaternion: Result<(f32, f32, f32, f32), &str> = r.as_ref().try_into();
          if let Ok(quaternion) = quaternion {
            let quaternion =
              Quaternion::new(quaternion.3, quaternion.0, quaternion.1, quaternion.2);
            let quaternion = UnitQuaternion::from_quaternion(quaternion);
            let pos = Translation::from(pos);
            Isometry3::from_parts(pos, quaternion)
          } else {
            // if setParam validation is correct this is impossible
            panic!("unexpected branch")
          }
        }
      };

      let simulation = self.simulation_var.get();
      let simulation = Var::into_object_mut_ref::<Simulation>(simulation, &SIMULATION_TYPE)?;

      let rigid_body = RigidBodyBuilder::new(status).position(iso).build();
      let rigid_body = simulation.bodies.insert(rigid_body);

      let shape = self.shape_var.get();
      if shape.is_seq() {
        let shapes: Seq = shape.try_into().unwrap();
        for shape in shapes {
          let shapeInfo = Var::into_object_mut_ref::<BaseShape>(shape, &SHAPE_TYPE)?;
          let shape = shapeInfo.shape.as_ref().unwrap().clone();
          let collider = ColliderBuilder::new(shape)
            .position(shapeInfo.position.unwrap())
            .build();
          self.collider = Some(simulation.colliders.insert(
            collider,
            rigid_body,
            &mut simulation.bodies,
          ));
        }
      } else {
        let shapeInfo = Var::into_object_mut_ref::<BaseShape>(shape, &SHAPE_TYPE)?;
        let shape = shapeInfo.shape.as_ref().unwrap().clone();
        let collider = ColliderBuilder::new(shape)
          .position(shapeInfo.position.unwrap())
          .build();
        self.collider = Some(simulation.colliders.insert(
          collider,
          rigid_body,
          &mut simulation.bodies,
        ));
      }
      self.rigid_body = Some(rigid_body);
    }
    Ok((self.rigid_body.unwrap(), p, r))
  }
}

lazy_static! {
  static ref PARAMETERS: Parameters = vec![
    (
      cstr!("Shapes"),
      cstr!("The shape or shapes of this rigid body."),
      vec![*SHAPE_VAR_TYPE, *SHAPES_VAR_TYPE, common_type::none]
    )
      .into(),
    (
      cstr!("Position"),
      cstr!("The initial position of this rigid body."),
      vec![common_type::float3, common_type::float3_var]
    )
      .into(),
    (
      cstr!("Rotation"),
      cstr!("The initial rotation of this rigid body. Either axis angles in radians Float3 or a quaternion Float4"),
      vec![common_type::float4, common_type::float4_var]
    )
      .into()
  ];
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
    cstr!("Physics.DynamicBody")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.DynamicBody-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Physics.DynamicBody"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &FLOAT4X4_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 | 1 | 2 => self.rb._set_param(index, value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 | 1 | 2 => self.rb._get_param(index),
      _ => unreachable!(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn cleanup(&mut self) {
    self.rb._cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.rb._warmup(context)
  }

  fn activate(&mut self, _: &Context, _input: &Var) -> Result<Var, &str> {
    let sim_var = self.rb.simulation_var.get();
    let simulation = Var::into_object_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
    let (rigid_body, _, _) = self.rb._populate(BodyStatus::Dynamic)?;
    let rb = simulation.bodies.get(rigid_body).unwrap();
    let mat: Matrix4<f32> = rb.position().to_matrix();
    fill_seq_from_mat4(&mut self.output, &mat);
    Ok(self.output.as_ref().into())
  }
}

#[derive(Default)]
struct StaticRigidBody {
  rb: RigidBody,
}

impl Block for StaticRigidBody {
  fn registerName() -> &'static str {
    cstr!("Physics.StaticBody")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.StaticBody-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Physics.StaticBody"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 | 1 | 2 => self.rb._set_param(index, value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 | 1 | 2 => self.rb._get_param(index),
      _ => unreachable!(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn cleanup(&mut self) {
    self.rb._cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.rb._warmup(context)
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    self.rb._populate(BodyStatus::Static)?;
    Ok(*input)
  }
}

#[derive(Default)]
struct KinematicRigidBody {
  rb: RigidBody,
  output: Seq,
}

impl Block for KinematicRigidBody {
  fn registerName() -> &'static str {
    cstr!("Physics.KinematicBody")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.KinematicBody-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Physics.KinematicBody"
  }

  fn inputTypes(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &FLOAT4X4_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 | 1 | 2 => self.rb._set_param(index, value),
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 | 1 | 2 => self.rb._get_param(index),
      _ => unreachable!(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn cleanup(&mut self) {
    self.rb._cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.rb._warmup(context)
  }

  fn activate(&mut self, _: &Context, _input: &Var) -> Result<Var, &str> {
    let sim_var = self.rb.simulation_var.get();
    let simulation = Var::into_object_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
    let (rigid_body, p, r) = self.rb._populate(BodyStatus::Kinematic)?;
    let rb = simulation.bodies.get_mut(rigid_body).unwrap();

    // this guy will read constantly pos and rotations from variable values
    let pos = {
      if p.is_none() {
        Vector3::new(0.0, 0.0, 0.0)
      } else {
        let (tx, ty, tz): (f32, f32, f32) = p.as_ref().try_into()?;
        Vector3::new(tx, ty, tz)
      }
    };

    let iso = {
      if r.is_none() {
        Isometry3::new(pos, Vector3::new(0.0, 0.0, 0.0))
      } else {
        let quaternion: Result<(f32, f32, f32, f32), &str> = r.as_ref().try_into();
        if let Ok(quaternion) = quaternion {
          let quaternion =
            Quaternion::new(quaternion.3, quaternion.0, quaternion.1, quaternion.2);
          let quaternion = UnitQuaternion::from_quaternion(quaternion);
          let pos = Translation::from(pos);
          Isometry3::from_parts(pos, quaternion)
        } else {
          // if setParam validation is correct this is impossible
          panic!("unexpected branch")
        }
      }
    };
    rb.set_next_kinematic_position(iso);

    // read the interpolated position and output it
    let mat: Matrix4<f32> = rb.position().to_matrix();
    fill_seq_from_mat4(&mut self.output, &mat);
    Ok(self.output.as_ref().into())
  }
}

pub fn registerBlocks() {
  registerBlock::<DynamicRigidBody>();
  registerBlock::<StaticRigidBody>();
  registerBlock::<KinematicRigidBody>();
}
