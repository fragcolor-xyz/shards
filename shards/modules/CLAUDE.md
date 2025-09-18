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