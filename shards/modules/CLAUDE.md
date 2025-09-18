# Creating Shards in C++

This guide explains how to create new shards in C++ within the Shards module system.

## Basic Shard Structure

Every shard is a C++ struct with specific static methods and member functions. Here's the essential pattern:

```cpp
struct MyShardName {
  // Required static methods
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString help() { return SHCCSTR("Description of what this shard does"); }

  // Lifecycle methods
  void warmup(SHContext *context) { /* Initialize resources */ }
  void cleanup(SHContext *context) { /* Clean up resources */ }
  SHTypeInfo compose(SHInstanceData &data) { /* Validate and setup at compile time */ }

  // Main execution
  SHVar activate(SHContext *context, const SHVar &input) {
    // Process input and return output
    return Var("result");
  }
};
```

## Core Components

### 1. Required Static Methods

- **`inputTypes()`**: Specifies what types this shard accepts as input
- **`outputTypes()`**: Specifies what types this shard produces as output
- **`help()`**: Provides documentation string for the shard

### 2. Parameters

Use the `PARAM` macro system for shard parameters:

```cpp
// Basic parameter
PARAM(ShardsVar, _action, "Action", "Description", {CoreInfo::ShardsOrNone});

// Variable parameter (can reference context variables)
PARAM_PARAMVAR(_color, "Color", "Description", {CoreInfo::ColorType, CoreInfo::ColorVarType});

// Required parameters implementation
PARAM_IMPL(PARAM_IMPL_FOR(_action), PARAM_IMPL_FOR(_color));
```

### 3. Lifecycle Methods

#### `warmup(SHContext *context)`
- Called when the shard is first used
- Initialize resources, allocate memory
- Always call `PARAM_WARMUP(context)` if you have parameters

#### `cleanup(SHContext *context)`
- Called when shard is destroyed
- Release resources, free memory
- Always call `PARAM_CLEANUP(context)` if you have parameters

#### `compose(SHInstanceData &data)`
- Called at "compile time" to validate shard configuration
- Returns output type information
- Use for type checking and validation

### 4. Main Execution

#### `activate(SHContext *context, const SHVar &input)`
- Main execution function called during flow execution
- Processes input and returns output
- Should be efficient as it's called frequently

## Common Patterns

### 1. Simple Transform Shard
```cpp
struct ToUpper {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString help() { return SHCCSTR("Converts string to uppercase"); }

  std::string _buffer;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto text = SHSTRVIEW(input);
    _buffer.assign(text.begin(), text.end());
    std::transform(_buffer.begin(), _buffer.end(), _buffer.begin(), ::toupper);
    return Var(_buffer);
  }
};
```

### 2. Shard with Parameters
```cpp
struct Multiply {
  static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString help() { return SHCCSTR("Multiplies input by factor"); }

  PARAM_VAR(_factor, "Factor", "Number to multiply by", {CoreInfo::IntType});
  PARAM_IMPL(PARAM_IMPL_FOR(_factor));

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    return Var(input.payload.intValue * _factor.payload.intValue);
  }
};
```

### 3. Shard with Sub-Actions
```cpp
struct Conditional {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Executes action if condition is true"); }

  PARAM_PARAMVAR(_condition, "Condition", "Boolean condition", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM(ShardsVar, _action, "Action", "Action to execute", {CoreInfo::ShardsOrNone});
  PARAM_IMPL(PARAM_IMPL_FOR(_condition), PARAM_IMPL_FOR(_action));

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    _action.compose(data);
    PARAM_COMPOSE_MERGE_REQUIRED(_action);
    return data.inputType; // Pass through input type
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _action.warmup(context);
  }

  void cleanup(SHContext *context) {
    _action.cleanup(context);
    PARAM_CLEANUP(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (_condition.get().payload.boolValue) {
      SHVar output{};
      _action.activate(context, input, output);
      return output;
    }
    return input;
  }
};
```

## Registration

All shards must be registered in a registration function:

```cpp
SHARDS_REGISTER_FN(mymodule) {
  REGISTER_SHARD("MyModule.ToUpper", ToUpper);
  REGISTER_SHARD("MyModule.Multiply", Multiply);
  REGISTER_SHARD("MyModule.Conditional", Conditional);
}
```

## Key Types and Utilities

### Common Input/Output Types
- `CoreInfo::AnyType` - Accepts any type
- `CoreInfo::StringType` - String input/output
- `CoreInfo::IntType` - Integer input/output
- `CoreInfo::BoolType` - Boolean input/output
- `CoreInfo::SeqType` - Sequence input/output
- `CoreInfo::NoneType` - No input expected

