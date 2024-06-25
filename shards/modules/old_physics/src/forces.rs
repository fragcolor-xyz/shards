/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::RigidBody;
use crate::Simulation;
use crate::RIGIDBODIES_TYPES_SLICE;
use crate::RIGIDBODY_TYPE;
use crate::SIMULATION_NAME;
use crate::SIMULATION_TYPE;
use rapier3d::na::Vector3;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use std::convert::TryInto;

lazy_static! {
  pub static ref INPUT_TYPES: Vec<Type> = vec![common_type::float3];
  pub static ref OUTPUT_TYPES: Vec<Type> = vec![common_type::float3];
}

fn apply_impl<T>(sim: &ParamVar, rb: &ParamVar, inner: T) -> Result<(), &'static str>
where
  T: Fn(&mut rapier3d::dynamics::RigidBody) -> (),
{
  let simulation = sim.get();
  let simulation = Var::from_object_ptr_mut_ref::<Simulation>(simulation, &SIMULATION_TYPE)
    .map_err(|_| "Physics simulation not found")?;

  let rb = rb.get();
  let rb = Var::from_object_ptr_mut_ref::<RigidBody>(rb, &RIGIDBODY_TYPE)
    .map_err(|_| "RigidBody not found")?;

  for rb in &rb.rigid_bodies {
    let rb = simulation.bodies.get_mut(*rb);
    if let Some(rb) = rb {
      inner(rb);
    } else {
      return Err("RigidBody not found in the simulation");
    }
  }
  Ok(())
}

#[derive(shards::shard)]
#[shard_info("Physics.Impulse", "Adds an impulse to a rigid body")]
struct Impulse {
  #[shard_required]
  required: ExposedTypes,
  #[shard_warmup]
  simulation: ParamVar,
  #[shard_param(
    "RigidBody",
    "The rigidbody to apply the impulse to.",
    RIGIDBODIES_TYPES_SLICE
  )]
  rb: ParamVar,
}

impl Default for Impulse {
  fn default() -> Self {
    Self {
      simulation: ParamVar::new_named(SIMULATION_NAME),
      rb: ParamVar::new(().into()),
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Impulse {
  fn input_types(&mut self) -> &Types {
    &INPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &OUTPUT_TYPES
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

    self.required.push(ExposedInfo::new_static_with_help(
      cstr!("Physics.Simulation"),
      shccstr!("The physics simulation subsystem."),
      *SIMULATION_TYPE,
    ));

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    apply_impl(&self.simulation, &self.rb, |rb| {
      let (x, y, z) = input.try_into().unwrap();
      rb.apply_impulse(Vector3::new(x, y, z), true);
    })?;
    Ok(*input)
  }
}

#[derive(shards::shard)]
#[shard_info("Physics.Force", "Adds a force to a rigid body")]
struct Force {
  #[shard_required]
  required: ExposedTypes,
  #[shard_warmup]
  simulation: ParamVar,
  #[shard_param(
    "RigidBody",
    "The rigidbody to apply the force to.",
    RIGIDBODIES_TYPES_SLICE
  )]
  rb: ParamVar,
}

impl Default for Force {
  fn default() -> Self {
    Self {
      simulation: ParamVar::new_named(SIMULATION_NAME),
      rb: ParamVar::new(().into()),
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Force {
  fn input_types(&mut self) -> &Types {
    &INPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &OUTPUT_TYPES
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

    self.required.push(ExposedInfo::new_static_with_help(
      cstr!("Physics.Simulation"),
      shccstr!("The physics simulation subsystem."),
      *SIMULATION_TYPE,
    ));

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    apply_impl(&self.simulation, &self.rb, |rb| {
      let (x, y, z) = input.try_into().unwrap();
      rb.add_force(Vector3::new(x, y, z), true);
    })?;
    Ok(*input)
  }
}

#[derive(shards::shard)]
#[shard_info("Physics.AngularImpulse", "Adds an angular impulse to a rigid body")]
struct AngularImpulse {
  #[shard_required]
  required: ExposedTypes,
  #[shard_warmup]
  simulation: ParamVar,
  #[shard_param(
    "RigidBody",
    "The rigidbody to apply the impulse to.",
    RIGIDBODIES_TYPES_SLICE
  )]
  rb: ParamVar,
}

impl Default for AngularImpulse {
  fn default() -> Self {
    Self {
      simulation: ParamVar::new_named(SIMULATION_NAME),
      rb: ParamVar::new(().into()),
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for AngularImpulse {
  fn input_types(&mut self) -> &Types {
    &INPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &OUTPUT_TYPES
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

    self.required.push(ExposedInfo::new_static_with_help(
      cstr!("Physics.Simulation"),
      shccstr!("The physics simulation subsystem."),
      *SIMULATION_TYPE,
    ));

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    apply_impl(&self.simulation, &self.rb, |rb| {
      let (x, y, z) = input.try_into().unwrap();
      rb.apply_torque_impulse(Vector3::new(x, y, z), true);
    })?;
    Ok(*input)
  }
}

#[derive(shards::shard)]
#[shard_info("Physics.Torque", "Adds a torque to a rigid body")]
struct Torque {
  #[shard_required]
  required: ExposedTypes,
  #[shard_warmup]
  simulation: ParamVar,
  #[shard_param(
    "RigidBody",
    "The rigidbody to apply the torque to.",
    RIGIDBODIES_TYPES_SLICE
  )]
  rb: ParamVar,
}

impl Default for Torque {
  fn default() -> Self {
    Self {
      simulation: ParamVar::new_named(SIMULATION_NAME),
      rb: ParamVar::new(().into()),
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Torque {
  fn input_types(&mut self) -> &Types {
    &INPUT_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &OUTPUT_TYPES
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

    self.required.push(ExposedInfo::new_static_with_help(
      cstr!("Physics.Simulation"),
      shccstr!("The physics simulation subsystem."),
      *SIMULATION_TYPE,
    ));

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    apply_impl(&self.simulation, &self.rb, |rb| {
      let (x, y, z) = input.try_into().unwrap();
      rb.add_torque(Vector3::new(x, y, z), true);
    })?;
    Ok(*input)
  }
}

pub fn register_shards() {
  register_shard::<Impulse>();
  register_shard::<Force>();
  register_shard::<AngularImpulse>();
  register_shard::<Torque>();
}
