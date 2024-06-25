/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::fill_seq_from_mat4;
use crate::BaseShape;
use crate::RigidBody;
use crate::Simulation;
use crate::POSITIONS_TYPES_SLICE;
use crate::RIGIDBODY_TYPE;
use crate::ROTATIONS_TYPES_SLICE;
use crate::SHAPES_TYPES_SLICE;
use crate::SHAPE_TYPE;
use crate::SIMULATION_NAME;
use crate::SIMULATION_TYPE;
use rapier3d::dynamics::RigidBodyBuilder;
use rapier3d::dynamics::{RigidBodyHandle, RigidBodyType};
use rapier3d::geometry::{ColliderBuilder, ColliderHandle};
use rapier3d::na::Isometry3;
use rapier3d::na::{Matrix4, Quaternion, Translation, UnitQuaternion, Vector3};
use rapier3d::prelude::ActiveEvents;
use rapier3d::prelude::Group;
use rapier3d::prelude::InteractionGroups;
use shards::core::register_shard;
use shards::shard::ParameterSet;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::FLOAT4X4orS_TYPES;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::Seq;
use shards::types::SeqVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::WireState;
use shards::types::ANY_TYPES;
use shards::types::FLOAT4X4S_TYPE;
use shards::types::FLOAT4X4_TYPE;
use shards::types::NONE_TYPES;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::util;
use shards::SHType_None;
use shards::SHType_Seq;
use std::convert::TryInto;

// TODO Major refactor to remove copy-pasta, man in C++ this would have been so easy for me... but.

lazy_static! {
  static ref VAR_TYPES: Types = vec![common_type::any_var];
  static ref INT_VAR_OR_NONE: Types =
    vec![common_type::int2, common_type::int2_var, common_type::none];
  static ref BOOL_TYPES: Vec<Type> = vec![common_type::bool];
  static ref CONSTRAINT_TYPE: Type = Type::seq(&BOOL_TYPES);
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
  #[shard_param(
    "Collision",
    "Handle collisions with this object.",
    SHARDS_OR_NONE_TYPES
  )]
  collision: ShardsVar,
  /// From rapier docs:
  /// An interaction is allowed between two filters `a` and `b` when two conditions
  /// are met simultaneously:
  /// - The groups membership of `a` has at least one bit set to `1` in common with the groups filter of `b`.
  /// - The groups membership of `b` has at least one bit set to `1` in common with the groups filter of `a`.
  #[shard_param(
    "SolverGroup",
    "Solver group (Membership, Filter) pair for contact forces.",
    INT_VAR_OR_NONE
  )]
  solver_group: ParamVar,
  #[shard_param(
    "CollisionGroup",
    "Collision group (Membership, Filter) pair for collision events.",
    INT_VAR_OR_NONE
  )]
  collision_group: ParamVar,
  #[shard_param("Tag", "Tag attached to this collider", [common_type::any])]
  user_data: ParamVar,
}

struct RigidBodyParams<'a> {
  allow_tsl: &'a ClonedVar,
  allow_rot: &'a ClonedVar,
}

impl RigidBody {
  fn warmup(&mut self, rapier_user_data: u128) {
    self.rapier_user_data = rapier_user_data;
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
  fn activate_single(
    &mut self,
    base: &RigidBodyBase,
    rb_params: Option<RigidBodyParams>,
    type_: RigidBodyType,
    p: &Var,
    r: &Var,
  ) -> Result<(&[RigidBodyHandle], Var, Var), &'static str> {
    let sim = &base.simulation;
    let shape = &base.shape;

    if self.rigid_bodies.is_empty() {
      // Mut borrow - this is repeated a lot sadly - TODO figure out
      let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;

      // init if array is empty
      let rigid_body = {
        let rigid_body =
          Self::make_rigid_body(simulation, &rb_params, self.rapier_user_data, type_, p, r)?;
        self.rigid_bodies.push(rigid_body);
        rigid_body
      };

      let shape_var = shape.get();
      if shape_var.is_seq() {
        let shapes: Seq = shape_var.try_into().unwrap();
        for shape in shapes.iter() {
          self.colliders.push(Self::make_collider(
            simulation,
            base,
            self.rapier_user_data,
            shape,
            rigid_body,
          )?);
        }
      } else {
        self.colliders.push(Self::make_collider(
          simulation,
          base,
          self.rapier_user_data,
          shape_var,
          rigid_body,
        )?);
      }
    }
    Ok((self.rigid_bodies.as_slice(), *p, *r))
  }

