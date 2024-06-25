/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

extern crate crossbeam;
extern crate rapier3d;

use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::CString;
use std::rc::Rc;

use shards::fourCharacterCode;
use shards::shardsc::SHObjectTypeInfo;
use shards::shardsc::SHType_Float4;

use shards::types::common_type;
use shards::types::ClonedVar;

use shards::types::ExposedInfo;

use shards::types::ParamVar;
use shards::types::Seq;
use shards::types::Type;
use shards::types::FRAG_CC;

use rapier3d::dynamics::{
  CCDSolver, ImpulseJointSet, IntegrationParameters, IslandManager, MultibodyJointSet,
  RigidBodyHandle, RigidBodySet,
};
use rapier3d::geometry::{
  BroadPhase, ColliderHandle, ColliderSet, ContactForceEvent, NarrowPhase, SharedShape,
};

use rapier3d::geometry::CollisionEvent as RapierCollisionEvent;
use rapier3d::na::{Isometry3, Matrix3, Matrix4, Vector3};
use rapier3d::pipeline::{ChannelEventCollector, PhysicsPipeline, QueryPipeline};

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

#[macro_use]
extern crate compile_time_crc32;

const SIMULATION_NAME: &str = "Physics.Simulation";
lazy_static! {
  pub static ref SIMULATION_NAME_CSTR: CString = CString::new(SIMULATION_NAME).unwrap();
}

lazy_static! {
  static ref SIMULATION_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"phys"));
  static ref EXPOSED_SIMULATION: Vec<ExposedInfo> = vec![ExposedInfo::new_static_with_help(
    SIMULATION_NAME_CSTR.to_str().unwrap(),
    shccstr!("The physics simulation subsystem."),
    *SIMULATION_TYPE
  )];
  static ref SHAPE_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"phyS"));
  static ref SHAPE_TYPE_VEC: Vec<Type> = vec![*SHAPE_TYPE];
  static ref SHAPES_TYPE: Type = Type::seq(&SHAPE_TYPE_VEC);
  static ref SHAPE_VAR_TYPE: Type = Type::context_variable(&SHAPE_TYPE_VEC);
  static ref SHAPES_VAR_TYPE: Type = Type::context_variable(&SHAPE_TYPE_VEC);
  static ref SHAPE_TYPES: Vec<Type> = vec![*SHAPE_TYPE];
  static ref RIGIDBODY_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"phyR"));
  static ref RIGIDBODY_TYPE_VEC: Vec<Type> = vec![*RIGIDBODY_TYPE];
  static ref RIGIDBODIES_TYPE: Type = Type::seq(&RIGIDBODY_TYPE_VEC);
  static ref RIGIDBODY_VAR_TYPE: Type = Type::context_variable(&RIGIDBODY_TYPE_VEC);
  static ref RIGIDBODIES_TYPES_SLICE: Vec<Type> = vec![*RIGIDBODY_VAR_TYPE, common_type::none];
  static ref SHAPES_TYPES_SLICE: Vec<Type> =
    vec![*SHAPE_VAR_TYPE, *SHAPES_VAR_TYPE, common_type::none];
}

static POSITIONS_TYPES_SLICE: &[Type] = &[
  common_type::float3,
  common_type::float3_var,
  common_type::float3s,
  common_type::float3s_var,
];
static ROTATIONS_TYPES_SLICE: &[Type] = &[
  common_type::float4,
  common_type::float4_var,
  common_type::float4s,
  common_type::float4s_var,
];
static POSITION_TYPES_SLICE: &[Type] = &[common_type::float3, common_type::float3_var];
static ROTATION_TYPES_SLICE: &[Type] = &[common_type::float4, common_type::float4_var];

pub type ColliderData = shards::types::Var;

struct Simulation {
  pipeline: PhysicsPipeline,
  islands_manager: IslandManager,
  query_pipeline: QueryPipeline,
  gravity: Vector3<f32>,
  integration_parameters: IntegrationParameters,
  broad_phase: BroadPhase,
  narrow_phase: NarrowPhase,
  bodies: RigidBodySet,
  colliders: ColliderSet,
  impulse_joints: ImpulseJointSet,
  multibody_joints: MultibodyJointSet,
  ccd_solver: CCDSolver,
  collisions_channel: crossbeam::channel::Receiver<RapierCollisionEvent>,
  contact_force_channel: crossbeam::channel::Receiver<ContactForceEvent>,
  event_handler: ChannelEventCollector,
  self_obj: ParamVar,
}

