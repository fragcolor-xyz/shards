# Below are comprehensive instructions for learning and using the Shards programming language:

## Context

Shards is a data flow programming language that follows a unique paradigm. It uses pipes (`|`) and organizes logic into Wires scheduled on Meshes.

## Execution Model

Shards uses data flow. There are no traditional functions. Wires are pipelines of operations (Shards) on data. Wires have state and run on Meshes.

## Parameter Rules

- Once you use a named parameter, all subsequent parameters must also be named
- Named parameters use colon syntax: `Name: value`
- Cannot mix unnamed and named parameters after first named parameter

Examples:

```shards
; Basic operations use pipes and parameters
5 | Add(3)                    ; 5 + 3
value | Mul(2.0)         ; value * 2.0

; All named parameters - OK
Http.Get(
  URL: "https://api.example.com"
  Headers: headers
  Timeout: 30
)

; Common with API calls
Http.Post(
  URL: "https://api.example.com/data"
  Headers: {
    "Content-Type": "application/json"
    "Authorization": token
  }
  Body: payload
)

; Error examples - wrong parameter order:
Http.Get(URL: "https://api.example.com" headers)             ; Error! Named then unnamed
Process(Input: data other-param)                            ; Error! Must put unnamed first
```

## Syntax & Structure

- Declare wires: `@wire(name { ... })`
- Data flows through `|`
- No braces for code blocks other than wire and template definitions, no semicolons needed
- Whitespace flexible
- Comments start with `;`
- No imperative assignments: data flows into variables

## Identifiers & Names

- Variables: lowercase, can be `var`, `my-var`, etc.
- Namespaces: `var/subvar`
- Shards: Capitalized, e.g. `Math.Sin`
- Use kebab-case for multi-word variable names
- No traditional functions; all logic is wires and shards

## String Operations

- String.Join: Combines sequence of strings
- String.Format: Combines strings with other types (numbers, vectors, etc)

Examples:

```shards
; String.Join for string sequences
["Hello" "World"] | String.Join = joined          ; "HelloWorld"
["a" "b" "c"] | String.Join = abc                ; "abc"

; String.Format for mixing types
["Score: " 42 " points"] | String.Format = score  ; "Score: 42 points"
["Position: " @f3(1 2 3)] | String.Format = pos   ; "Position: @f3(1 2 3)"

; Common patterns
["Bearer " token] | String.Join = auth-header     ; Joining strings
["User " name " is " age " years old"] | String.Format = info  ; Mixing strings and numbers

; With variables
"Alice" = name
30 = age
["Hello " name "! Age: " age] | String.Format = greeting  ; "Hello Alice! Age: 30"
```

## Data Types & Literals

- Strings: `"hello"`
- Integers: `1`
- Floats: `1.0`
- Float vectors: `@f2(1.0 2.0)`, `@f3(1.0 2.0 3.0)`, `@f4(1.0 2.0 3.0 4.0)`
- Int vectors: `@i2(1 2)`, `@i3(1 2 3)`, `@i4(1 2 3 4)`
- Booleans: `true`, `false`
- None: `none` or `null`
- Hex: `0x1A3F`
- Sequences: `[1 2 3]` (can hold any types)
  - Access: `sequence | Take(index)`, `RTake`, `Slice`, etc.
  - Modify: append with `value >> seq-var`, remove with `Erase(index seq-var)`

Example:

```shards
[1 2 3 4 5] >= numbers
numbers | Take(2) | Log("Third element")          ; Outputs: 3
numbers | Take([0 2 4]) | Log("Selected")         ; Outputs: [1 3 5]
numbers | Slice(1 3) | Log("Slice")               ; Outputs: [2 3]
numbers | Take(0) | Log("First element")          ; Outputs: 1
numbers | RTake(0) | Log("Last element")          ; Outputs: 5
6 >> numbers                                      ; Append 6
{data: [1 2 3]} >= table-with-seq
4 | Push(table-with-seq "data")                   ; Push 4 into table's data seq
Erase(2 numbers)                                  ; Remove element at index 2
numbers | Log("After removal")
Erase([0 2] numbers)                              ; Remove multiple indices
numbers | Log("After multiple removals")
```

- Tables: `{key: value key2: value2}`
  - Access: `table:key` or `table | Take(key)`
  - Modify: `new-value | Update(table key)`
  - Remove: `Erase(key table)`

Example:

```shards
{name: "Alice" age: 30 city: "NY"} >= person-data
person-data:name | Log(Label: "Name")             ; "Alice"
"Bob" | Update(person-data "name")                ; update name
31 | Update(person-data "age")                    ; update age
Erase("city" person-data)                         ; remove city
person-data | Log(Label: "After removing city")
Erase(["name" "age"] person-data)                 ; remove multiple keys
person-data | Log(Label: "After removing keys")
```

