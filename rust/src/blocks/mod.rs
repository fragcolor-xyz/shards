#[cfg(not(target_arch = "wasm32"))]
extern crate reqwest;

extern crate tiny_keccak;
extern crate sha2;
extern crate secp256k1;
extern crate hex;
extern crate bs58;
extern crate wasabi_leb128;

#[cfg(not(any(target_arch = "wasm32", target_os = "ios")))]
extern crate webbrowser;

use crate::types::Type;
use crate::types::common_type;

#[cfg(not(target_arch = "wasm32"))]
pub mod http;

pub mod hash;
pub mod sign;

pub mod physics;

#[cfg(not(any(target_arch = "wasm32", target_os = "ios")))]
pub mod browse;

pub mod casting;