### Variable Access
- `SHSTRVIEW(var)` - Get string view from SHVar
- `asSeq(var)` - Get sequence from SHVar
- `Var("string")` - Create string SHVar
- `Var(123)` - Create integer SHVar
- `Var::Empty` - Empty/none value

### Parameter Types
- `PARAM_VAR` - Basic parameter
- `PARAM_PARAMVAR` - Parameter that can reference variables
- `PARAM` - Complex parameter (like ShardsVar for sub-actions)

## Module Structure

1. Create `.hpp` file in `shards/modules/[module_name]/`
2. Include required headers:
   ```cpp
   #include <shards/shards.hpp>
   #include <shards/core/shared.hpp>
   #include <shards/core/params.hpp>
   #include <shards/common_types.hpp>
   ```
3. Define shards in namespace
4. Add registration function
5. Update module's CMakeLists.txt if needed

## Best Practices

1. **Memory Management**: Use RAII, clean up in `cleanup()`
2. **Error Handling**: Use `ActivationError`, `ComposeError`, `WarmupError`
3. **Performance**: Cache computations, reuse buffers
4. **Type Safety**: Validate inputs in `compose()` when possible
5. **Documentation**: Provide clear help strings
6. **Naming**: Use PascalCase for shard names, prefix with module name

## Example: Complete Module

```cpp
#include <shards/shards.hpp>
#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>

namespace shards {
namespace text {

struct Reverse {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString help() { return SHCCSTR("Reverses a string"); }

  std::string _buffer;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto text = SHSTRVIEW(input);
    _buffer.assign(text.rbegin(), text.rend());
    return Var(_buffer);
  }
};

} // namespace text

SHARDS_REGISTER_FN(text) {
  using namespace text;
  REGISTER_SHARD("Text.Reverse", Reverse);
}
} // namespace shards
```

This pattern provides a solid foundation for creating new shards in the Shards ecosystem.

---

# Creating Shards in Rust

This guide explains how to create new shards in Rust within the Shards module system. Rust shards use proc macros for code generation and provide excellent type safety and memory management.

## Basic Rust Shard Structure

Every Rust shard is a struct that implements the `Shard` trait, using derive macros for code generation:

```rust
use shards::shard::Shard;
use shards::types::{ClonedVar, Context, Type, Types, Var, ExposedTypes, InstanceData};

#[derive(shards::shard)]
#[shard_info("MyModule.Transform", "Transforms input data")]
pub struct MyTransformShard {
    #[shard_required]
    required: ExposedTypes,

    // Parameters using attribute macros
    #[shard_param("Factor", "Multiplication factor", [common_type::int])]
    factor: ClonedVar,

    // Internal state
    output: ClonedVar,
}

impl Default for MyTransformShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            factor: 2.into(), // Default value
            output: ClonedVar::default(),
        }
    }
}

#[shards::shard_impl]
impl Shard for MyTransformShard {
    fn input_types(&mut self) -> &Types {
        &INT_TYPES
    }

    fn output_types(&mut self) -> &Types {
        &INT_TYPES
    }

    fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.warmup_helper(ctx)?;
        Ok(())
    }

    fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.cleanup_helper(ctx)?;
        self.output = ClonedVar::default();
        Ok(())
    }

    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        self.compose_helper(data)?;
        Ok(self.output_types()[0])
    }

    fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        let value: i64 = input.try_into()?;
        let factor: i64 = self.factor.0.as_ref().try_into()?;
        let result = value * factor;
        self.output = result.into();
        Ok(Some(self.output.0))
    }
}
```

## Core Components

### 1. Derive Macros

- **`#[derive(shards::shard)]`**: Generates boilerplate code for the shard
- **`#[shard_info("Name", "Description")]`**: Sets shard name and help text
- **`#[shard_impl]`**: Implements the Shard trait with helper methods

### 2. Parameters

Parameters use attribute macros on struct fields:

```rust
// Basic parameter with type constraints
#[shard_param("Size", "Buffer size", [common_type::int])]
size: ClonedVar,

// Parameter that can reference variables
#[shard_param("Input", "Input tensor", [*TENSOR_VAR_TYPE])]
input_tensor: ParamVar,

// Parameter with multiple allowed types
#[shard_param("Value", "Input value", [common_type::int, common_type::float])]
value: ParamVar,
```

### 3. Required Components

Every shard needs:

```rust
#[shard_required]
required: ExposedTypes,  // For variable tracking
```

### 4. Lifecycle Methods

#### `warmup(&mut self, ctx: &Context) -> Result<(), &str>`
- Initialize resources and validate parameters
- Always call `self.warmup_helper(ctx)?`

#### `cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str>`
- Clean up resources and reset state
- Always call `self.cleanup_helper(ctx)?`

