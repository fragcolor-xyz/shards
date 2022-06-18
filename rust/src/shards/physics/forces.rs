/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::shards::physics::RigidBody;
use crate::shards::physics::Simulation;
use crate::shards::physics::EXPOSED_SIMULATION;
use crate::shards::physics::RIGIDBODIES_TYPE;
use crate::shards::physics::RIGIDBODY_TYPE;
use crate::shards::physics::RIGIDBODY_VAR_TYPE;
use crate::shards::physics::SIMULATION_TYPE;
use crate::shardsc::SHPointer;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Type;
use crate::types::ANY_TYPES;
use crate::Shard;
use crate::Types;
use crate::Var;
use rapier3d::dynamics::{IntegrationParameters, JointSet, RigidBodySet};
use rapier3d::geometry::{
  BroadPhase, ColliderSet, ContactEvent, InteractionGroups, IntersectionEvent, NarrowPhase, Ray,
};
use rapier3d::na::{Point3, Vector3};
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline, QueryPipeline};
use std::convert::TryInto;
use std::rc::Rc;

lazy_static! {
  pub static ref INPUT_TYPES: Vec<Type> = vec![common_type::float3];
  pub static ref OUTPUT_TYPES: Vec<Type> = vec![common_type::float3];
  static ref PARAMETERS: Parameters = vec![(
    cstr!("RigidBody"),
    shccstr!("The rigidbody to apply the impulse to."),
    vec![*RIGIDBODY_VAR_TYPE, common_type::none]
  )
    .into()];
}

struct Impulse {
  simulation: ParamVar,
  rb: ParamVar,
  reqs: ExposedTypes,
}

impl Default for Impulse {
  fn default() -> Self {
    let mut r = Impulse {
      simulation: ParamVar::new(().into()),
      rb: ParamVar::new(().into()),
      reqs: ExposedTypes::new(),
    };
    r.simulation.set_name("Physics.Simulation");
    r
  }
}

impl Shard for Impulse {
  fn registerName() -> &'static str {
    cstr!("Physics.Impulse")
  }
  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.Impulse-rust-0x20200101")
  }
  fn name(&mut self) -> &str {
    "Physics.Impulse"
  }
  fn inputTypes(&mut self) -> &Types {
    &INPUT_TYPES
  }
  fn outputTypes(&mut self) -> &Types {
    &OUTPUT_TYPES
  }
  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&PARAMETERS)
  }
  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => {
        if !value.is_none() {
          Ok(self.rb.set_name(value.try_into()?))
        } else {
          Ok(())
        }
      }
      _ => unreachable!(),
    }
  }
  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => {
        if self.rb.is_variable() {
          self.rb.get_name().into()
        } else {
          ().into()
        }
      }
      _ => unreachable!(),
    }
  }
  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.reqs.clear();
    self.reqs.push(ExposedInfo::new_static_with_help(
      cstr!("Physics.Simulation"),
      shccstr!("The physics simulation subsystem."),
      *SIMULATION_TYPE,
    ));
    if self.rb.is_variable() {
      self.reqs.push(ExposedInfo::new_with_help_from_ptr(
        self.rb.get_name(),
        shccstr!("The required rigid_body."),
        *RIGIDBODY_TYPE,
      ));
    }
    Some(&self.reqs)
  }
  fn cleanup(&mut self) -> Result<(), &str> {
    self.rb.cleanup();
    self.simulation.cleanup();
    Ok(())
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.rb.warmup(context);
    self.simulation.warmup(context);
    Ok(())
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let simulation = self.simulation.get();
    let simulation = Var::from_object_ptr_mut_ref::<Simulation>(simulation, &SIMULATION_TYPE)
      .map_err(|_| "Physics simulation not found")?;

    let rb = self.rb.get();
    let rb = Var::from_object_mut_ref::<RigidBody>(rb, &RIGIDBODY_TYPE)
      .map_err(|_| "RigidBody not found")?;

    for rb in &rb.rigid_bodies {
      let rb = simulation.bodies.get_mut(*rb);
      if let Some(rb) = rb {
        let (x, y, z) = input.as_ref().try_into()?;
        rb.apply_impulse(Vector3::new(x, y, z), true);
      } else {
        return Err("RigidBody not found in the simulation");
      }
    }

    Ok(*input)
  }
}

pub fn registerShards() {
  registerShard::<Impulse>();
}
