#[cfg(not(target_arch = "wasm32"))]
extern crate reqwest;

extern crate tiny_keccak;
extern crate secp256k1;
extern crate hex;

use crate::types::Type;
use crate::types::common_type;

#[cfg(not(target_arch = "wasm32"))]
pub mod http;

pub mod hash;
pub mod sign;

pub mod physics;
