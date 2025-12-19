# HDL Module - Shards to Verilog Transpiler

This module translates Shards code into synthesizable Verilog HDL for FPGA development. It follows the same architectural pattern as the shader translator (`shards/modules/gfx/shader/`).

## Architecture Overview

```
Shards Code → Registry Dispatch → Translation Handlers → IR Blocks → Verilog Emitter → Verilog Code
```

The key insight (shared with shader translator): **Shards objects already act as AST nodes**, so no parser is needed. We walk the shard graph and translate each shard to hardware constructs.

## Directory Structure

```
shards/modules/hdl/
├── CMakeLists.txt           # Build configuration
├── hdl.hpp                  # Main include (aggregates all headers)
├── hdl.cpp                  # HDL.Module shard implementation
├── types.hpp/cpp            # HDL type system (BitWidth, FixedPoint)
├── blocks.hpp/cpp           # IR representation (Module, Port, Wire, etc.)
├── context.hpp/cpp          # Translation state management
├── registry.hpp/cpp         # Handler dispatch system
├── verilog_emitter.hpp/cpp  # Verilog code generation
└── shards/
    ├── hardware.cpp         # Core handlers (Set, Get, Ref, Const)
    ├── io.cpp               # HDL.Input, HDL.Output shards
    └── math.cpp             # Math.Add, Math.Subtract, etc. handlers
```

## Core Concepts

### 1. Two Types of Components

**HDL-Specific Shards** (new shards with no software equivalent):
- `HDL.Module` - Entry point, compiles contents to Verilog module
- `HDL.Input` - Declares input port
- `HDL.Output` - Declares output port and assigns value

**Translation Handlers** (translate existing shards to Verilog):
- `Math.Add` → `+`
- `Math.Subtract` → `-`
- `Math.Multiply` → `*`
- `Math.And` → `&`
- `Math.Or` → `|`
- `Math.Xor` → `^`
- `Set` → wire declaration + assignment
- `Get`/`Ref` → signal reference
- `Const` → literal value

### 2. Type System (`types.hpp`)

HDL types are bit-width aware, unlike software types:

```cpp
// Unsigned/signed bit vectors
struct BitWidth {
    uint16_t bits;      // Number of bits (1-65535)
    bool isSigned;      // Signed or unsigned
};

// Fixed-point (future)
struct FixedPoint {
    uint16_t integerBits;
    uint16_t fractionalBits;
    bool isSigned;
};

// Type variant
using Type = std::variant<BitWidth, FixedPoint>;

// Convenience constructors
Type U8()  { return BitWidth(8, false); }   // 8-bit unsigned
Type S16() { return BitWidth(16, true); }   // 16-bit signed
```

### 3. IR Blocks (`blocks.hpp`)

Intermediate representation before Verilog emission:

```cpp
// Base class for all HDL expressions
struct Block {
    virtual ~Block() = default;
    virtual BlockPtr clone() const = 0;
};

// Signal reference (wire/port name)
struct SignalRef : Block {
    std::string name;
    Type type;
};

// Literal value
struct Literal : Block {
    int64_t value;
    Type type;
};

// Binary expression (a + b, a & b, etc.)
struct BinaryExpr : Block {
    BinaryOp op;        // Add, Sub, Mul, And, Or, Xor
    BlockPtr lhs, rhs;
    Type resultType;
};

// Module-level constructs
struct Port { std::string name; Type type; bool isInput; };
struct Wire { std::string name; Type type; };
struct Assign { std::string target; BlockPtr value; };
struct Module {
    std::string name;
    std::vector<Port> ports;
    std::vector<Wire> wires;
    std::vector<Assign> assigns;
};
```

### 4. Translation Context (`context.hpp`)

Maintains state during translation:

```cpp
struct HDLContext {
    HDLRegistry &registry;              // Handler lookup
    std::map<std::string, SignalInfo> signals;  // Declared signals

    // Current value stack (dataflow)
    BlockPtr currentValue;
    std::optional<Type> currentType;

    // Module being built
    std::unique_ptr<Module> currentModule;

    // Key methods
    void beginModule(const std::string &name);
    std::unique_ptr<Module> endModule();

    void addInput(const std::string &name, Type type);
    void addOutput(const std::string &name, Type type);
    void addWire(const std::string &name, Type type);
    void addAssign(const std::string &name, BlockPtr value);

    void setCurrentValue(BlockPtr value, Type type);
    BlockPtr takeCurrentValue();
    std::optional<SignalInfo> findSignal(const std::string &name);
};
```