  // make and populate in the self.rigid_bodies list multiple RigidBodies
  // this is called every frame so it must check if empty, if not empty just passthrough
  fn activate_multi(
    &mut self,
    base: &RigidBodyBase,
    rb_params: Option<RigidBodyParams>,
    type_: RigidBodyType,
    p: &Var,
    r: &Var,
  ) -> Result<(&[RigidBodyHandle], Var, Var), &'static str> {
    let sim = &base.simulation;
    let shape = &base.shape;

    if self.rigid_bodies.is_empty() {
      // Mut borrow - this is repeated a lot sadly - TODO figure out
      let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;

      let p: Seq = p.try_into()?;
      for (idx, p) in p.iter().enumerate() {
        // init if array is empty
        let rigid_body = {
          if r.is_seq() {
            let r: Seq = r.try_into()?;
            let rigid_body = Self::make_rigid_body(
              simulation,
              &rb_params,
              self.rapier_user_data,
              type_,
              &p,
              &r[idx],
            )?;
            self.rigid_bodies.push(rigid_body);
            rigid_body
          } else {
            let rigid_body =
              Self::make_rigid_body(simulation, &rb_params, self.rapier_user_data, type_, &p, r)?;
            self.rigid_bodies.push(rigid_body);
            rigid_body
          }
        };

        let shape = shape.get();
        if shape.is_seq() {
          let shapes: Seq = shape.try_into().unwrap();
          for shape in shapes.iter() {
            self.colliders.push(Self::make_collider(
              simulation,
              base,
              self.rapier_user_data,
              shape,
              rigid_body,
            )?);
          }
        } else {
          // Mut borrow - this is repeated a lot sadly - TODO figure out
          let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim.get(), &SIMULATION_TYPE)?;
          self.colliders.push(Self::make_collider(
            simulation,
            base,
            self.rapier_user_data,
            shape,
            rigid_body,
          )?);
        }
      }
    }
    Ok((self.rigid_bodies.as_slice(), *p, *r))
  }

  fn activate(
    &mut self,
    context: &Context,
    base: &RigidBodyBase,
    rb_params: Option<RigidBodyParams>,
    type_: RigidBodyType,
  ) -> Result<(&[RigidBodyHandle], Var, Var), &'static str> {
    let p = *base.position.get();
    let r: shards::SHVar = *base.rotation.get();

    {
      self.want_collision_events = base.has_collision_callback();
      let mut events = self.collision_events.borrow_mut();
      for evt in events.iter() {
        let mut output = Var::default();
        let state = base
          .collision
          .activate(context, &evt.tag.0, &mut output);
        if state == WireState::Error {
          return Err("Physics.RigidBody: Error in collision callback");
        }
      }
      events.clear();

      // Update user data attached to the rigid body
      self.user_data.assign(base.user_data.get());
    }

    if p.is_seq() {
      self.activate_multi(base, rb_params, type_, &p, &r)
    } else {
      self.activate_single(base, rb_params, type_, &p, &r)
    }
  }

  // Utility - makes a single RigidBody
  fn make_rigid_body(
    simulation: &mut Simulation,
    rb_params: &Option<RigidBodyParams>,
    user_data: u128,
    type_: RigidBodyType,
    p: &Var,
    r: &Var,
  ) -> Result<RigidBodyHandle, &'static str> {
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

    let mut builder = RigidBodyBuilder::new(type_).position(iso);

    if let Some(rb_params) = rb_params {
      if let Ok(seq) = rb_params.allow_tsl.0.as_seq() {
        if seq.len() != 3 {
          return Err("Translation Lock requires 3 boolean values");
        }
        builder = builder.enabled_translations(
          (&seq[0]).try_into()?,
          (&seq[1]).try_into()?,
          (&seq[2]).try_into()?,
        );
      }

      if let Ok(seq) = rb_params.allow_rot.0.as_seq() {
        if seq.len() != 3 {
          return Err("Rotation Lock requires 3 boolean values");
        }
        builder = builder.enabled_rotations(
          (&seq[0]).try_into()?,
          (&seq[1]).try_into()?,
          (&seq[2]).try_into()?,
        );
      }
    }

    let mut rigid_body = builder.build();
    rigid_body.user_data = user_data;
    Ok(simulation.bodies.insert(rigid_body))
  }

  fn make_interaction_groups(bits: (u32, u32)) -> InteractionGroups {
    InteractionGroups::new(
      Group::from_bits_truncate(bits.0),
      Group::from_bits_truncate(bits.1),
    )
  }

  // Utility - makes a collider or a compound from preset variables shapes
  fn make_collider(
    simulation: &mut Simulation,
    base: &RigidBodyBase,
    user_data: u128,
    shape: &Var,
    rigid_body: RigidBodyHandle,
  ) -> Result<ColliderHandle, &'static str> {
    let shape_info = Var::from_object_ptr_mut_ref::<BaseShape>(&shape, &SHAPE_TYPE)?;
    let shape = shape_info.shape.as_ref().unwrap().clone();

    let mut builder = ColliderBuilder::new(shape).position(shape_info.position.unwrap());
    if base.has_collision_callback() {
      builder = builder.active_events(ActiveEvents::COLLISION_EVENTS);
    }

    if let Ok(bits) = TryInto::<(u32, u32)>::try_into(base.solver_group.get()) {
      builder = builder.solver_groups(Self::make_interaction_groups(bits));
    }
    if let Ok(bits) = TryInto::<(u32, u32)>::try_into(base.collision_group.get()) {
      builder = builder.collision_groups(Self::make_interaction_groups(bits));
    }

    if let Some(mass) = shape_info.mass {
      builder = builder.mass(mass);
    }
    let mut collider = builder.build();
    collider.user_data = user_data;

    let collider_handle =
      simulation
        .colliders
        .insert_with_parent(collider, rigid_body, &mut simulation.bodies);

    Ok(collider_handle)
  }
}

