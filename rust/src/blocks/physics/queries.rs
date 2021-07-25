use crate::blocks::physics::Simulation;
use crate::blocks::physics::EXPOSED_SIMULATION;
use crate::blocks::physics::RIGIDBODIES_TYPE;
use crate::blocks::physics::RIGIDBODY_TYPE;
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
  pub static ref RAY_INPUT_TYPE: Type = {
    let mut t = common_type::float3s;
    t.fixedSize = 2;
    t
  };
  pub static ref RAY_INPUT_TYPES: Vec<Type> = vec![*RAY_INPUT_TYPE];
  pub static ref RAY_OUTPUT_TYPES: Vec<Type> = vec![*RIGIDBODIES_TYPE];
}

struct CastRay {
  simulation_var: ParamVar,
  output: Seq,
}

impl Default for CastRay {
  fn default() -> Self {
    let mut r = CastRay {
      simulation_var: ParamVar::new(().into()),
      output: Seq::new(),
    };
    r.simulation_var.set_name("Physics.Simulation");
    r
  }
}

impl Block for CastRay {
  fn registerName() -> &'static str {
    cstr!("Physics.CastRay")
  }
  fn hash() -> u32 {
    compile_time_crc32::crc32!("Physics.CastRay-rust-0x20200101")
  }
  fn name(&mut self) -> &str {
    "Physics.CastRay"
  }
  fn inputTypes(&mut self) -> &Types {
    &RAY_INPUT_TYPES
  }
  fn outputTypes(&mut self) -> &Types {
    &RAY_OUTPUT_TYPES
  }
  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    Some(&EXPOSED_SIMULATION)
  }
  fn cleanup(&mut self) {
    self.simulation_var.cleanup();
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.simulation_var.warmup(context);
    Ok(())
  }
  fn activate(&mut self, _: &Context, input: &Var) -> Result<Var, &str> {
    self.output.clear();

    let simulation = self.simulation_var.get();
    let simulation = Var::from_object_ptr_mut_ref::<Simulation>(simulation, &SIMULATION_TYPE)
      .map_err(|_| "Physics simulation not found.")?;

    let inputs: Seq = input.try_into().unwrap();
    let (from_x, from_y, from_z): (f32, f32, f32) = inputs[0].as_ref().try_into()?;
    let (dir_x, dir_y, dir_z): (f32, f32, f32) = inputs[1].as_ref().try_into()?;

    let ray = Ray::new(
      Point3::new(from_x, from_y, from_z),
      Vector3::new(dir_x, dir_y, dir_z),
    );

    let closest = simulation.query_pipeline.cast_ray(
      // TODO other type of queries enum
      &simulation.colliders,
      &ray,
      f32::MAX,
      true,                     // TODO
      InteractionGroups::all(), // TODO
      None,                     // TODO
    );

    if let Some(closest) = closest {
      // do we fear panics?
      let closest = simulation.colliders.get(closest.0).unwrap().user_data;
      unsafe {
        let closest = Var::new_object_from_raw_ptr(closest as CBPointer, &RIGIDBODY_TYPE);
        self.output.push(closest);
      }
    }

    Ok(self.output.as_ref().into())
  }
}

pub fn registerBlocks() {
  registerBlock::<CastRay>();
}