### 5. Handler Registry (`registry.hpp`)

Maps shard names to translation handlers:

```cpp
struct IHDLHandler {
    virtual void translate(Shard *shard, HDLContext &context) = 0;
};

struct HDLRegistry {
    void registerHandler(const char *shardName, IHDLHandler *handler);
    IHDLHandler *resolve(Shard *shard);
};

// Registration (in shards/math.cpp)
void registerHDLMathShards() {
    auto &registry = getHDLRegistry();
    registry.registerHandler("Math.Add", &mathAddHandler);
    registry.registerHandler("Math.Subtract", &mathSubtractHandler);
    // ...
}
```

### 6. Verilog Emitter (`verilog_emitter.hpp`)

Converts IR to Verilog text:

```cpp
std::string emitVerilog(const Module &module);

// Generates:
// module name(
//     input [7:0] a,
//     output [8:0] sum
// );
//     wire [7:0] temp;
//     assign sum = (a + b);
// endmodule
```

## How Translation Works

### Example Flow

```shards
HDL.Module(Name: "adder" Contents: {
  HDL.Input(Name: "a" Width: 8) >= a
  HDL.Input(Name: "b" Width: 8) >= b
  a | Math.Add(b) | HDL.Output(Name: "sum" Width: 9)
})
```

1. **HDL.Module** creates `HDLContext`, calls `beginModule("adder")`
2. **HDL.Input("a")** → `InputHandler::translate()`:
   - Adds port to module: `input [7:0] a`
   - Sets current value to `SignalRef("a", U8())`
3. **Set (>=)** → `SetHandler::translate()`:
   - Detects aliasing (input port), registers "a" as alias
   - Passes through the signal reference
4. **HDL.Input("b")** → Same as above for "b"
5. **Get (a)** → `GetHandler::translate()`:
   - Looks up signal "a", sets current value to `SignalRef("a")`
6. **Math.Add(b)** → `MathAddHandler::translate()`:
   - Takes LHS from current value (`SignalRef("a")`)
   - Resolves RHS operand "b" → `SignalRef("b")`
   - Creates `BinaryExpr(Add, a, b, U9())` (9-bit result for overflow)
   - Sets as current value
7. **HDL.Output("sum")** → `OutputHandler::translate()`:
   - Adds port: `output [8:0] sum`
   - Adds assign: `assign sum = (a + b)`
8. **HDL.Module** calls `endModule()`, `emitVerilog()` → Verilog string

## Adding New Handlers

### Adding a Translation Handler for an Existing Shard

Example: Adding `Math.Divide` support:

```cpp
// In shards/math.cpp

struct MathDivideHandler : public IHDLHandler {
    void translate(Shard *shard, HDLContext &context) override {
        SHVar operandVar = shard->getParam(shard, 0);

        if (!context.hasCurrentValue()) {
            throw HDLError("Math.Divide requires an input value");
        }

        auto lhsType = context.getCurrentType();
        auto lhs = context.takeCurrentValue();

        auto [rhs, rhsType] = resolveOperand(operandVar, context, lhsType);

        // Division keeps same width (integer division)
        auto resultType = lhsType.value_or(U32());

        auto expr = makeBinaryExpr(BinaryOp::Div, std::move(lhs), std::move(rhs), resultType);
        context.setCurrentValue(std::move(expr), resultType);
    }
};

static MathDivideHandler mathDivideHandler;

void registerHDLMathShards() {
    auto &registry = getHDLRegistry();
    // ... existing registrations ...
    registry.registerHandler("Math.Divide", &mathDivideHandler);
}
```

Don't forget to add `Div` to `BinaryOp` enum and handle it in `verilog_emitter.cpp`.

### Adding a New HDL-Specific Shard

Example: Adding `HDL.Reg` for registers (sequential logic):

