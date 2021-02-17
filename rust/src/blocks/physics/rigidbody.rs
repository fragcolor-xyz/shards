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
use crate::types::ANY_TYPE;
use crate::Block;
use crate::Types;
use crate::Var;
use rapier3d::dynamics::RigidBodyBuilder;
use rapier3d::dynamics::{
  BodyStatus, IntegrationParameters, JointSet, RigidBodyHandle, RigidBodySet,
};
use rapier3d::geometry::{BroadPhase, ColliderSet, ContactEvent, IntersectionEvent, NarrowPhase};
use rapier3d::na::Vector3;
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline};
use std::convert::TryInto;

lazy_static! {
  static ref PARAMETERS: Parameters = vec![(
    "Gravity",
    "The gravity force vector.",
    vec![common_type::float3]
  )
    .into()];
}

struct RigidBody {
  simulation_var: ParamVar,
  rigid_body: Option<RigidBodyHandle>,
}

impl Default for RigidBody {
  fn default() -> Self {
    let mut r = RigidBody {
      simulation_var: ParamVar::new(().into()),
      rigid_body: None,
    };
    r.simulation_var.setName("Physics.Simulation");
    r
  }
}

#[derive(Default)]
struct DynamicRigidBody {
  rb: RigidBody,
}

impl Block for DynamicRigidBody {
  fn registerName() -> &'static str {
    cstr!("Physics.DynamicRigidBody")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.DynamicRigidBody-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "Physics.DynamicRigidBody"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPE
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPE
  }

  // fn parameters(&mut self) -> Option<&Parameters> {
  //   Some(&PARAMETERS)
  // }

  // fn setParam(&mut self, index: i32, value: &Var) {
  //   match index {
  //     0 => {
  //       let (x, y, z) = value.try_into().unwrap();
  //       self.gravity = Vector3::new(x, y, z);
  //     }
  //     _ => unreachable!(),
  //   }
  // }
  // fn getParam(&mut self, index: i32) -> Var {
  //   match index {
  //     0 => (self.gravity[0], self.gravity[1], self.gravity[2]).into(),
  //     _ => unreachable!(),
  //   }
  // }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }

  fn cleanup(&mut self) {
    self.rb.simulation_var.cleanup();
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.rb.simulation_var.warmup(context);
    let sim_var = self.rb.simulation_var.get();
    unsafe {
      let simulation = Var::into_object_mut_ref::<Simulation>(sim_var, &SIMULATION_TYPE)?;
      let rigid_body = RigidBodyBuilder::new(BodyStatus::Dynamic).build();
      self.rb.rigid_body = Some(simulation.bodies.insert(rigid_body));
    }
    Ok(())
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    Ok(*input)
  }
}

pub fn registerBlocks() {
  registerBlock::<DynamicRigidBody>();
}
