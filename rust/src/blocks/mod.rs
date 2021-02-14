#[cfg(not(target_arch = "wasm32"))]
extern crate reqwest;
extern crate cid;

#[cfg(not(target_arch = "wasm32"))]
pub mod http;
pub mod ipfs_cid;

#[cfg(test)]
mod tests {
  #[test]
  fn it_works() {
    assert_eq!(2 + 2, 4);
  }
}