```cpp
// In shards/sequential.cpp (new file)

struct RegShard {
    static SHTypesInfo inputTypes() { return CoreInfo::IntType; }
    static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
    static SHOptionalString help() {
        return SHCCSTR("Declares a register (sequential element) in HDL synthesis.");
    }

    PARAM_VAR(_name, "Name", "Register name", {CoreInfo::StringType});
    PARAM_VAR(_width, "Width", "Bit width", {CoreInfo::IntType});
    PARAM_IMPL(PARAM_IMPL_FOR(_name), PARAM_IMPL_FOR(_width));

    // ... lifecycle methods ...

    SHVar activate(SHContext *context, const SHVar &input) {
        return input; // Passthrough in software mode
    }
};

struct RegHandler : public IHDLHandler {
    void translate(Shard *shard, HDLContext &context) override {
        // Extract parameters, create reg declaration
        // Add to always @(posedge clk) block
        // ...
    }
};

// Register both the shard AND the handler
SHARDS_REGISTER_FN(hdl_sequential) {
    REGISTER_SHARD("HDL.Reg", RegShard);
}

void registerHDLSequentialShards() {
    auto &registry = getHDLRegistry();
    registry.registerHandler("HDL.Reg", &regHandler);
}
```

Update `registry.cpp` to call `registerHDLSequentialShards()`.

## Testing

Test files go in `shards/tests/hdl-*.shs`:

```shards
; hdl-verilog-0.shs - Basic combinational test
@wire(test-hdl {
  HDL.Module(Name: "adder" Contents: {
    HDL.Input(Name: "a" Width: 8) >= a
    HDL.Input(Name: "b" Width: 8) >= b
    a | Math.Add(b) | HDL.Output(Name: "sum" Width: 9)
  })
  | Log("Generated Verilog:")
  | Assert.IsNot("")
})

@mesh(main)
@schedule(main test-hdl)
@run(main)
```

Run with:
```bash
build/Debug/shards shards/tests/hdl-verilog-0.shs
```

## Future Work (Phases)

### Phase 2: Sequential Logic
- `HDL.Reg` shard for registers
- Clock domain context (`clk`, `rst_n`)
- `always @(posedge clk)` block generation
- Reset handling

### Phase 3: Memory & Hierarchy
- `HDL.BRAM` for block RAM inference
- Module instantiation (`HDL.Instance`)
- Parameterized modules

### Phase 4: DSP & Advanced
- Fixed-point arithmetic (`@fixed(8 8)`)
- DSP block inference (MAC patterns)
- Testbench generation

## Comparison with Shader Translator

| Aspect | Shader Translator | HDL Module |
|--------|-------------------|------------|
| Target | WGSL (WebGPU shaders) | Verilog (FPGA) |
| Types | Float vectors, textures | Bit vectors, fixed-point |
| Flow | Dataflow → shader function | Dataflow → module |
| Context | Graphics pipeline | Clock domains, ports |
| Registry | `TranslationRegistry` | `HDLRegistry` |
| IR | `IWGSLGenerated`, `blocks::*` | `Block`, `Module` |

The patterns are intentionally similar - if you understand one, you understand the other.

## Key Files to Reference

| File | Purpose |
|------|---------|
| `hdl.cpp` | Entry point, `HDL.Module` implementation |
| `context.hpp` | Translation state, signal tracking |
| `registry.cpp` | Handler registration, `registerAllHDLShards()` |
| `shards/math.cpp` | Example translation handlers |
| `verilog_emitter.cpp` | Code generation |

## Common Patterns

### Resolving Operands (literal or variable)

```cpp
// Helper in math.cpp - handles both integer literals and variable references
auto [rhs, rhsType] = resolveOperand(operandVar, context, lhsType);
```

### Port Aliasing

When `Set` receives a reference to an existing port, it creates an alias instead of a new wire:

```cpp
// In SetHandler::translate()
if (auto *ref = dynamic_cast<SignalRef *>(value.get())) {
    auto existingSignal = context.findSignal(ref->name);
    if (existingSignal && existingSignal->isPort) {
        // Just create alias, don't create wire
        context.signals.emplace(name, *existingSignal);
        return;
    }
}
```

### Type Inference for Results

Binary operations compute result type based on operands:

```cpp
// Wider type wins, addition/subtraction may need +1 bit for overflow
auto resultType = binaryOpResultType(lhsType, rhsType, '+');
```