- Matrices: sequences of vectors, e.g. identity matrix:
  `[@f4(1 0 0 0) @f4(0 1 0 0) @f4(0 0 1 0) @f4(0 0 0 1)]`
  Rotate 45° around Z:
  `[@f3(0.707 -0.707 0) @f3(0.707 0.707 0) @f3(0 0 1)]`

## Variables & Data Flow

- Immutable assign: `value = var`  or `value | Ref(var)`
- Mutable assign: `value >= var`  or `value | Set(var)`
- Update mutable: `new-value > var`  or `new-value | Update(var)`
- Append seq: `value >> seq-var`  or `value | Push(seq-var)`
- Pipe for chaining: `op1 | op2 | op3`

Think of a river system, not a storage container:

TRADITIONAL (WRONG):
x = 5          ❌ This is not a container being filled
x += 1         ❌ This is not incrementing a stored value

SHARDS (CORRECT):
5 >= x         ✅ This creates a channel named 'x' and flows 5 into it
x | Add(1) > x   ✅ This takes flow from x, adds 1, and channels it back

Key concept: In Shards, variables are not boxes that hold values.
They are CHANNELS that direct data flow, like:

- Water channels in a river system
- Data streams in a network
- Signal paths in a circuit

Examples showing the flow:

; Creating flow channels
5 >= counter           ; Start a mutable flow channel named 'counter'
"hello" = message      ; Start an immutable flow channel named 'message'

; Directing and transforming flows
counter | Add(1) > counter    ; Take flow from counter, add 1, direct back
input | Transform | Process > output   ; Chain of flow transformations

Remember: The | operator is not a pipe between containers.
It's a direction marker showing how data flows through your system.

## Wire State & Variables:

- Each wire has its own isolated state
- Child wires can access parent wire variables
- Two ways to run child wires:
  1. `Do(wire)` - child shares parent's variables directly
  2. `Detach(wire)` - child gets copies of parent's variables
- Use `Global: true` for shared state between any wires

Examples:

```shards
; Parent wire with child access
@wire(parent {
  0 >= counter
  "data" = value

  ; Regular child - shares parent's variables
  Do(child)
  counter | Log(Label: "After child")  ; Shows child's changes

  ; Detached child - gets copies of variables
  Detach(detached-child)
  counter | Log(Label: "After detached")  ; Unchanged
})

@wire(child {
  ; Can modify parent's variables directly
  counter | Add(1) > counter
})

@wire(detached-child {
  ; Works with copies of parent's variables
  counter | Add(1) > counter  ; Only affects local copy
})

; Global variables for shared state between any wires
@wire(init {
  ; Create global variables that any wire can access
  0 | Set(shared-counter Global: true)
  "initial" | Set(shared-value Global: true)

  ; Global tables
  Table(shared-state Type: @type({
    status: Type::String
    count: Type::Int
    data: Type::Sequence
  }) Global: true)
  "ready" | Update(shared-state "status")
})

@wire(any-wire {
  ; Can access and modify globals from anywhere
  shared-counter | Add(1) > shared-counter
  shared-value | Log(Label: "Value")
  shared-state:status | Log(Label: "Status")
})
```

Best practices:

- Use globals when you need truly shared state between wires
- Use Do(wire) when child needs to modify parent state
- Use Detach(wire) when child should work with its own copy
- Document global variables clearly
- Use tables for structured global state
- Initialize all globals in a single init wire

## Sub Blocks & Flow Control

- Sub blocks `{...}` are used when multiple operations need to process the same input independently
- Example with ForEach:

  ```shards
  [1 2 3] | ForEach({
    {Mul(2) | Log}  ; each operation gets original input
    {Add(5) | Log}       ; processes same input independently
  })
  ```

- Not needed for single operation flows:

  ```shards
  ForRange(1 10 {
    Add(sum) > sum  ; single flow, no sub block needed
  })
  ```

- Common use cases for Sub blocks:
  - ForEach: when multiple operations need to process each element
  - Cond: for multiple condition/action pairs on same input
  - When operations need to branch but maintain original input

## Control Flow

- `If(condition then else)`
- `When(condition action)`
- `Repeat({ ... } Times: count)`
- `ForEach({ ... })`
- `Match([value1 {action1} value2 {action2} ...])`
- `Once({ ... })` for one-time init

Examples:

```shards
10 | If(IsMore(5)
  {"Greater" | Log(Label: "Result")}
  {"LessOrEq" | Log(Label: "Result")})

Repeat({
  "Repeated" | Log(Label: "Loop")
} Times: 3)

[1 2 3] | ForEach({
  Mul(2) | Log(Label: "Doubled")
})

"B" | Match([
  "A" {"Option A" | Log(Label: "Match")}
  "B" {"Option B" | Log(Label: "Match")}
  none {"Other" | Log(Label: "Match")}
])
```

Once block for init:

```shards
Once({
  0 >= counter
  "" >= message
})
```

## Types & Safety

