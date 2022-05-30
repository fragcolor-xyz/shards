/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::shards::physics::fill_seq_from_mat4;
use crate::shards::physics::mat4_from_seq;
use crate::shards::physics::BaseShape;
use crate::shards::physics::RigidBody;
use crate::shards::physics::Simulation;
use crate::shards::physics::EXPOSED_SIMULATION;
use crate::shards::physics::RIGIDBODY_TYPE;
use crate::shards::physics::SHAPES_TYPE;
use crate::shards::physics::SHAPES_VAR_TYPE;
use crate::shards::physics::SHAPE_TYPE;
use crate::shards::physics::SHAPE_VAR_TYPE;
use crate::shards::physics::SIMULATION_TYPE;
use crate::core::deriveType;
use crate::core::registerShard;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::FLOAT4X4orS_TYPES;
use crate::types::InstanceData;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Type;
use crate::types::ANY_TYPES;
use crate::types::FLOAT4X2_TYPE;
use crate::types::FLOAT4X2_TYPES;
use crate::types::FLOAT4X4S_TYPE;
use crate::types::FLOAT4X4_TYPE;
use crate::types::FLOAT4X4_TYPES;
use crate::types::NONE_TYPES;
use crate::Shard;
use crate::Types;
use crate::Var;
use rapier3d::dynamics::RigidBodyBuilder;
use rapier3d::dynamics::{
  IntegrationParameters, JointSet, RigidBodyHandle, RigidBodySet, RigidBodyType,
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
use rapier3d::prelude::Collider;
use std::convert::TryInto;
use std::rc::Rc;

// TODO Major refactor to remove copy-pasta, man in C++ this would have been so easy for me... but.

lazy_static! {
  static ref PARAMETERS: Parameters = vec![
    (
      cstr!("Shapes"),
      shccstr!("The shape or shapes of this rigid body."),
      vec![*SHAPE_VAR_TYPE, *SHAPES_VAR_TYPE, common_type::none]
    )
      .into(),
    (
      cstr!("Position"),
      shccstr!("The initial position of this rigid body. Can be updated in the case of a kinematic rigid body."),
      vec![common_type::float3, common_type::float3_var, common_type::float3s, common_type::float3s_var]
    )
      .into(),
    (
      cstr!("Rotation"),
      shccstr!("The initial rotation of this rigid body. Either axis angles in radians Float3 or a quaternion Float4. Can be updated in the case of a kinematic rigid body."),
      vec![common_type::float4, common_type::float4_var, common_type::float4s, common_type::float4s_var]
    )
      .into(),
    (
      cstr!("Name"),
      shccstr!("The optional name of the variable that will be exposed to identify, apply forces (if dynamic) and control this rigid body."),
      vec![common_type::string, common_type::none]
    )
      .into()
  ];
}

impl Default for RigidBody {
  fn default() -> Self {
    let mut r = RigidBody {
      simulation_var: ParamVar::new(().into()),
      shape_var: ParamVar::new(().into()),
      rigid_bodies: Vec::new(),
      colliders: Vec::new(),
      position: ParamVar::new((0.0, 0.0, 0.0).into()),
      rotation: ParamVar::new((0.0, 0.0, 0.0, 1.0).into()),
      user_data: 0,
    };
    r.simulation_var.set_name("Physics.Simulation");
    r
  }
}

impl RigidBody {
  fn _set_param(&mut self, index: i32, value: &Var) {
    match index {
      0 => self.shape_var.set_param(value),
      1 => self.position.set_param(value),
      2 => self.rotation.set_param(value),
      _ => unreachable!(),
    }
  }

  fn _get_param(&mut self, index: i32) -> Var {
    match index {
      0 => self.shape_var.get_param(),
      1 => self.position.get_param(),
      2 => self.rotation.get_param(),
      _ => unreachable!(),
    }
  }

  fn _compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // we need to derive the position parameter type, because if it's a sequence we should create multiple RigidBodies
    // TODO we should also use input type to determine the output if dynamic
    let pvt = deriveType(&self.position.get_param(), data);
    if pvt.0 == common_type::float3 {
      Ok(*FLOAT4X4_TYPE)
    } else if pvt.0 == common_type::float3s {
      Ok(*FLOAT4X4S_TYPE)
    } else {
      Err("Physics.RigidBody: Invalid position or rotation parameter type, if one is a sequence the other must also be a sequence")
    }
  }

  fn _cleanup(&mut self) {
    if let Some(simulation) = self.simulation_var.try_get() {
      let simulation =
        Var::from_object_ptr_mut_ref::<Simulation>(simulation, &SIMULATION_TYPE).unwrap();
      for rigid_body in &self.rigid_bodies {
        // this removes both RigidBodies and colliders attached.
        simulation.bodies.remove(
          *rigid_body,
          &mut simulation.islands_manager,
          &mut simulation.colliders,
          &mut simulation.joints,
        );
      }
    }

    // clear, it's crucial as it signals we need to re-populate when running again
    self.rigid_bodies.clear();
    self.colliders.clear();

    self.simulation_var.cleanup();
    self.shape_var.cleanup();
    self.position.cleanup();
    self.rotation.cleanup();
  }

  fn _warmup(&mut self, context: &Context, user_data: u128) {
    self.simulation_var.warmup(context);
    self.shape_var.warmup(context);
    self.position.warmup(context);
    self.rotation.warmup(context);
    self.user_data = user_data;
  }

  // Utility - makes a single RigidBody
  fn make_rigid_body<'a>(
    simulation: &mut Simulation,
    user_data: u128,
    status: RigidBodyType,
    p: &Var,
    r: &Var,
  ) -> Result<RigidBodyHandle, &'a str> {
    let pos = {
      if p.is_none() {
        Vector3::new(0.0, 0.0, 0.0)
      } else {
        let (tx, ty, tz): (f32, f32, f32) = p.try_into()?;
        Vector3::new(tx, ty, tz)
      }
    };

    let iso = {
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
    };

    let mut rigid_body = RigidBodyBuilder::new(status).position(iso).build();
    rigid_body.user_data = user_data;
    Ok(simulation.bodies.insert(rigid_body))
  }

  // Utility - makes a collider or a compound from preset variables shapes
  fn make_collider(
    simulation: &mut Simulation,
    user_data: u128,
    shape: Var,
    rigid_body: RigidBodyHandle,
  ) -> Result<ColliderHandle, &str> {
    let shapeInfo = Var::from_object_ptr_mut_ref::<BaseShape>(shape, &SHAPE_TYPE)?;
    let shape = shapeInfo.shape.as_ref().unwrap().clone();
    let mut collider = ColliderBuilder::new(shape)
      .position(shapeInfo.position.unwrap())
      .build();
    collider.user_data = user_data;
    Ok(
      simulation
        .colliders
        .insert_with_parent(collider, rigid_body, &mut simulation.bodies),
    )
  }

  // make and populate in the self.rigid_bodies list a new RigidBody
  // this is called every frame so it must check if empty, if not empty just passthrough
  fn populate_single(
    &mut self,
    status: RigidBodyType,
    p: &Var,
    r: &Var,
  ) -> Result<(&[RigidBodyHandle], Var, Var), &str> {
    if self.rigid_bodies.is_empty() {
      // init if array is empty
      let rigid_body = {
        // Mut borrow - this is repeated a lot sadly - TODO figure out
        let simulation =
          Var::from_object_ptr_mut_ref::<Simulation>(self.simulation_var.get(), &SIMULATION_TYPE)?;
        let rigid_body = Self::make_rigid_body(simulation, self.user_data, status, p, r)?;
        self.rigid_bodies.push(rigid_body);
        rigid_body
      };

      let shape = self.shape_var.get();
      if shape.is_seq() {
        let shapes: Seq = shape.try_into().unwrap();
        for shape in shapes {
          // Mut borrow - this is repeated a lot sadly - TODO figure out
          let simulation = Var::from_object_ptr_mut_ref::<Simulation>(
            self.simulation_var.get(),
            &SIMULATION_TYPE,
          )?;
          self.colliders.push(Self::make_collider(
            simulation,
            self.user_data,
            shape,
            rigid_body,
          )?);
        }
      } else {
        // Mut borrow - this is repeated a lot sadly - TODO figure out
        let simulation =
          Var::from_object_ptr_mut_ref::<Simulation>(self.simulation_var.get(), &SIMULATION_TYPE)?;
        self.colliders.push(Self::make_collider(
          simulation,
          self.user_data,
          shape,
          rigid_body,
        )?);
      }
    }
    Ok((self.rigid_bodies.as_slice(), *p, *r))
  }

  // make and populate in the self.rigid_bodies list multiple RigidBodies
  // this is called every frame so it must check if empty, if not empty just passthrough
  fn populate_multi(
    &mut self,
    status: RigidBodyType,
    p: &Var,
    r: &Var,
  ) -> Result<(&[RigidBodyHandle], Var, Var), &str> {
    if self.rigid_bodies.is_empty() {
      let p: Seq = p.try_into()?;
      for (idx, p) in p.iter().enumerate() {
        // init if array is empty
        let rigid_body = {
          // Mut borrow - this is repeated a lot sadly - TODO figure out
          let simulation = Var::from_object_ptr_mut_ref::<Simulation>(
            self.simulation_var.get(),
            &SIMULATION_TYPE,
          )?;
          if r.is_seq() {
            let r: Seq = r.try_into()?;
            let rigid_body =
              Self::make_rigid_body(simulation, self.user_data, status, &p, &r[idx])?;
            self.rigid_bodies.push(rigid_body);
            rigid_body
          } else {
            let rigid_body = Self::make_rigid_body(simulation, self.user_data, status, &p, r)?;
            self.rigid_bodies.push(rigid_body);
            rigid_body
          }
        };

        let shape = self.shape_var.get();
        if shape.is_seq() {
          let shapes: Seq = shape.try_into().unwrap();
          for shape in shapes {
            // Mut borrow - this is repeated a lot sadly - TODO figure out
            let simulation = Var::from_object_ptr_mut_ref::<Simulation>(
              self.simulation_var.get(),
              &SIMULATION_TYPE,
            )?;
            self.colliders.push(Self::make_collider(
              simulation,
              self.user_data,
              shape,
              rigid_body,
            )?);
          }
        } else {
          // Mut borrow - this is repeated a lot sadly - TODO figure out
          let simulation = Var::from_object_ptr_mut_ref::<Simulation>(
            self.simulation_var.get(),
            &SIMULATION_TYPE,
          )?;
          self.colliders.push(Self::make_collider(
            simulation,
            self.user_data,
            shape,
            rigid_body,
          )?);
        }
      }
    }
    Ok((self.rigid_bodies.as_slice(), *p, *r))
  }

  fn _populate(&mut self, status: RigidBodyType) -> Result<(&[RigidBodyHandle], Var, Var), &str> {
    let p = &self.position.get();
    let r = &self.rotation.get();
    if p.is_seq() {
      self.populate_multi(status, p, r)
    } else {
      self.populate_single(status, p, r)
    }
  }
}

