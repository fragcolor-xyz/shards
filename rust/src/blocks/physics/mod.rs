extern crate crossbeam;
extern crate rapier3d;

use crate::chainblocksc::CBTypeInfo_Details_Object;
use crate::types::common_type;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Type;
use rapier3d::dynamics::{IntegrationParameters, JointSet, RigidBodySet};
use rapier3d::geometry::{BroadPhase, ColliderSet, ContactEvent, IntersectionEvent, NarrowPhase};
use rapier3d::na::Vector3;
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline};

lazy_static! {
  static ref SIMULATION_TYPE: Type = {
    let mut t = common_type::object;
    t.details.object = CBTypeInfo_Details_Object {
      vendorId: 0x73696e6b, // 'sink'
      typeId: 0x70687973, // 'phys'
    };
    t
  };

  static ref EXPOSED_SIMULATION: Vec<ExposedInfo> = vec![ExposedInfo::new_static(
    cstr!("Physics.Simulation"),
    *SIMULATION_TYPE
  )];
}

struct Simulation {
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
  self_obj: ParamVar,
}

pub mod simulation;
pub mod rigidbody;