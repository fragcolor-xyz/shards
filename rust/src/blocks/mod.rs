#[cfg(not(target_arch = "wasm32"))]
extern crate reqwest;
extern crate tiny_keccak;

#[cfg(not(target_arch = "wasm32"))]
pub mod http;
pub mod hash;

#[cfg(test)]
mod tests {
  #[test]
  fn it_works() {
    assert_eq!(2 + 2, 4);
  }
}
