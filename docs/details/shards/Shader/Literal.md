This shards inserts WGSL source code directly into the generated shader. This can be usefull to reuse large existing shader code bases.

## Usages

There are multiple ways to use this shard.

### Static code

```clojure
  Shader.Literal(Source: "
    let color = vec4<f32>(1.0, 0.5, 0.25, 1.0);
    return color; ")
```

This insertes the given WGSL source code directly into the current entry point.

### Static header code

When defining WGSL functions, specify the `Type: ShaderLiteralType::Header` parameter:

```clojure
  Shader.Literal(Type: ShaderLiteralType::Header Source: "
    fn scale(val: f32) -> f32 {
      return val * 0.5;
    }")
```

The generated code will then be placed outside the current entry point where it can be referenced by all other entry points as well.

### Output value

When a Literal shard is expected to output a value, you need to specify the output type and dimensions.

For example, returning the value from calling the function defined in the previous section:

```clojure
  Shader.Literal(Source: "scale(1.0)" OutputType: ShaderFieldBaseType::Float32 OutputDimension: 1)
  >= result ; This will now contain the Float scalar result
```

`OutputMatrixDimension` can also be specified when returning matrix types, a value of 1 (default) indicates that the type is not a matrix. `mat4x3` would be represented by an `OutputDimension` of 4 and `MatrixDimension` of 3.

The default value for `OutputDimension` is 4, so it can be left unspecified when returning 4 component vectors.

### Capturing variables

To pass existing variables into WGSL code, use the folowing structure:

```clojure
  1.0 >= my-value
  Shader.Literal(Source: ["scale(" my-value ")"])
```

When the source is a sequence, any variable references inside this sequence will be inserted into the genereted WGSL code.

## More Info

For more information about the WebGPU Shading Language (WGSL), check the working draft [here](https://www.w3.org/TR/WGSL/).

For converting existing GLSL or SPIR-V shaders to WGSL, [naga](https://github.com/gfx-rs/naga) can be used.
