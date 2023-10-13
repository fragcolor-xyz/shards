/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::fill_seq_from_mat4;
use crate::RigidBody;
use crate::SIMULATION_NAME;
use crate::SIMULATION_NAME_CSTR;
use rapier3d::prelude::MassProperties;
use shards::core::deriveType;
use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::shard::ParameterSet;
use shards::shard::Shard;
use shards::types::Types;
use shards::util;
use shards::SHType;
use shards::SHType_None;
use shards::SHType_Seq;

use crate::BaseShape;
use crate::Simulation;
use crate::EXPOSED_SIMULATION;
use crate::POSITIONS_TYPES_SLICE;
use crate::RIGIDBODY_TYPE;
use crate::ROTATIONS_TYPES_SLICE;

use crate::SHAPES_TYPES_SLICE;

use crate::SHAPE_TYPE;

use crate::SIMULATION_TYPE;
use shards::types::common_type;

use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::FLOAT4X4orS_TYPES;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Seq;
use shards::types::Type;
use shards::types::ANY_TYPES;

use shards::types::FLOAT4X4S_TYPE;
use shards::types::FLOAT4X4_TYPE;

use shards::shard::LegacyShard;
use shards::types::NONE_TYPES;
use shards::types::STRING_OR_NONE_SLICE;

use rapier3d::dynamics::RigidBodyBuilder;
use rapier3d::dynamics::{RigidBodyHandle, RigidBodyType};
use rapier3d::geometry::{ColliderBuilder, ColliderHandle};
use shards::types::Var;

use rapier3d::na::Isometry3;

use rapier3d::na::{Matrix4, Quaternion, Translation, UnitQuaternion, Vector3};

use std::convert::TryInto;
use std::rc::Rc;

// TODO Major refactor to remove copy-pasta, man in C++ this would have been so easy for me... but.

lazy_static! {
  static ref VAR_TYPES: Types = vec![common_type::any_var];
}

#[derive(param_set)]
pub struct RigidBodyBase {
  #[shard_warmup]
  simulation: ParamVar,
  #[shard_param(
    "Shapes",
    "The shape or shapes of this rigid body.",
    SHAPES_TYPES_SLICE
  )]
  shape: ParamVar,
  #[shard_param("Position", "The initial position of this rigid body. Can be updated in the case of a kinematic rigid body.", POSITIONS_TYPES_SLICE)]
  position: ParamVar,
  #[shard_param("Rotation", "The initial rotation of this rigid body. Either axis angles in radians Float3 or a quaternion Float4. Can be updated in the case of a kinematic rigid body.", ROTATIONS_TYPES_SLICE)]
  rotation: ParamVar,
  // #[shard_param("Data", "Data attached to this collider", [common_type::any_table, common_type::any_table_var])]
  // user_data: ParamVar,
}

impl RigidBody {
  fn warmup(&mut self, user_data: u128) {
    self.user_data = user_data;
  }
  fn cleanup(&mut self, sim: &ParamVar) {
    if let Some(simulation) = sim.try_get() {
      let simulation =
        Var::from_object_ptr_mut_ref::<Simulation>(simulation, &SIMULATION_TYPE).unwrap();
      for rigid_body in &self.rigid_bodies {
        // this removes both RigidBodies and colliders attached.
        simulation.bodies.remove(
          *rigid_body,
          &mut simulation.islands_manager,
          &mut simulation.colliders,
          &mut simulation.impulse_joints,
          &mut simulation.multibody_joints,
          true,
        );
      }
    }

    // clear, it's crucial as it signals we need to re-populate when running again
    self.rigid_bodies.clear();
    self.colliders.clear();
  }

