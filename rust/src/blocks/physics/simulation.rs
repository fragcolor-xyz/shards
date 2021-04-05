use crate::blocks::physics::Simulation;
use crate::blocks::physics::EXPOSED_SIMULATION;
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
use crate::Block;
use crate::Types;
use crate::Var;
use rapier3d::dynamics::{CCDSolver, IntegrationParameters, JointSet, RigidBodySet};
use rapier3d::geometry::{BroadPhase, ColliderSet, ContactEvent, IntersectionEvent, NarrowPhase};
use rapier3d::na::Vector3;
use rapier3d::pipeline::{ChannelEventCollector, PhysicsHooks, PhysicsPipeline, QueryPipeline};
use std::convert::TryInto;

lazy_static! {
  static ref SIMULATION_PARAMETERS: Parameters = vec![(
    cstr!("Gravity"),
    cbccstr!("The gravity force vector."),
    vec![common_type::float3]
  )
    .into()];
}

impl Default for Simulation {
  fn default() -> Self {
    let (contact_send, contact_recv) = crossbeam::channel::unbounded();
    let (intersection_send, intersection_recv) = crossbeam::channel::unbounded();
    let mut res = Simulation {
      pipeline: PhysicsPipeline::new(),
      query_pipeline: QueryPipeline::new(),
      gravity: Vector3::new(0.0, -9.81, 0.0),
      integration_parameters: IntegrationParameters::default(),
      broad_phase: BroadPhase::new(),
      narrow_phase: NarrowPhase::new(),
      bodies: RigidBodySet::new(),
      colliders: ColliderSet::new(),
      joints: JointSet::new(),
      ccd_solver: CCDSolver::new(),
      contacts_channel: contact_recv,
      intersections_channel: intersection_recv,
      event_handler: ChannelEventCollector::new(intersection_send, contact_send),
      self_obj: ParamVar::new(().into()),
    };
    res.self_obj.setName("Physics.Simulation");
    res
  }
}

impl Block for Simulation {
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

  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => {
        let (x, y, z) = value.try_into().unwrap();
        self.gravity = Vector3::new(x, y, z);
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

  fn cleanup(&mut self) {
    self.self_obj.cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.self_obj.warmup(context);
    unsafe {
      self.self_obj.set(Var::new_object_from_ptr(
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
      &mut self.broad_phase,
      &mut self.narrow_phase,
      &mut self.bodies,
      &mut self.colliders,
      &mut self.joints,
      &mut self.ccd_solver,
      &(),
      &self.event_handler,
    );
    self.query_pipeline.update(&self.bodies, &self.colliders);
    Ok(*input)
  }
}

pub fn registerBlocks() {
  registerBlock::<Simulation>();
}
