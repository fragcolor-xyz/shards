use tract_onnx::prelude::*;

use crate::{shard::Shard, types::ANY_TYPES, core::registerShard};

#[derive(Default)]
struct TractTest {}

impl Shard for TractTest {
  fn registerName() -> &'static str {
    cstr!("ONNX.Test")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("ONNX.Test-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "ONNX.Test"
  }

  fn inputTypes(&mut self) -> &crate::types::Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &crate::types::Types {
    &ANY_TYPES
  }

  fn activate(
    &mut self,
    context: &crate::types::Context,
    input: &crate::types::Var,
  ) -> Result<crate::types::Var, &str> {
    let _ = Self::test_me().unwrap();
    Ok(().into())
  }
}

impl TractTest {
  fn test_me() -> TractResult<()> {
    let model = tract_onnx::onnx()
      // load the model
      .model_for_path("mobilenetv2-7.onnx")?
      // specify input type and shape
      .with_input_fact(0, f32::fact(&[1, 3, 224, 224]).into())?
      // optimize the model
      .into_optimized()?
      // make the model runnable and fix its inputs and outputs
      .into_runnable()?;

    // open image, resize it and make a Tensor out of it
    let image = image::open("grace_hopper.jpg").unwrap().to_rgb8();
    let resized =
      image::imageops::resize(&image, 224, 224, ::image::imageops::FilterType::Triangle);
    let image: Tensor = tract_ndarray::Array4::from_shape_fn((1, 3, 224, 224), |(_, c, y, x)| {
      let mean = [0.485, 0.456, 0.406][c];
      let std = [0.229, 0.224, 0.225][c];
      (resized[(x as _, y as _)][c] as f32 / 255.0 - mean) / std
    })
    .into();

    // run the model on the input
    let result = model.run(tvec!(image))?;

    // find and display the max value with its index
    let best = result[0]
      .to_array_view::<f32>()?
      .iter()
      .cloned()
      .zip(2..)
      .max_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
    println!("result: {:?}", best);
    Ok(())
  }
}

pub fn registerShards() {
  registerShard::<TractTest>();
}