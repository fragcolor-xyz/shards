extern crate crossbeam;
extern crate rapier3d;

use crate::chainblocksc::CBTypeInfo_Details_Object;
use crate::chainblocksc::CBType_Float4;
use crate::chainblocksc::CBType_None;
use crate::types::common_type;
use crate::types::ClonedVar;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::Seq;
use crate::types::Type;
use crate::Var;
use rapier3d::dynamics::{IntegrationParameters, JointSet, RigidBodySet};
use rapier3d::geometry::{BroadPhase, ColliderSet, ContactEvent, IntersectionEvent, NarrowPhase};
use rapier3d::na::{Matrix3, Matrix4, Vector3};
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

  static ref SHAPE_TYPE: Type = {
    let mut t = common_type::object;
    t.details.object = CBTypeInfo_Details_Object {
      vendorId: 0x73696e6b, // 'sink'
      typeId: 0x70687953, // 'phyS'
    };
    t
  };
  static ref SHAPES_TYPE: Type = Type::seq(&[*SHAPE_TYPE]);
  static ref SHAPE_VAR_TYPE: Type = Type::context_variable(&[*SHAPE_TYPE]);
  static ref SHAPES_VAR_TYPE: Type = Type::context_variable(&[*SHAPES_TYPE]);
  static ref SHAPE_TYPES: Vec<Type> = vec![*SHAPE_TYPE];
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

fn fill_seq_from_mat4(var: &mut Seq, mat: &Matrix4<f32>) {
  unsafe {
    let mat = mat.as_slice();
    var[0].valueType = CBType_Float4;
    var[0].payload.__bindgen_anon_1.float4Value[0] = mat[0];
    var[0].payload.__bindgen_anon_1.float4Value[1] = mat[1];
    var[0].payload.__bindgen_anon_1.float4Value[2] = mat[2];
    var[0].payload.__bindgen_anon_1.float4Value[3] = mat[3];
    var[1].valueType = CBType_Float4;
    var[1].payload.__bindgen_anon_1.float4Value[0] = mat[4];
    var[1].payload.__bindgen_anon_1.float4Value[1] = mat[5];
    var[1].payload.__bindgen_anon_1.float4Value[2] = mat[6];
    var[1].payload.__bindgen_anon_1.float4Value[3] = mat[7];
    var[2].valueType = CBType_Float4;
    var[2].payload.__bindgen_anon_1.float4Value[0] = mat[8];
    var[2].payload.__bindgen_anon_1.float4Value[1] = mat[9];
    var[2].payload.__bindgen_anon_1.float4Value[2] = mat[10];
    var[2].payload.__bindgen_anon_1.float4Value[3] = mat[11];
    var[3].valueType = CBType_Float4;
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

pub mod rigidbody;
pub mod shapes;
pub mod simulation;
