/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

use crate::Simulation;
use crate::EXPOSED_SIMULATION;
use crate::SIMULATION_TYPE;
use shards::core::register_legacy_shard;

use shards::types::Context;

use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::Parameters;

use shards::shard::LegacyShard;
use shards::types::ANY_TYPES;
use shards::types::FLOAT_TYPES_SLICE;

use rapier3d::dynamics::{
  CCDSolver, ImpulseJointSet, IntegrationParameters, MultibodyJointSet, RigidBodySet,
};
use rapier3d::geometry::{BroadPhase, ColliderSet, NarrowPhase};
use rapier3d::na::Vector3;
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline, QueryPipeline};
use rapier3d::prelude::IslandManager;
use shards::types::Types;
use shards::types::Var;
use std::convert::TryInto;
use std::rc::Rc;

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

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.self_obj.cleanup(ctx);
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

    self.contact_force_channel.try_iter().for_each(|f| {
      // TODO: Contact events
    });

    self.collisions_channel.try_iter().for_each(|f| {
      match (
        self.colliders.get(f.collider1()),
        self.colliders.get(f.collider2()),
      ) {
        (Some(c1), Some(c2)) => unsafe {
          let ptr1 = &mut *(c1.user_data as *mut crate::RigidBody);
          let ptr2 = &mut *(c2.user_data as *mut crate::RigidBody);
          if f.started() {
            if ptr1.want_collision_events {
              let mut vec = ptr1.collision_events.borrow_mut();
              vec.push(crate::CollisionEvent {
                tag: ptr2.user_data.clone(),
              })
            }
            if ptr2.want_collision_events {
              let mut vec = ptr2.collision_events.borrow_mut();
              vec.push(crate::CollisionEvent {
                tag: ptr1.user_data.clone(),
              })
            }
          }
        },
        _ => {} // Ignore if colliders are missing
      }
    });

    Ok(*input)
  }
}

pub fn register_shards() {
  register_legacy_shard::<Simulation>();
}
