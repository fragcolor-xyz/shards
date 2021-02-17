extern crate crossbeam;
extern crate rapier3d;

use crate::chainblocksc::CBTypeInfo_Details_Object;
use crate::types::common_type;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::Type;

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
    cstr!("Physics.Pipeline"),
    *SIMULATION_TYPE
  )];
}

pub mod simulation;