  // make and populate in the self.rigid_bodies list a new RigidBody
  // this is called every frame so it must check if empty, if not empty just passthrough
  fn populate_single(
    &mut self,
    sim: &ParamVar,
    type_: RigidBodyType,
    shape: &ParamVar,
    p: &Var,
    r: &Var,
  ) -> Result<(&[RigidBodyHandle], Var, Var), &str> {
    if self.rigid_bodies.is_empty() {
      // init if array is empty
      let rigid_body = {
        // Mut borrow - this is repeated a lot sadly - TODO figure out
        let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;
        let rigid_body = Self::make_rigid_body(simulation, self.user_data, type_, p, r)?;
        self.rigid_bodies.push(rigid_body);
        rigid_body
      };

      let shape_var = shape.get();
      if shape_var.is_seq() {
        let shapes: Seq = shape_var.try_into().unwrap();
        for shape in shapes.iter() {
          // Mut borrow - this is repeated a lot sadly - TODO figure out
          let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;
          self.colliders.push(Self::make_collider(
            simulation,
            self.user_data,
            shape,
            rigid_body,
          )?);
        }
      } else {
        // Mut borrow - this is repeated a lot sadly - TODO figure out
        let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;
        self.colliders.push(Self::make_collider(
          simulation,
          self.user_data,
          shape_var,
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
    sim: &ParamVar,
    type_: RigidBodyType,
    shape: &ParamVar,
    p: &Var,
    r: &Var,
  ) -> Result<(&[RigidBodyHandle], Var, Var), &str> {
    if self.rigid_bodies.is_empty() {
      let p: Seq = p.try_into()?;
      for (idx, p) in p.iter().enumerate() {
        // init if array is empty
        let rigid_body = {
          // Mut borrow - this is repeated a lot sadly - TODO figure out
          let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;
          if r.is_seq() {
            let r: Seq = r.try_into()?;
            let rigid_body = Self::make_rigid_body(simulation, self.user_data, type_, &p, &r[idx])?;
            self.rigid_bodies.push(rigid_body);
            rigid_body
          } else {
            let rigid_body = Self::make_rigid_body(simulation, self.user_data, type_, &p, r)?;
            self.rigid_bodies.push(rigid_body);
            rigid_body
          }
        };

        let shape = shape.get();
        if shape.is_seq() {
          let shapes: Seq = shape.try_into().unwrap();
          for shape in shapes.iter() {
            // Mut borrow - this is repeated a lot sadly - TODO figure out
            let simulation =
              Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;
            self.colliders.push(Self::make_collider(
              simulation,
              self.user_data,
              shape,
              rigid_body,
            )?);
          }
        } else {
          // Mut borrow - this is repeated a lot sadly - TODO figure out
          let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;
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

  fn populate(
    &mut self,
    sim: &ParamVar,
    type_: RigidBodyType,
    shape: &ParamVar,
    position: &ParamVar,
    rotation: &ParamVar,
  ) -> Result<(&[RigidBodyHandle], Var, Var), &str> {
    let p = *position.get();
    let r = *rotation.get();
    if p.is_seq() {
      self.populate_multi(sim, type_, shape, &p, &r)
    } else {
      self.populate_single(sim, type_, shape, &p, &r)
    }
  }

  // Utility - makes a single RigidBody
  fn make_rigid_body<'a>(
    simulation: &mut Simulation,
    user_data: u128,
    type_: RigidBodyType,
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

    let mut rigid_body = RigidBodyBuilder::new(type_).position(iso).build();
    rigid_body.user_data = user_data;
    Ok(simulation.bodies.insert(rigid_body))
  }

  // Utility - makes a collider or a compound from preset variables shapes
  fn make_collider<'a>(
    simulation: &'a mut Simulation,
    user_data: u128,
    shape: &Var,
    rigid_body: RigidBodyHandle,
  ) -> Result<ColliderHandle, &'a str> {
    let shape_info = Var::from_object_ptr_mut_ref::<BaseShape>(&shape, &SHAPE_TYPE)?;
    let shape = shape_info.shape.as_ref().unwrap().clone();

    let mut builder = ColliderBuilder::new(shape).position(shape_info.position.unwrap());
    if let Some(mass) = shape_info.mass {
      builder = builder.mass(mass);
    }
    let mut collider = builder.build();
    collider.user_data = user_data;

    Ok(
      simulation
        .colliders
        .insert_with_parent(collider, rigid_body, &mut simulation.bodies),
    )
  }
}

impl Default for RigidBodyBase {
  fn default() -> Self {
    Self {
      simulation: ParamVar::new_named(SIMULATION_NAME),
      shape: ParamVar::default(),
      position: ParamVar::new((0.0, 0.0, 0.0).into()),
      rotation: ParamVar::new((0.0, 0.0, 0.0, 1.0).into()),
      // user_data: ParamVar::default(),
    }
  }
}

impl RigidBodyBase {
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // we need to derive the position parameter type, because if it's a sequence we should create multiple RigidBodies
    // TODO we should also use input type to determine the output if dynamic

    let pvt = util::get_param_var_type(&data, &self.position)?;
    let pos_type: &Type = (&pvt).into();

    let pvt = util::get_param_var_type(&data, &self.rotation)?;
    let rot_type: &Type = (&pvt).into();

    let pos_is_seq = pos_type.basicType == SHType_Seq;
    let rot_is_seq = rot_type.basicType == SHType_Seq;
    if rot_type.basicType != SHType_None {
      if pos_is_seq != rot_is_seq {
        return Err("Physics.RigidBody: Invalid position or rotation parameter type, if one is a sequence the other must also be a sequence");
      }
    }

    if *pos_type == common_type::float3 {
      Ok(*FLOAT4X4_TYPE)
    } else if *pos_type == common_type::float3s {
      Ok(*FLOAT4X4S_TYPE)
    } else {
      Err("Physics.RigidBody: Invalid position parameter type")
    }
  }
}

#[derive(shards::shard)]
#[shard_info("Physics.StaticBody", "")]
struct StaticRigidBodyShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param_set]
  base: RigidBodyBase,
  #[shard_param("Name", "The optional name of the variable that will be exposed to identify, apply forces (if dynamic) and control this rigid body.", VAR_TYPES)]
  self_obj: ParamVar,
  // #[shard_warmup]
  // self_obj: ParamVar,
  rb: Rc<RigidBody>,
  exposing: ExposedTypes,
}

impl Default for StaticRigidBodyShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      base: RigidBodyBase::default(),
      self_obj: ParamVar::default(),
      // self_obj: ParamVar::default(),
      rb: Rc::new(RigidBody::default()),
      exposing: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for StaticRigidBodyShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    let obj = Var::new_object(&self.rb, &RIGIDBODY_TYPE);
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set_cloning(&obj);
    }
    Rc::get_mut(&mut self.rb).unwrap().warmup(user_data);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;
    Rc::get_mut(&mut self.rb).map(|x| x.cleanup(&self.base.simulation));
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    self.base.compose(data)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    // just hit populate, it will be a noop if already populated, nothing else to do here
    Rc::get_mut(&mut self.rb).unwrap().populate(
      &self.base.simulation,
      RigidBodyType::Fixed,
      &self.base.shape,
      &self.base.position,
      &self.base.rotation,
    )?;
    Ok(*input)
  }

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
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
}

