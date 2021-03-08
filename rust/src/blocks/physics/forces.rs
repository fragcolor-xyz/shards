use crate::blocks::physics::RigidBody;
use crate::blocks::physics::Simulation;
use crate::blocks::physics::EXPOSED_SIMULATION;
use crate::blocks::physics::RIGIDBODIES_TYPE;
use crate::blocks::physics::RIGIDBODY_TYPE;
use crate::blocks::physics::RIGIDBODY_VAR_TYPE;
use crate::blocks::physics::SIMULATION_TYPE;
use crate::chainblocksc::CBPointer;
use crate::core::registerBlock;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::Type;
use crate::types::ANY_TYPES;
use crate::Block;
use crate::Types;
use crate::Var;
use rapier3d::dynamics::{IntegrationParameters, JointSet, RigidBodySet};
use rapier3d::geometry::{
  BroadPhase, ColliderSet, ContactEvent, InteractionGroups, IntersectionEvent, NarrowPhase, Ray,
};
use rapier3d::na::{Point3, Vector3};
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline, QueryPipeline};
use std::convert::TryInto;

lazy_static! {
  pub static ref INPUT_TYPES: Vec<Type> = vec![common_type::float3];
  pub static ref OUTPUT_TYPES: Vec<Type> = vec![common_type::float3];
  static ref PARAMETERS: Parameters = vec![(
    cstr!("Rigidbody"),
    cbccstr!("The rigidbody to apply the impulse to."),
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
    r.simulation.setName("Physics.Simulation");
    r
  }
}

impl Block for Impulse {
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
  fn setParam(&mut self, index: i32, value: &Var) {
    match index {
      0 => {
        if !value.is_none() {
          self.rb.setName(value.try_into().unwrap())
        }
      }
      _ => unreachable!(),
    }
  }
  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => {
        if self.rb.isVariable() {
          self.rb.getName().into()
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
      cbccstr!("The physics simulation subsystem."),
      *SIMULATION_TYPE,
    ));
    if self.rb.isVariable() {
      self.reqs.push(ExposedInfo::new_with_help_from_ptr(
        self.rb.getName(),
        cbccstr!("The required rigid_body."),
        *RIGIDBODY_TYPE,
      ));
    }
    Some(&self.reqs)
  }
  fn cleanup(&mut self) {
    self.rb.cleanup();
    self.simulation.cleanup();
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.rb.warmup(context);
    self.simulation.warmup(context);
    Ok(())
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    let simulation = self.simulation.get();
    let simulation = Var::into_object_mut_ref::<Simulation>(simulation, &SIMULATION_TYPE)
      .map_err(|_| "Physics simulation not found.")?;

    let rb = self.rb.get();
    let rb = Var::into_object_mut_ref::<RigidBody>(rb, &SIMULATION_TYPE)
      .map_err(|_| "Rigidbody not found.")?;

    if let Some(rb) = rb.rigid_body {
      let rb = simulation.bodies.get_mut(rb);
      if let Some(rb) = rb {
        let (x, y, z) = input.as_ref().try_into()?;
        rb.apply_impulse(Vector3::new(x, y, z), true);
      }
    }

    Ok(*input)
  }
}

pub fn registerBlocks() {
  registerBlock::<Impulse>();
}
