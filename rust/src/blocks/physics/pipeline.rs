use crate::core::registerBlock;
use crate::types::Context;
use crate::types::ANY_TYPE;
use crate::Block;
use crate::Types;
use crate::Var;
use rapier3d::dynamics::{IntegrationParameters, JointSet, RigidBodySet};
use rapier3d::geometry::{BroadPhase, ColliderSet, ContactEvent, IntersectionEvent, NarrowPhase};
use rapier3d::na::Vector3;
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline};

struct Pipeline {
  pipeline: PhysicsPipeline,
  gravity: Vector3<f32>,
  integration_parameters: IntegrationParameters,
  broad_phase: BroadPhase,
  narrow_phase: NarrowPhase,
  bodies: RigidBodySet,
  colliders: ColliderSet,
  joints: JointSet,
  contacts_channel: crossbeam::channel::Receiver<ContactEvent>,
  intersections_channel: crossbeam::channel::Receiver<IntersectionEvent>,
  event_handler: ChannelEventCollector,
}

impl Default for Pipeline {
  fn default() -> Self {
    let (contact_send, contact_recv) = crossbeam::channel::unbounded();
    let (intersection_send, intersection_recv) = crossbeam::channel::unbounded();
    Pipeline {
      pipeline: PhysicsPipeline::new(),
      gravity: Vector3::new(0.0, -9.81, 0.0),
      integration_parameters: IntegrationParameters::default(),
      broad_phase: BroadPhase::new(),
      narrow_phase: NarrowPhase::new(),
      bodies: RigidBodySet::new(),
      colliders: ColliderSet::new(),
      joints: JointSet::new(),
      contacts_channel: contact_recv,
      intersections_channel: intersection_recv,
      event_handler: ChannelEventCollector::new(intersection_send, contact_send),
    }
  }
}

impl Block for Pipeline {
  fn registerName() -> &'static str {
    cstr!("Physics.Pipeline")
  }
  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.Pipeline-rust-0x20200101")
  }
  fn name(&mut self) -> &str {
    "Physics.Pipeline"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPE
  }
  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPE
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
      None,
      None,
      &self.event_handler,
    );
    Ok(*input)
  }
}

pub fn registerBlocks() {
  registerBlock::<Pipeline>();
}