#### `compose(&mut self, data: &InstanceData) -> Result<Type, &str>`
- Validate shard configuration at "compile time"
- Always call `self.compose_helper(data)?`

#### `activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str>`
- Main execution logic
- Returns `Ok(Some(var))` for normal output, `Ok(None)` for no output

## Common Patterns

### 1. Simple Transform Shard

```rust
#[derive(shards::shard)]
#[shard_info("Text.Upper", "Converts text to uppercase")]
pub struct UpperShard {
    #[shard_required]
    required: ExposedTypes,

    output: ClonedVar,
}

impl Default for UpperShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            output: ClonedVar::default(),
        }
    }
}

#[shards::shard_impl]
impl Shard for UpperShard {
    fn input_types(&mut self) -> &Types { &STRING_TYPES }
    fn output_types(&mut self) -> &Types { &STRING_TYPES }

    fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.warmup_helper(ctx)?;
        Ok(())
    }

    fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.cleanup_helper(ctx)?;
        self.output = ClonedVar::default();
        Ok(())
    }

    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        self.compose_helper(data)?;
        Ok(self.output_types()[0])
    }

    fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        let text: &str = input.try_into()?;
        let upper_text = text.to_uppercase();
        self.output = Var::ephemeral_string(&upper_text).into();
        Ok(Some(self.output.0))
    }
}
```

### 2. Shard with Parameters

```rust
#[derive(shards::shard)]
#[shard_info("Math.Scale", "Scales input by factor")]
pub struct ScaleShard {
    #[shard_required]
    required: ExposedTypes,

    #[shard_param("Factor", "Scaling factor", [common_type::float])]
    factor: ClonedVar,

    output: ClonedVar,
}

impl Default for ScaleShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            factor: 1.0.into(),
            output: ClonedVar::default(),
        }
    }
}

#[shards::shard_impl]
impl Shard for ScaleShard {
    fn input_types(&mut self) -> &Types { &FLOAT_TYPES }
    fn output_types(&mut self) -> &Types { &FLOAT_TYPES }

    fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.warmup_helper(ctx)?;
        Ok(())
    }

    fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.cleanup_helper(ctx)?;
        self.output = ClonedVar::default();
        Ok(())
    }

    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        self.compose_helper(data)?;
        Ok(self.output_types()[0])
    }

    fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        let value: f64 = input.try_into()?;
        let factor: f64 = self.factor.0.as_ref().try_into()?;
        let result = value * factor;
        self.output = result.into();
        Ok(Some(self.output.0))
    }
}
```

### 3. Shard Working with Sequences

```rust
#[derive(shards::shard)]
#[shard_info("Seq.Sum", "Sums all numbers in sequence")]
pub struct SumShard {
    #[shard_required]
    required: ExposedTypes,

    output: ClonedVar,
}

impl Default for SumShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            output: ClonedVar::default(),
        }
    }
}

#[shards::shard_impl]
impl Shard for SumShard {
    fn input_types(&mut self) -> &Types { &SEQ_OF_FLOAT_TYPES }
    fn output_types(&mut self) -> &Types { &FLOAT_TYPES }

    fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.warmup_helper(ctx)?;
        Ok(())
    }

    fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.cleanup_helper(ctx)?;
        self.output = ClonedVar::default();
        Ok(())
    }

    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        self.compose_helper(data)?;
        Ok(self.output_types()[0])
    }

    fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        let seq: SeqVar = input.try_into()?;
        let mut sum = 0.0f64;

        for item in seq.iter() {
            let value: f64 = item.try_into()?;
            sum += value;
        }

        self.output = sum.into();
        Ok(Some(self.output.0))
    }
}
```

### 4. Custom Object Types

For complex data types, define custom objects:

```rust
use shards::{fourCharacterCode, ref_counted_object_type_impl};

// Define the object wrapper
struct MyObject(SomeRustStruct);
ref_counted_object_type_impl!(MyObject);

// Create type constants
lazy_static! {
    pub static ref MYOBJECT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"mOBJ"));
    pub static ref MYOBJECT_TYPE_VEC: Vec<Type> = vec![*MYOBJECT_TYPE];
}

// Use in shard
#[derive(shards::shard)]
#[shard_info("MyModule.Process", "Process custom object")]
pub struct ProcessShard {
    #[shard_required]
    required: ExposedTypes,

    output: ClonedVar,
}

#[shards::shard_impl]
impl Shard for ProcessShard {
    fn input_types(&mut self) -> &Types { &MYOBJECT_TYPE_VEC }
    fn output_types(&mut self) -> &Types { &MYOBJECT_TYPE_VEC }

    fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        let obj = unsafe { &mut *Var::from_ref_counted_object::<MyObject>(&input, &*MYOBJECT_TYPE)? };

        // Process obj.0 (the wrapped SomeRustStruct)
        // ...

        self.output = Var::new_ref_counted(MyObject(processed), &*MYOBJECT_TYPE).into();
        Ok(Some(self.output.0))
    }
}
```