impl Default for RigidBodyBase {
  fn default() -> Self {
    Self {
      simulation: ParamVar::new_named(SIMULATION_NAME),
      shape: ParamVar::default(),
      position: ParamVar::new((0.0, 0.0, 0.0).into()),
      rotation: ParamVar::new((0.0, 0.0, 0.0, 1.0).into()),
      collision: ShardsVar::default(),
      user_data: ParamVar::default(),
      solver_group: ParamVar::default(),
      collision_group: ParamVar::default(),
    }
  }
}

impl RigidBodyBase {
  fn has_collision_callback(&self) -> bool {
    return !self.collision.is_empty();
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    // we need to derive the position parameter type, because if it's a sequence we should create multiple RigidBodies
    // TODO we should also use input type to determine the output if dynamic

    {
      let mut data_copy = *data;
      data_copy.inputType = common_type::any;
      let cr = self.collision.compose(&data_copy)?;
    }

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
#[shard_info("Physics.StaticBody", "A static rigid body that does not move.")]
struct StaticRigidBodyShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param_set]
  base: RigidBodyBase,
  #[shard_param("Name", "The optional name of the variable that will be exposed to identify, apply forces (if dynamic) and control this rigid body.", VAR_TYPES)]
  self_obj: ParamVar,
  rb: RigidBody,
  exposing: ExposedTypes,
}

