---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---
# Data Construction

## Integer Vectors
Include `@i2`, `@i3`, `@i4`, `@i8`, `@i16`

* These built-in functions construct integer vector types (int2, int3, int4, int8, int16).
* If they are called in expression position and have only constants for parameters, the value is evaluated at compile time.
* Otherwise (if any of its parameters are variables or if the function was called in pipeline position), the value is evaluated at runtime.
* Can only accept both constants and variables as values for its parameters, however they must be of type int.
* If only one value was provided for its parameter, the function will automatically fill the remaining parameters with the provided value.

=== "@i2, @i3, @i4, @i8, @i16 example"
    ```shards
      @i2(1 1) | Log("i2 value")

      @i3(1 2 3) | Log("i3 value")

      @i4(1 2 3 4) | Log("i4 value")

      @i8(1) | Log("i8 value")

      @i16(1) | Log("i16 value")
    ```

!!! note
    **Expression Position VS Pipeline Position**
    When a built-in function is called to provide a value for a parameter of another function or shard, it is in expression position. If a built-in shard is called in its own standalone line however, for example as the input for a shard, it is in pipeline position.

    === "Expression Position"
    
        ```shards
        Math.Add(Operand: @i2(1 1)) ;; compile time evaluation
        ```
    Because `@i2` was used to provide a value for the `Operand` parameter in the `Math.Add` shard, it is in expression position.

    === "Pipeline Position"
    
        ```shards
        @i2(1 1) ;; runtime evaluation
        Log("value")
        ```
        Because `@i2` is it's own standalone step, it is in pipeline position.


## Float Vectors
Include `@f2`, `@f3`, `@f4`

* These built-in functions construct float vector types (float2, float3, float4).
* If they are called in expression position and have only constants for parameters, the value is evaluated at compile time.
* Otherwise (if any of its parameters are variables or if the function was called in pipeline position), the value is evaluated at runtime.
* Can only accept both constants and variables as values for its parameters, however they must be of `Type::Float`.
* If only one value was provided for its parameter, the function will automatically fill the remaining parameters with the provided value.

=== "@f2, @f3, @f4, @f8, @f16 example"
    ```shards
      @f2(1 1) | Log("f2 value")

      @f3(1 2 3) | Log("f3 value")

      @f4(1 2 3 4) | Log("f4 value")
    ```

## Color Vectors
Includes `@color`

* Creates a RGBA color (RGBA color value represents RED, GREEN, and BLUE light sources, with an Alpha channel (opacity))
* There are different formats to construct the RGBA color.
* If integer constants or variables were provided as the parameters, each parameter (red, green, blue or alpha) defines the intensity of the color or alpha with a value between 0 and 255.
* If float constants or variables were provided as the parameters, each parameter (red, green, blue or alpha) defines the intensity of the color or alpha with a value between 0.0 and 1.0.
* If a single float or integer value was provided as the parameter, the remaining 3 will automatically be filled with the single value provided.
* A hexadecimal RGB literal can also be provided as a parameter to generate the corresponding color.

| Format          | Example                   | Description                                                 |
| --------------- | ------------------------- | ----------------------------------------------------------- |
| **Float RGBA**  | `@color(1.0 0.5 0.0 1.0)` | Values between `0.0`–`1.0` for R, G, B, A                   |
| **Integer RGB** | `@color(255 0 128)`       | Values between `0`–`255` for R, G, B; alpha defaults to 255 |
| **Hex RGB**     | `@color(0x008080)`        | Packed RGB hex literal (`0xRRGGBB`)                         |
| **Hex RGBA**    | `@color(0x008080ff)`      | Packed RGBA hex literal (`0xRRGGBBAA`)                      |


=== "@color example"
    ```shards
      @color(1.0 0.0 0.0 1.0) | Log("red color")

      @color(255 0 255) | Log("purple color")

      @color(0) | Log("black color")

      @color(0x008080ff) | Log("teal color")
    ```

--8<-- "includes/license.md"