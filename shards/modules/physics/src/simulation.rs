/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use shards::core::register_legacy_shard;
use crate::Simulation;
use crate::EXPOSED_SIMULATION;
use crate::SIMULATION_TYPE;

use shards::types::Context;

use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::Parameters;

use shards::types::ANY_TYPES;
use shards::types::FLOAT_TYPES_SLICE;
use shards::shard::LegacyShard;

use shards::types::Types;
use shards::types::Var;
use rapier3d::dynamics::{
  CCDSolver, ImpulseJointSet, IntegrationParameters, MultibodyJointSet, RigidBodySet,
};
use rapier3d::geometry::{BroadPhase, ColliderSet, NarrowPhase};
use rapier3d::na::Vector3;
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline, QueryPipeline};
use rapier3d::prelude::IslandManager;
use std::convert::TryInto;

lazy_static! {
  static ref SIMULATION_PARAMETERS: Parameters = vec![(
    cstr!("Gravity"),
    shccstr!("The gravity force vector."),
    FLOAT_TYPES_SLICE
  )
    .into()];
}

impl Default for Simulation {
  fn default() -> Self {
    let (collision_send, collision_recv) = crossbeam::channel::unbounded();
    let (contact_force_send, contact_force_recv) = crossbeam::channel::unbounded();
    let mut res = Simulation {
      pipeline: PhysicsPipeline::new(),
      islands_manager: IslandManager::new(),
      query_pipeline: QueryPipeline::new(),
      gravity: Vector3::new(0.0, -9.81, 0.0),
      integration_parameters: IntegrationParameters::default(),
      broad_phase: BroadPhase::new(),
      narrow_phase: NarrowPhase::new(),
      bodies: RigidBodySet::new(),
      colliders: ColliderSet::new(),
      impulse_joints: ImpulseJointSet::new(),
      multibody_joints: MultibodyJointSet::new(),
      ccd_solver: CCDSolver::new(),
      collisions_channel: collision_recv,
      contact_force_channel: contact_force_recv,
      event_handler: ChannelEventCollector::new(collision_send, contact_force_send),
      self_obj: ParamVar::new(().into()),
    };
    res.self_obj.set_name("Physics.Simulation");
    res
  }
}

impl LegacyShard for Simulation {
  fn registerName() -> &'static str {
    cstr!("Physics.Simulation")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.Simulation-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Physics.Simulation"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&SIMULATION_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => {
        let (x, y, z) = value.try_into()?;
        self.gravity = Vector3::new(x, y, z);
        Ok(())
      }
      _ => unreachable!(),
    }
  }
  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => (self.gravity[0], self.gravity[1], self.gravity[2]).into(),
      _ => unreachable!(),
    }
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.self_obj.cleanup();
    Ok(())
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.self_obj.warmup(context);
    unsafe {
      self.self_obj.set_cloning(&Var::new_object_from_ptr(
        self as *const Simulation,
        &SIMULATION_TYPE,
      ));
    }
    Ok(())
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    self.pipeline.step(
      &self.gravity,
      &self.integration_parameters,
      &mut self.islands_manager,
      &mut self.broad_phase,
      &mut self.narrow_phase,
      &mut self.bodies,
      &mut self.colliders,
      &mut self.impulse_joints,
      &mut self.multibody_joints,
      &mut self.ccd_solver,
      Some(&mut self.query_pipeline),
      &(),
      &self.event_handler,
    );
    Ok(*input)
  }
}

pub fn registerShards() {
  register_legacy_shard::<Simulation>();
}