impl Default for StaticRigidBodyShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      base: RigidBodyBase::default(),
      self_obj: ParamVar::default(),
      // self_obj: ParamVar::default(),
      rb: RigidBody::default(),
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

    let obj = unsafe { Var::new_object_from_ptr(&self.rb, &RIGIDBODY_TYPE) };
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set_cloning(&obj);
    }
    self.rb.warmup(user_data);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.rb.cleanup(&self.base.simulation);
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    self.base.compose(data)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    // just hit populate, it will be a noop if already populated, nothing else to do here
    self
      .rb
      .activate(context, &self.base, None, RigidBodyType::Fixed)?;
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
#[shard_info("Physics.DynamicBody", "A dynamic fully simulated rigid body.")]
struct DynamicRigidBodyShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param_set]
  base: RigidBodyBase,
  #[shard_param("Name", "The optional name of the variable that will be exposed to identify, apply forces (if dynamic) and control this rigid body.", VAR_TYPES)]
  self_obj: ParamVar,
  #[shard_param(
    "AllowTranslation",
    "Translation contraints for this object [x y z].",
    [common_type::none, *CONSTRAINT_TYPE]
  )]
  allow_tsl: ClonedVar,
  #[shard_param(
    "AllowRotation",
    "Rotation contraints for this object [x y z].",
    [common_type::none, *CONSTRAINT_TYPE]
  )]
  allow_rot: ClonedVar,
  rb: RigidBody,
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
      allow_tsl: ClonedVar::default(),
      allow_rot: ClonedVar::default(),
      self_obj: ParamVar::default(),
      rb: RigidBody::default(),
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

    let obj = unsafe { Var::new_object_from_ptr(&self.rb as *const _, &RIGIDBODY_TYPE) };
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set_cloning(&obj);
    }
    self.rb.warmup(user_data);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.rb.cleanup(&self.base.simulation);
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    self.base.compose(data)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    // dynamic will use parameter position and rotation only the first time
    // after that will be driven by the physics engine so what we do is get the new matrix and output it
    let sim_var = self.base.simulation.get();
    let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
    let (rbs, _, _) = self.rb.activate(
      context,
      &self.base,
      Some(RigidBodyParams {
        allow_tsl: &self.allow_tsl,
        allow_rot: &self.allow_rot,
      }),
      RigidBodyType::Dynamic,
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
#[shard_info("Physics.KinematicBody", "A kinematic rigid body that can be controlled by the user and interacts with the physics simulation.")]
struct KinematicRigidBodyShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param_set]
  base: RigidBodyBase,
  #[shard_param("Name", "The optional name of the variable that will be exposed to identify, apply forces (if dynamic) and control this rigid body.", VAR_TYPES)]
  self_obj: ParamVar,
  rb: RigidBody,
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
      self_obj: ParamVar::default(),
      rb: RigidBody::default(),
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

    let obj = unsafe { Var::new_object_from_ptr(&self.rb, &RIGIDBODY_TYPE) };
    let user_data: u128 =
      { unsafe { obj.payload.__bindgen_anon_1.__bindgen_anon_1.objectValue as u128 } };
    if self.self_obj.is_variable() {
      self.self_obj.set_cloning(&obj);
    }
    self.rb.warmup(user_data);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.rb.cleanup(&self.base.simulation);
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    self.base.compose(data)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    // kinematic pos/rot will be updated every frame by reading the parameters which should be variables
    // it will also output a properly interpolated matrix
    let sim_var = self.base.simulation.get();
    let simulation = Var::from_object_ptr_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
    // TODO KinematicVelocityBased as well
    let (rbs, p, r) = self.rb.activate(
      context,
      &self.base,
      None,
      RigidBodyType::KinematicPositionBased,
    )?;
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
