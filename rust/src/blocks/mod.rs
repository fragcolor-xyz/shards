extern crate reqwest;
#[cfg(target_arch="wasm32")]
extern crate wasm_bindgen_futures;

pub mod http;

#[cfg(test)]
mod tests {
  #[test]
  fn it_works() {
    assert_eq!(2 + 2, 4);
  }
}