struct StaticRigidBody {
  rb: Rc<RigidBody>,
  self_obj: ParamVar,
  exposing: ExposedTypes,
}

impl Default for StaticRigidBody {
  fn default() -> Self {
    StaticRigidBody {
      rb: Rc::new(RigidBody::default()),
      self_obj: ParamVar::new(().into()),
      exposing: Vec::new(),
    }
  }
}

impl Shard for StaticRigidBody {
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
      0 | 1 | 2 => Rc::get_mut(&mut self.rb).unwrap()._set_param(index, value),
      3 => {
        if !value.is_none() {
          self.self_obj.set_name(value.try_into().unwrap())
        }
      }
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 | 1 | 2 => Rc::get_mut(&mut self.rb).unwrap()._get_param(index),
      3 => {
        if self.self_obj.is_variable() {
          self.self_obj.get_name().into()
        } else {
          ().into()
        }
      }
      _ => unreachable!(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.self_obj.is_variable() {
      self.exposing.clear();
      let exp_info = ExposedInfo {
        exposedType: *RIGIDBODY_TYPE,
        name: self.self_obj.get_name(),
        help: shccstr!("The exposed kinematic rigid body."),
        ..ExposedInfo::default()
      };
      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn cleanup(&mut self) {
    self.self_obj.cleanup();
    Rc::get_mut(&mut self.rb).unwrap()._cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.self_obj.warmup(context);
    let obj = Var::new_object(&self.rb, &RIGIDBODY_TYPE);
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set(obj);
    }
    Rc::get_mut(&mut self.rb)
      .unwrap()
      ._warmup(context, user_data);
    Ok(())
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    // just hit populate, it will be a noop if already populated, nothing else to do here
    Rc::get_mut(&mut self.rb)
      .unwrap()
      ._populate(RigidBodyType::Static)?;
    Ok(*input)
  }
}

struct DynamicRigidBody {
  rb: Rc<RigidBody>,
  self_obj: ParamVar,
  exposing: ExposedTypes,
  output: Seq,
}

impl Default for DynamicRigidBody {
  fn default() -> Self {
    let mut drb = DynamicRigidBody {
      rb: Rc::new(RigidBody::default()),
      self_obj: ParamVar::new(().into()),
      exposing: Vec::new(),
      output: Seq::new(),
    };
    drb.output.set_len(4);
    drb
  }
}

impl Shard for DynamicRigidBody {
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
    &FLOAT4X4orS_TYPES
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    Rc::get_mut(&mut self.rb).unwrap()._compose(data)
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 | 1 | 2 => Rc::get_mut(&mut self.rb).unwrap()._set_param(index, value),
      3 => {
        if !value.is_none() {
          self.self_obj.set_name(value.try_into().unwrap())
        }
      }
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 | 1 | 2 => Rc::get_mut(&mut self.rb).unwrap()._get_param(index),
      3 => {
        if self.self_obj.is_variable() {
          self.self_obj.get_name().into()
        } else {
          ().into()
        }
      }
      _ => unreachable!(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.self_obj.is_variable() {
      self.exposing.clear();
      let exp_info = ExposedInfo {
        exposedType: *RIGIDBODY_TYPE,
        name: self.self_obj.get_name(),
        help: shccstr!("The exposed dynamic rigid body."),
        ..ExposedInfo::default()
      };
      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn cleanup(&mut self) {
    self.self_obj.cleanup();
    Rc::get_mut(&mut self.rb).unwrap()._cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.self_obj.warmup(context);
    let obj = Var::new_object(&self.rb, &RIGIDBODY_TYPE);
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set(obj);
    }
    Rc::get_mut(&mut self.rb)
      .unwrap()
      ._warmup(context, user_data);
    Ok(())
  }

  fn activate(&mut self, _: &Context, _input: &Var) -> Result<Var, &str> {
    // dynamic will use parameter position and rotation only the first time
    // after that will be driven by the physics engine so what we do is get the new matrix and output it
    let rbData = Rc::get_mut(&mut self.rb).unwrap();
    let sim_var = rbData.simulation_var.get();
    let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
    let (rbs, _, _) = rbData._populate(RigidBodyType::Dynamic)?;
    if rbs.len() == 1 {
      let rb = simulation.bodies.get(rbs[0]).unwrap();
      let mat: Matrix4<f32> = rb.position().to_matrix();
      fill_seq_from_mat4(&mut self.output, &mat);
      Ok(self.output.as_ref().into())
    } else {
      // TODO multiple RigidBodies
      Err("Unsupported RigidBody sequence")
    }
  }
}

struct KinematicRigidBody {
  rb: Rc<RigidBody>,
  self_obj: ParamVar,
  exposing: ExposedTypes,
  output: Seq,
}

impl Default for KinematicRigidBody {
  fn default() -> Self {
    let mut drb = KinematicRigidBody {
      rb: Rc::new(RigidBody::default()),
      self_obj: ParamVar::new(().into()),
      exposing: Vec::new(),
      output: Seq::new(),
    };
    drb.output.set_len(4);
    drb
  }
}

impl Shard for KinematicRigidBody {
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
    &FLOAT4X4orS_TYPES
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    Rc::get_mut(&mut self.rb).unwrap()._compose(data)
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 | 1 | 2 => Rc::get_mut(&mut self.rb).unwrap()._set_param(index, value),
      3 => {
        if !value.is_none() {
          self.self_obj.set_name(value.try_into().unwrap())
        }
      }
      _ => unreachable!(),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 | 1 | 2 => Rc::get_mut(&mut self.rb).unwrap()._get_param(index),
      3 => {
        if self.self_obj.is_variable() {
          self.self_obj.get_name().into()
        } else {
          ().into()
        }
      }
      _ => unreachable!(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.self_obj.is_variable() {
      self.exposing.clear();
      let exp_info = ExposedInfo {
        exposedType: *RIGIDBODY_TYPE,
        name: self.self_obj.get_name(),
        help: shccstr!("The exposed kinematic rigid body."),
        ..ExposedInfo::default()
      };
      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn cleanup(&mut self) {
    self.self_obj.cleanup();
    Rc::get_mut(&mut self.rb).unwrap()._cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.self_obj.warmup(context);
    let obj = Var::new_object(&self.rb, &RIGIDBODY_TYPE);
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set(obj);
    }
    Rc::get_mut(&mut self.rb)
      .unwrap()
      ._warmup(context, user_data);
    Ok(())
  }

  fn activate(&mut self, _: &Context, _input: &Var) -> Result<Var, &str> {
    // kinematic pos/rot will be updated every frame by reading the parameters which should be variables
    // it will also output a properly interpolated matrix
    let rbData = Rc::get_mut(&mut self.rb).unwrap();
    let sim_var = rbData.simulation_var.get();
    let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
    let (rbs, p, r) = rbData._populate(RigidBodyType::KinematicPositionBased)?; // TODO KinematicVelocityBased as well
    if rbs.len() == 1 {
      let rb = simulation.bodies.get_mut(rbs[0]).unwrap();
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
    } else {
      Err("Unsupported RigidBody sequence")
    }
  }
}

pub fn registerShards() {
  registerShard::<DynamicRigidBody>();
  registerShard::<StaticRigidBody>();
  registerShard::<KinematicRigidBody>();
}