## Registration

Register shards in the module registration function:

```rust
#[no_mangle]
pub extern "C" fn shardsRegister_mymodule(core: *mut shards::shardsc::SHCore) {
    unsafe {
        shards::core::Core = core;
    }

    // Register enums
    register_enum::<MyEnum>();

    // Register object types
    register_object_type::<MyObject>(FRAG_CC, fourCharacterCode(*b"mOBJ"));

    // Register shards
    register_shard::<UpperShard>();
    register_shard::<ScaleShard>();
    register_shard::<SumShard>();
    register_shard::<ProcessShard>();
}
```

## Key Types and Utilities

### Common Type Collections
- `STRING_TYPES` - String input/output
- `INT_TYPES`, `FLOAT_TYPES` - Numeric types
- `SEQ_OF_INT_TYPES`, `SEQ_OF_FLOAT_TYPES` - Sequences
- `TENSOR_TYPE_VEC` - Custom object types

### Variable Types
- `ClonedVar` - Owned variable, cached value
- `ParamVar` - Parameter that can reference context variables
- `AutoSeqVar` - Automatically managed sequence
- `SeqVar` - Sequence variable
- `TableVar` - Table/map variable

### Type Conversion
```rust
let string_val: &str = var.try_into()?;
let int_val: i64 = var.try_into()?;
let float_val: f64 = var.try_into()?;
let seq: SeqVar = var.try_into()?;
```

### Creating Variables
```rust
let var = Var::ephemeral_string("text");
let var = 42i64.into();
let var = 3.14f64.into();
let var = Var::new_ref_counted(obj, &TYPE);
```

## Module Structure

1. Create `src/lib.rs` in `shards/modules/[module_name]/`
2. Set up dependencies in `Cargo.toml`:
   ```toml
   [dependencies]
   shards = { path = "../../../" }
   lazy_static = "1.4"
   ```
3. Include required imports:
   ```rust
   use shards::shard::Shard;
   use shards::types::*;
   use shards::core::{register_shard, register_enum, register_object_type};
   ```
4. Define shards with derive macros
5. Add registration function

## Best Practices

1. **Error Handling**: Use `Result<Option<Var>, &str>` for activate
2. **Memory Management**: Rust's ownership handles most cases automatically
3. **Type Safety**: Use strong typing and `try_into()` conversions
4. **Performance**: Cache expensive computations in struct fields
5. **Documentation**: Use clear shard_info descriptions
6. **Naming**: Use module prefix like "Tensor.Mul", "ML.Forward"

## Enums

Define custom enums for shard parameters:

```rust
#[derive(shards::shards_enum)]
#[enum_info(b"MODE", "ProcessMode", "Processing mode selection")]
pub enum ProcessMode {
    #[enum_value("Fast processing mode")]
    Fast = 0x1,
    #[enum_value("Accurate processing mode")]
    Accurate = 0x2,
}
```

## Complete Example Module

```rust
use shards::shard::Shard;
use shards::types::*;
use shards::core::{register_shard};

#[derive(shards::shard)]
#[shard_info("Text.Reverse", "Reverses input string")]
pub struct ReverseShard {
    #[shard_required]
    required: ExposedTypes,

    output: ClonedVar,
}

impl Default for ReverseShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            output: ClonedVar::default(),
        }
    }
}

#[shards::shard_impl]
impl Shard for ReverseShard {
    fn input_types(&mut self) -> &Types { &STRING_TYPES }
    fn output_types(&mut self) -> &Types { &STRING_TYPES }

    fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.warmup_helper(ctx)?;
        Ok(())
    }

    fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.cleanup_helper(ctx)?;
        self.output = ClonedVar::default();
        Ok(())
    }

    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        self.compose_helper(data)?;
        Ok(self.output_types()[0])
    }

    fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        let text: &str = input.try_into()?;
        let reversed: String = text.chars().rev().collect();
        self.output = Var::ephemeral_string(&reversed).into();
        Ok(Some(self.output.0))
    }
}

#[no_mangle]
pub extern "C" fn shardsRegister_text_utils(core: *mut shards::shardsc::SHCore) {
    unsafe {
        shards::core::Core = core;
    }

    register_shard::<ReverseShard>();
}
```

This Rust pattern provides excellent type safety, memory management, and integration with the Shards ecosystem. The derive macros eliminate most boilerplate while maintaining full control over shard behavior.