struct CollisionEvent {
  tag: ClonedVar,
}

#[derive(Default)]
struct RigidBody {
  rigid_bodies: Vec<RigidBodyHandle>,
  colliders: Vec<ColliderHandle>,
  user_data: ClonedVar,
  rapier_user_data: u128,
  want_collision_events: bool,
  collision_events: RefCell<Vec<CollisionEvent>>,
}

#[derive(Default)]
struct BaseShape {
  shape: Option<SharedShape>,
  position: Option<Isometry3<f32>>,
  mass: Option<f32>,
}

fn fill_seq_from_mat4(var: &mut Seq, mat: &Matrix4<f32>) {
  unsafe {
    let mat = mat.as_slice();
    var[0].valueType = SHType_Float4;
    var[0].payload.__bindgen_anon_1.float4Value[0] = mat[0];
    var[0].payload.__bindgen_anon_1.float4Value[1] = mat[1];
    var[0].payload.__bindgen_anon_1.float4Value[2] = mat[2];
    var[0].payload.__bindgen_anon_1.float4Value[3] = mat[3];
    var[1].valueType = SHType_Float4;
    var[1].payload.__bindgen_anon_1.float4Value[0] = mat[4];
    var[1].payload.__bindgen_anon_1.float4Value[1] = mat[5];
    var[1].payload.__bindgen_anon_1.float4Value[2] = mat[6];
    var[1].payload.__bindgen_anon_1.float4Value[3] = mat[7];
    var[2].valueType = SHType_Float4;
    var[2].payload.__bindgen_anon_1.float4Value[0] = mat[8];
    var[2].payload.__bindgen_anon_1.float4Value[1] = mat[9];
    var[2].payload.__bindgen_anon_1.float4Value[2] = mat[10];
    var[2].payload.__bindgen_anon_1.float4Value[3] = mat[11];
    var[3].valueType = SHType_Float4;
    var[3].payload.__bindgen_anon_1.float4Value[0] = mat[12];
    var[3].payload.__bindgen_anon_1.float4Value[1] = mat[13];
    var[3].payload.__bindgen_anon_1.float4Value[2] = mat[14];
    var[3].payload.__bindgen_anon_1.float4Value[3] = mat[15];
  }
}

fn mat4_from_seq(var: &Seq) -> Matrix4<f32> {
  unsafe {
    Matrix4::new(
      var[0].payload.__bindgen_anon_1.float4Value[0],
      var[0].payload.__bindgen_anon_1.float4Value[1],
      var[0].payload.__bindgen_anon_1.float4Value[2],
      var[0].payload.__bindgen_anon_1.float4Value[3],
      var[1].payload.__bindgen_anon_1.float4Value[0],
      var[1].payload.__bindgen_anon_1.float4Value[1],
      var[1].payload.__bindgen_anon_1.float4Value[2],
      var[1].payload.__bindgen_anon_1.float4Value[3],
      var[2].payload.__bindgen_anon_1.float4Value[0],
      var[2].payload.__bindgen_anon_1.float4Value[1],
      var[2].payload.__bindgen_anon_1.float4Value[2],
      var[2].payload.__bindgen_anon_1.float4Value[3],
      var[3].payload.__bindgen_anon_1.float4Value[0],
      var[3].payload.__bindgen_anon_1.float4Value[1],
      var[3].payload.__bindgen_anon_1.float4Value[2],
      var[3].payload.__bindgen_anon_1.float4Value[3],
    )
  }
}

fn mat3_from_seq(var: &Seq) -> Matrix3<f32> {
  unsafe {
    Matrix3::new(
      var[0].payload.__bindgen_anon_1.float3Value[0],
      var[0].payload.__bindgen_anon_1.float3Value[1],
      var[0].payload.__bindgen_anon_1.float3Value[2],
      var[1].payload.__bindgen_anon_1.float3Value[0],
      var[1].payload.__bindgen_anon_1.float3Value[1],
      var[1].payload.__bindgen_anon_1.float3Value[2],
      var[2].payload.__bindgen_anon_1.float3Value[0],
      var[2].payload.__bindgen_anon_1.float3Value[1],
      var[2].payload.__bindgen_anon_1.float3Value[2],
    )
  }
}

pub mod forces;
pub mod queries;
pub mod rigidbody;
pub mod shapes;
pub mod simulation;

#[no_mangle]
pub extern "C" fn shardsRegister_physics_physics(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  simulation::register_shards();
  shapes::register_shards();
  rigidbody::register_shards();
  queries::register_shards();
  forces::register_shards();
}
