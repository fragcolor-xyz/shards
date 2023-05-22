# Shards in C++

## Implementing a new shard

To create a new shards and register it to the system, do the following.

### Create a struct

The struct should contain any field necessary for the shard, especially parameters. They don't need to be `public`.

```cpp
struct MyShard {
  private:
    ParamVar _myParam;
    ShardsVar _myShards;
    bool _myBool;
}
```

### Give default values to fields

All declared fields should be given a default value.

```cpp
struct MyShard {
  private:
    ParamVar _myParam{};
    ShardsVar _myShards{};
    bool _myBool{false};
}
```

Note that `ParamVar{}` is equivalent to setting `nil` in the textual language. That parameter will have the `None` type by default. To specify a different default value (for instance an integer), use the `Paramvar {Var()}` constructor:

```cpp
struct MyShard {
  private:
    ParamVar _myParam{Var(42)};
    ShardsVar _myShards{};
    bool _myBool{false};
}
```

### Minimal implementation

Some functions have a default implementation. At minimum the following must be implemented:

```cpp
struct MyShard {
  static SHTypesInfo inputTypes() {}
  static SHTypesInfo outputTypes() {}
  SHVar activate(SHContext *context, const SHVar &input) {}
}
```

#### Implement `inputTypes()`, `outputTypes()`

These functions define the accepted input types and the expected output types of the shard. It can be any type including `None` and `Any`.

```cpp
struct MyShard {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
}
```

*Note: a shard that doesn't use its input could be accepting and producing `None`. However, it limits the usability of that shard, and we usually prefer to have the input "pass through", in which case we accept `Any` type and return the same.*

#### Implement `activate()`

This function is called every tick of the wire owning an instance of this shard. This is where the main logic should be implemented.

It receives the `input` of the shard and should return an output (it can be `Var::Empty`, which is the equivalent of `None`).

```cpp
struct MyShard {
  SHVar activate(SHContext *context, const SHVar &input) {
     SHVar output{};
    _myShards.activate(context, input, output);
    
    // input passthrough
    return input;
  }
}
```

### Parameters

If the shards has parameters, additional functions should be implemented.

```cpp
struct MyShard {
  static SHParametersInfo parameters() {}
  void setParam(int index, const SHVar &value) {}
  SHVar getParam(int index) {}
  void warmup(SHContext *ctx) {}
  void cleanup() {}
}
```

#### Implement `parameters()`

If the shard has parameters, this function should return of description of them. Usually this is done in two steps:

1. Define a static variable to hold the description.
2. Return that description in the `parameters()` function.

```cpp
struct MyShard {
  static SHParametersInfo parameters() { return SHParametersInfo(_params); }
  
private:
  static inline Parameters _params{
    {"MyParam", SHCCSTR("The integer parameter"), {CoreInfo::IntType, CoreInfo::IntVarType}},
    {"Shards", SHCCSTR("The inner shards"), {CoreInfo::ShardsOrNone}},
    {"MyBool", SHCCSTR("Some boolean value"), {CoreInfo::BoolType}}
  };
}
```

#### Implement `setParam()`, `getParam()`

Since the shard has parameters, we need to implement their getters and setters. The parameter index matches the order of definition in the `SHParametersInfo` struct returned by `parameters()`.

```cpp
struct MyShard {
  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0: {
      _myParam = value;
    } break;
    case 1:
      _myShards = value;
      break;
    case 2:
      _myBool = value.payload.boolValue
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _myParam;
    case 1:
      return _myShards;
    case 2:
      return Var(_myBool);
    default:
      return Var::Empty;
    }
  }
}
```

#### Implement `warmup()`, `cleanup()`

Some parameters are saved as `ParamVar` or `ShardsVar`. Those types need special care to manage the underlying memory.

```cpp
struct MyShard {
  void warmup(SHContext *ctx) {
    _myParam.warmup(ctx);
    _myShards.warmup(ctx);
  }

  void cleanup() {
    _myShards.cleanup();
    _myParam.cleanup();
  }
}
```

*Note: by convention `cleanup()` uses the reverse order of `warmup()`. This prevents some potential issues with dependent resources, though it might occur only in rare cases.*

### Other

Finally, if the shard has other shards as parameters, has additional type checks, or if it should expose variables; then other functions might need to be implemented.

#### Implement `compose()`

```cpp
struct MyShard {
  SHTypeInfo compose(const SHInstanceData &data) {
    _myShards.compose(data);

    // passthrough the input
    return data.inputType;
  }
}
```

#### Implement `exposedVariables()`

Implement this function when a shard can receive a variable as parameter that doesn't exist (i.e. is not declared elsewhere). In this case, the shard will expose that variable for other shards to use.

```cpp
struct MyShard {
  SHExposedTypesInfo exposedVariables() {
    if (_myParam.isVariable() > 0 && _exposing) {
      _expInfo = ExposedInfo(
          ExposedInfo::Variable(_myParam.variableName(), SHCCSTR("The exposed variable"), CoreInfo::IntType, true));
      return SHExposedTypesInfo(_expInfo);
    } else {
      return {};
    }
  }
}
```

The `ExposedInfo` instance needs to be owned by the shard. Hence, it must be defined as a field and properly initialized.

```cpp
struct MyShard {
  private:
    ExposedInfo _expInfo{};
    bool _exposing{false};
}
```

In addition, the variable should only be exposed if it doesn't exist yet. We can check whether that's the case during compose:

```cpp
struct MyShard {
  SHTypeInfo compose(const SHInstanceData &data) {
    if (_myParam.isVariable()) {
      _exposing = true; // assume we expose a new variable
      // search for a possible existing variable and ensure it's the right type
      for (auto &var : data.shared) {
        if (strcmp(var.name, _myParam.variableName()) == 0) {
          // we found a variable, make sure it's the right type and mark exposing off
          _exposing = false;
          if (var.exposedType.basicType != CoreInfo::IntType.basicType) {
            throw SHException("MyShard: incorrect type of variable.");
          }
          // also make sure it's mutable!
          if (!var.isMutable) {
            throw SHException("MyShard: Existing variable is not mutable.");
          }
          break;
        }
      }
    }

    // passthrough the input
    return data.inputType;
  }
}
```

#### Implement `requiredVariables()`

In a similar but opposite way to `exposedVariables()`, a shard might require that a variable exists.

```cpp
struct MyShard {
  SHExposedTypesInfo requiredVariables() {
    if (_myParam.isVariable()) {
      _reqInfo = ExposedInfo(
          ExposedInfo::Variable(_variable.variableName(), SHCCSTR("The integer parameter"), CoreInfo::IntType, true));
      return SHExposedTypesInfo(_reqInfo);
    } else {
      return {};
    }
  }
}
```

The `ExposedInfo` instance needs to be owned by the shard. Hence, it must be defined as a field and properly initialized.

```cpp
struct MyShard {
  private:
    ExposedInfo _reqInfo{};
}
```

*Note: exposing and requiring the same variable is illogical and likely a bug that needs to be fixed.*

### Register the shard

Once a shard is ready, it must be registered. Usually it is done in a `registerShards()` function:

```cpp
void registerShards() {
  REGISTER_SHARD("MyShard", MyShard);
}
```

When any shards are added to a module you can declare a register function like this:

```cpp
namespace shards {
SHARDS_REGISTER_FN(some_identifier) { 
  REGISTER_SHARD(...); 
}
}
```

The identifier can be anything unique inside the module. Then add the identifier to the module definition so it will be called:

```cmake
add_shards_module(my_module SOURCES ${SOURCES}
  REGISTER_SHARDS some_identifier)
```

You can add mode than one register function per module
