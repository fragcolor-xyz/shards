#[cfg(not(target_arch = "wasm32"))]
extern crate reqwest;

extern crate tiny_keccak;
extern crate secp256k1;
extern crate hex;

#[cfg(not(target_arch = "wasm32"))]
pub mod http;

pub mod hash;
pub mod sign;

#[cfg(test)]
mod tests {
  #[test]
  fn it_works() {
    assert_eq!(2 + 2, 4);
  }
}