- Define custom types with `@type({ ... })`
- `Expect(type)` to ensure type matches
- `Assert.Is(value)` to verify conditions

Example:

```shards
; Define a custom type
@type({
  name: Type::String
  age: Type::Int
  scores: Type::Sequence
}) = person-type

; Use the type
{name: "Alice" age: 30 scores: [85 92 78]} | Expect(person-type) = person-data
person-data:age | Assert.Is(30)
```

## Concurrency

- Wires on different meshes run concurrently
- Use `Detach(wire)` for non-blocking

Example:

```shards
@wire(long-task {
  Pause(5.0)
  "Done" | Log("Background")
})

@wire(main-loop {
  "Main loop" | Log
  Detach(long-task)
} Looped: true)

@mesh(root)
@schedule(root main-loop)
@run(root FPS: 30)
```

## Error Handling

- `Maybe({ ... } { ... })` for error handling.

Example:

```shards
Maybe({
  "10" | FromJson | Add(5) | Log(Label: "Success")
} {
  "Error occurred" | Log(Label: "Error")
})
```

## Logging

- `message | Log("Label")`
- Label optional

## Program Structure

- Wires: `@wire(name { ... })`
- Defines: `@define(name ...)`
- Templates: `@template(name [args] { ... })`
- Built-ins start with `@`, e.g. `@f3`, `@template`
- Variables: lowercase
- Shards: Capitalized, e.g. `UI.Area`, `Math.Sin`

## Operation Grouping

- Use parentheses to control evaluation order
- `(time | Mul(0.5))` ensures that part runs first

Example:

```shards
orbit-angle | Add((time | Mul(0.5))) > orbit-angle
```

## Templates

- `@template(name [params] { code })`
- Reusable code blocks
- Called like `@template-name(param1 param2 ...)`

Example:

```shards
@template(process-data [input output] {
  input | Transform | Calculate > output
})

; and used like this:
@process-data(in-var out-var)
```

## Variable Declaration/Usage

- Declare before use
- `=`: immutable, `>=`: mutable, `>`: update

Example:

```shards
"Hello" = greeting
0 >= counter
greeting | Log(Label: "Greeting")
```

## Operation Grouping & Evaluation Order

- Use parentheses `(...)` to control evaluation order in pipe chains

Example:

```shards
; Need parentheses for nested pipe operations:
ToFloat | Div((32 | Div(3.1415926535))) >= x

; Equivalent to:
32 | Div(3.1415926535) = tmp
ToFloat | Div(tmp) >= x
```

- Parentheses create an implicit temporary variable
- Required when you want to use the result of a pipe operation as an argument
- Common in mathematical expressions and nested operations

## Examples

Basic wire:

```shards
@wire(hello-world {
  "Hello, Shards!" | Log("Greeting")
  @f3(1.0 2.0 3.0) = vec3-variable | Log("Vector3")
  [1 2 3 4] = seq-variable
  {a: 1 b: 2} = table-ref
})
```

Control flow:

```shards
@wire(control-flow-demo {
  10 | If(IsMore(5)
    {"Greater than 5" | Log("Result")}
    {"Less or equal to 5" | Log("Result")}
  )
  Repeat({"Repeated" | Log("Loop")} Times: 3)
  [1 2 3] | ForEach({Mul(2) | Log("Doubled")})
  "B" | Match([
    "A" {"Option A" | Log("Match")}
    "B" {"Option B" | Log("Match")}
    none {"Other option" | Log("Match")}
  ])
})
```

Types & error:

```shards
@define(person @type({name: Type::String age: Type::Int}))
@wire(type-and-error-demo {
  {name: "Alice" age: 30} | Expect(@person) = person-table-ref
  person-table-ref:age | Assert.Is(30)
  Maybe({
    "10" | FromJson | Add(5) | Log("Success")
  } {
    "Error occurred" | Log
  })
})
```

Concurrency & scheduling:

```shards
@wire(long-running-task {
  Pause(5.0)
  "Task completed" | Log("Background")
})

@wire(main-loop {
  "Main loop running" | Log("Main")
  Detach(long-running-task)
} Looped: true)

@mesh(root)
@schedule(root main-loop)
@run(root FPS: 30)
```

Grouping operations:

```shards
@wire(grouping-demo {
  10 | Add((5 | Mul(2))) | Log("Result")   ; 10+(5*2)=20
  10 | Add(5) | Mul(2) | Log("Result")     ; (10+5)*2=30

  1.0 | Set(base)
  2.0 | Set(exponent)
  0.5 | Set(scale)

  base | Math.Pow((exponent | Mul(scale))) | Log("Correct")
  (base | Math.Pow(exponent)) | Mul(scale) | Log("Different")
})
```

## Mathematical Operations in Shards

- Data flows left to right, use `|` and proper grouping. No traditional infix.

Example:

```shards
radius | Mul((Math.Sin(theta))) | Mul((Math.Cos(phi))) = x
```