#[derive(shards::shard)]
#[shard_info("Physics.DynamicBody", "")]
struct DynamicRigidBodyShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param_set]
  base: RigidBodyBase,
  #[shard_param("Name", "The optional name of the variable that will be exposed to identify, apply forces (if dynamic) and control this rigid body.", VAR_TYPES)]
  self_obj: ParamVar,
  // #[shard_warmup]
  // name: ParamVar,
  rb: Rc<RigidBody>,
  output: Seq,
  exposing: ExposedTypes,
}

impl Default for DynamicRigidBodyShard {
  fn default() -> Self {
    let mut output = Seq::new();
    output.set_len(4);
    Self {
      required: ExposedTypes::new(),
      base: RigidBodyBase::default(),
      // name: ParamVar::default(),
      self_obj: ParamVar::default(),
      rb: Rc::new(RigidBody::default()),
      output,
      exposing: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for DynamicRigidBodyShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &FLOAT4X4orS_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    let obj = Var::new_object(&self.rb, &RIGIDBODY_TYPE);
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set_cloning(&obj);
    }
    Rc::get_mut(&mut self.rb).unwrap().warmup(user_data);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;
    Rc::get_mut(&mut self.rb).map(|x| x.cleanup(&self.base.simulation));
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    self.base.compose(data)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    // dynamic will use parameter position and rotation only the first time
    // after that will be driven by the physics engine so what we do is get the new matrix and output it
    let rbData = Rc::get_mut(&mut self.rb).unwrap();
    let sim_var = self.base.simulation.get();
    let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
    let (rbs, _, _) = rbData.populate(
      &self.base.simulation,
      RigidBodyType::Dynamic,
      &self.base.shape,
      &self.base.position,
      &self.base.rotation,
    )?;
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

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
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
}

#[derive(shards::shard)]
#[shard_info("Physics.KinematicBody", "")]
struct KinematicRigidBodyShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param_set]
  base: RigidBodyBase,
  #[shard_param("Name", "The optional name of the variable that will be exposed to identify, apply forces (if dynamic) and control this rigid body.", VAR_TYPES)]
  self_obj: ParamVar,
  // #[shard_warmup]
  // name: ParamVar,
  rb: Rc<RigidBody>,
  output: Seq,
  exposing: ExposedTypes,
}

impl Default for KinematicRigidBodyShard {
  fn default() -> Self {
    let mut output = Seq::new();
    output.set_len(4);
    Self {
      required: ExposedTypes::new(),
      base: RigidBodyBase::default(),
      // name: ParamVar::default(),
      self_obj: ParamVar::default(),
      rb: Rc::new(RigidBody::default()),
      output,
      exposing: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for KinematicRigidBodyShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &FLOAT4X4orS_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    let obj = Var::new_object(&self.rb, &RIGIDBODY_TYPE);
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set_cloning(&obj);
    }
    Rc::get_mut(&mut self.rb).unwrap().warmup(user_data);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;
    Rc::get_mut(&mut self.rb).map(|x| x.cleanup(&self.base.simulation));
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    self.base.compose(data)
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    // kinematic pos/rot will be updated every frame by reading the parameters which should be variables
    // it will also output a properly interpolated matrix
    let rbData = Rc::get_mut(&mut self.rb).unwrap();
    let sim_var = self.base.simulation.get();
    let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
    let (rbs, p, r) = rbData.populate(
      &self.base.simulation,
      RigidBodyType::KinematicPositionBased,
      &self.base.shape,
      &self.base.position,
      &self.base.rotation,
    )?; // TODO KinematicVelocityBased as well
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

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
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
}

pub fn register_shards() {
  register_shard::<StaticRigidBodyShard>();
  register_shard::<DynamicRigidBodyShard>();
  register_shard::<KinematicRigidBodyShard>();
}
