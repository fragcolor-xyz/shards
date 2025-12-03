# Shards Programming Language Reference

## Core Philosophy

Shards is a **dataflow programming language**. Data flows through channels like water through rivers - variables are not containers, they're named channels directing flow. There are no traditional functions - everything is Wires (pipelines of operations) scheduled on Meshes.

## Execution Model

- **Wires**: Pipelines of operations (Shards) that process data
- **Meshes**: Schedulers that run Wires
- **State**: Each Wire has isolated state; child Wires can access parent state via `Do()` or get copies via `Detach()`

## Syntax Fundamentals

```shards
// Comments - C-style
// Single line comment
/* Multi-line comment */

// No semicolons, no braces for code blocks (except wire/template definitions)
// Whitespace flexible

// Data flows left-to-right, top-to-bottom
// Newlines are IMPLICIT pipes - flow continues across lines
// These are equivalent:
"Hello" | Log

"Hello"
Log

// This is NOT like imperative languages where newlines end statements
// In Shards, data keeps flowing until explicitly stopped

// Identifiers
my-variable       // Variables: lowercase, kebab-case
var/subvar        // Namespaces use /
Math.Sin          // Shards: Capitalized, dot-namespaced
```

## Variable Assignment & Data Flow

Think channels, not containers.

**Everything is a shard** - the syntax is just sugar:
```shards
0 >= x          // desugars to: Const(0) | Set(x)
1 > x           // desugars to: Const(1) | Update(x)
"hi" = msg      // desugars to: Const("hi") | Ref(msg)
5 | Add(3)      // Add is a shard, 5 is Const(5)
```

```shards
// Immutable channel (cannot be redirected)
"hello" = greeting
value | Ref(var-name)           // equivalent

// Mutable channel (can be redirected)  
0 >= counter
value | Set(var-name)           // equivalent

// Redirect flow (update mutable)
new-value > counter
new-value | Update(var-name)    // equivalent

// Append to sequence (tributary joining main flow)
item >> my-sequence
item | Push(my-sequence)        // equivalent

// Append to string
"more text" | AppendTo(my-string)
"prefix" | PrependTo(my-string)
```

## Data Types & Literals

```shards
// Primitives
"hello"                         // String
"""multi
line
string"""                       // Triple-quoted string (preserves newlines)
42                              // Integer
3.14                            // Float
true false                      // Booleans
none null                       // None/null
0x1A3F                          // Hex

// Vectors
@f2(1.0 2.0)                    // Float2
@f3(1.0 2.0 3.0)                // Float3
@f4(1.0 2.0 3.0 4.0)            // Float4
@i2(1 2)                        // Int2
@i3(1 2 3)                      // Int3
@i4(1 2 3 4)                    // Int4
@color(0.8 0.2 0.2)             // Color (RGB float)

// Sequences (arrays - can hold mixed types)
[1 2 3 4 5]
["mixed" 42 true @f3(1 2 3)]

// Tables (dictionaries/objects)
{name: "Alice" age: 30 active: true}

// Bytes
"hello" | StringToBytes
```

## Parameter Rules

**Critical**: Once you use a named parameter, ALL subsequent must be named.

```shards
// Positional parameters
Add(3)
Take(0)

// Named parameters
Http.Get(URL: "https://api.example.com" Headers: headers Timeout: 30)

// Mixed - positional MUST come first
Process(data Input: extra Output: result)    // OK
Process(Input: data extra)                   // ERROR! Named then unnamed
```

## Sequence Operations

```shards
// Declaration with type
Sequence(items Type: @type([Type::String]))
Sequence(any-items Type: @type([{none: Type::Any}]))

// Access (dot notation - literal indices only)
[1 2 3 4 5] = numbers
numbers.0                       // 1 (first element)
numbers.2                       // 3 (third element)
// Note: numbers.some-var does NOT work - must be literal index

// Access (dynamic - use Take for variable indices)
[1 2 3 4 5] >= numbers
numbers | Take(2)               // Get index 2 → 3
2 = idx
numbers | Take(idx)             // Same - variable index works with Take
numbers | Take([0 2 4])         // Multiple indices → [1 3 5]
numbers | RTake(0)              // From end → 5
numbers | Slice(1 3)            // Range → [2 3]
numbers | Slice(From: 1)        // From index to end
numbers | Slice(To: -2)         // From start, excluding last 2

// ⚠️ Take vs Limit vs Slice - don't confuse!
// Take(idx)   = element AT specific index (single element)
// Limit(n)    = first N elements (truncation)
// Slice       = range extraction (subsequence)
[1 2 3 4 5] | Take(2)           // → 3 (element at index 2)
[1 2 3 4 5] | Limit(2)          // → [1 2] (first 2 elements)
[1 2 3 4 5] | Slice(1 4)        // → [2 3 4] (indices 1,2,3)
[1 2 3 4 5] | Slice(From: 2)    // → [3 4 5] (from index 2 to end)
[1 2 3 4 5] | Slice(To: 3)      // → [1 2 3] (from start up to index 3)
[1 2 3 4 5] | Slice(From: 1 To: -1)  // → [2 3 4] (index 1 to second-to-last)

// Modification
6 >> numbers                    // Append
6 | Push(numbers)               // Same as above
item | Insert(0 numbers)        // Insert at index
PopFront(numbers)               // Remove & return first
DropFront(numbers)              // Remove first (no return)
Erase(2 numbers)                // Remove at index
Erase([0 2] numbers)            // Remove multiple indices
Clear(numbers)                  // Empty the sequence
other-seq | Extend(numbers)     // Extend with another sequence

// Iteration & Transformation
numbers | ForEach({ Mul(2) | Log })
numbers | Map({ Mul(2) })       // Transform, returns new sequence
numbers | Reverse               // Reverse order
numbers | Count                 // Length
```

## Table Operations

```shards
{name: "Alice" age: 30} >= person

// Access (dot notation - literal keys only)
person.name                     // "Alice"
person.age                      // 30
// Note: person.some-var does NOT work - must be literal key

// Access (dynamic - use Take for variable keys)
person | Take("name")           // "Alice" (or none if missing)
"name" = field-name
person | Take(field-name)       // "Alice" - variable key works with Take

// Get with default (for potentially undefined variables)
Get(maybe-undefined-var Default: {})

// Modification
"Bob" | Set(person "name")      // Set value
"NYC" | Update(person "city")   // Update existing
Erase("age" person)             // Remove key
other-table | Merge(person)     // Merge tables

// Dynamic table creation
Table(my-table)                 // Empty table
Table(typed-table Type: @type({name: Type::String age: Type::Int}))
```

## String Operations

```shards
// Joining & Formatting
["Hello" "World"] | String.Join              // "HelloWorld" (strings only)
["Score: " 42 " pts"] | String.Format        // "Score: 42 pts" (mixed types)

// Manipulation
"  hello  " | String.Trim                    // "hello"
"hello" | String.Starts(With: "he")          // true
"hello" | String.Ends(With: "lo")            // true
"a,b,c" | String.Split(",")                  // ["a" "b" "c"]
"a,b,c" | String.Split("," KeepSeparator: true)
"hello" | Replace(["l"] "L")                 // "heLLo"
"hello" | Slice(1 4)                         // "ell"

// Regex
"test@email.com" | Regex.Match("""(\w+)@(\w+)\.(\w+)""")

// Encoding
"hello" | StringToBytes
bytes | BytesToString
data | ToBase64
base64-str | FromBase64
bytes | ToHex
```

## Control Flow

```shards
// If-Then-Else
value | If(IsMore(5) 
  {"Greater" | Log} 
  {"LessOrEqual" | Log}
)

// When (no else branch)
value | When(IsMore(5) {"Greater" | Log})
value | WhenNot(IsMore(5) {"NotGreater" | Log})

// Multi-branch Cond
value | Cond([
  {IsMore(10)} {"Large" | Log}
  {IsMore(5)}  {"Medium" | Log}
  {true}       {"Small" | Log}    // default case
] Passthrough: false)

// Pattern Matching
status | Match([
  "success" {"OK" | Log}
  "error"   {"Failed" | Log}
  none      {"Unknown" | Log}     // default case
] Passthrough: false)

// Loops
Repeat({"tick" | Log} Times: 5)

0 >= sum
0 | ForRange(To: 10 {
  Add(sum) > sum
})

[1 2 3] | ForEach({
  Mul(2) | Log
})

// Early exit
Repeat({
  When(condition Return)          // Exit loop early
} Times: 100)

// Until condition
Repeat({
  do-work
} Until: {done-condition})

// Pass (do nothing - useful in Match/Cond)
Match([
  "skip" Pass
  none   {"handle" | Log}
])
```

## Type System

```shards
// Type definitions
@type({name: Type::String age: Type::Int})
@type([Type::String])                        // Sequence of strings
@type([{none: Type::Any}])                   // Sequence of any

// Available types
Type::String Type::Int Type::Float Type::Bool
Type::Any Type::Bytes Type::Image Type::Sequence
Type::Object                                 // For Shards objects

// Object types
@type(Type::Object ObjectName: "Http.Stream")
@type(Type::Object ObjectName: "LLM.Chat")

// Runtime type checking
value | IsString                             // Returns bool
value | IsInt
value | IsFloat
value | IsBool  
value | IsTable
value | IsSeq
value | IsNone
value | IsNotNone
value | IsImage

// Type assertions (fail if wrong type)
value | ExpectString
value | ExpectInt
value | ExpectFloat
value | ExpectBool
value | ExpectTable
value | ExpectSeq
value | ExpectBytes
value | ExpectImage
value | Expect(@type({name: Type::String}))

// Conversions
42 | ToString                                // "42"
"42" | ToInt                                 // 42
42 | ToFloat                                 // 42.0
value | ToAny                                // Erase type info
table | ToAnyTable                           // Generic table
data | ToJson                                // JSON string
json-str | FromJson                          // Parse JSON
```

## Comparison & Logic

```shards
// Equality
value | Is(42)
value | IsNot(42)

// Comparison
value | IsMore(10)
value | IsLess(10)
value | IsMoreEqual(10)
value | IsLessEqual(10)

// Logical operators (for booleans in flow)
is-valid | And | is-enabled      // logical AND - true if both true
cond1 | Or | cond2               // logical OR - true if either true  
condition | Not                  // Negate

// Multi-condition chaining
a | And | b | And | c            // all must be true
x | Or | y | Or | z              // any can be true

// ⚠️ IMPORTANT: And/Or have flow control side effects!
// - And stops flow and restarts from top if condition is false
// - Or stops flow and restarts from top if condition is true
// Use them ONLY inside conditional contexts (If, When, Cond predicates)
// Using them in regular flow can cause unexpected restarts!

// ✓ CORRECT - inside conditional predicate
value | When({IsMore(5) | And | IsLess(10)} {"In range" | Log})

// ✗ CAREFUL - in regular flow, And/Or affect flow control
// some-bool | And | other-bool | Log  // may restart flow unexpectedly!

// Note: Math.And / Math.Or are BITWISE operators (see Math section)
// Don't confuse: And/Or (logical+flow) vs Math.And/Math.Or (bitwise)

// Boolean checks
flag | IsTrue
flag | IsFalse

// Assertions
value | Assert.Is(42)
value | Assert.IsNot(0)
```

## Math Operations

```shards
// Arithmetic (data flows through)
5 | Add(3)                       // 8
10 | Sub(3)                      // 7
4 | Mul(2)                       // 8
10 | Div(2)                      // 5
10 | Pow(2.0)                    // 100.0

// In-place increment/decrement
Inc(counter)                     // counter += 1
Dec(counter)                     // counter -= 1

// Clamping
value | Min(100)                 // Cap at 100
value | Max(0)                   // Floor at 0

// Math functions
angle | Math.Sin
angle | Math.Cos
value | Math.Floor
value | Math.Abs

// Bitwise (for integers - don't confuse with logical And/Or!)
1 | Math.LShift(4)               // Left shift
mask | Math.Xor(other)           // XOR
flags | Math.Or(0x04)            // Bitwise OR
mask | Math.And(0xFF)            // Bitwise AND
```

## Wires & Execution

Wires are **stateful coroutines** - not just functions. They maintain state between calls, can be suspended (with `Pause`), and their memory is tied to their lifetime.

### Memory Model

Shards has a simple, efficient memory model:
- **No manual memory management** - users never need to think about allocation/deallocation
- **Memory tied to wire lifetime** - when a wire ends, its memory is cleaned up
- **Looped wires recycle memory** - variables are reused each iteration, no new allocations
- **Warmup allocates, activation reuses** - heavy lifting happens once at warmup

This is why looped wires are so efficient - after the first iteration, everything is pre-allocated and just gets recycled.

```shards
@wire(game-loop {
  // These allocations happen ONCE at warmup
  Once({
    Sequence(entities Type: @type([Type::Any]))
    {} >= game-state
  })
  
  // This runs every frame with ZERO allocations
  // Variables are recycled, not reallocated
  update-entities
  render-frame
} Looped: true)
```

```shards
// Wire definition - wires are stateful coroutines
@wire(my-wire {
  "Hello" | Log
  42                             // Wire outputs last value
})

// Looped wire (runs continuously, memory recycled each iteration)
@wire(main-loop {
  update-game
} Looped: true)

// Pure wire (isolated scope, no access to parent variables)
@wire(isolated-wire {
  // Cannot see parent's variables
} Pure: true)

// Wire execution methods
Do(my-wire)                      // Run INLINE (not a coroutine), share parent state
Step(looped-wire)                // Step looped wire once, share state (coroutine)
Detach(my-wire)                  // Schedule on mesh, copied state (coroutine, non-blocking)
Spawn(my-wire)                   // Spawn clone for each item (multiple coroutines)
Branch([wire1 wire2])            // Create submesh, wires share parent state
Branch([wires] CaptureAll: true FailureBehavior: BranchFailure::Everything)
Expand(Wire: my-wire Size: 100)  // Create N copies, collect results
TryMany(Wire: my-wire Policy: WaitUntil::FirstSuccess)

// Dynamic wire execution
wire-var | ExpectWire
WireRunner(wire-var)
```

### Wire Execution Methods Comparison

| Method   | Uses Original Variables? | Restarts Wire? | Continues Parent? | Multiple Instances? |
| :------- | :----------------------- | :------------- | :---------------- | :------------------ |
| Do       | Yes                      | No             | Yes               | No                  |
| Step     | Yes                      | No             | Yes               | No                  |
| Detach   | No (copies)              | Yes            | Yes               | No                  |
| Spawn    | No (copies)              | Yes            | Yes               | Yes                 |
| Branch   | Yes                      | No             | Yes               | No                  |
| Expand   | No (copies)              | Yes            | Yes               | Yes                 |
| TryMany  | No (copies)              | Yes            | Yes               | Yes                 |

### Wire Control

```shards
// Wait for detached wire to complete
Detach(background-task)
Wait(background-task)

// Pause/Resume wires
Suspend(wire-name)               // Pause a wire
Resume(wire-name)                // Resume from where it paused

// Switch execution flow
SwitchTo(other-wire)             // Suspend current, switch to other

// Stop a wire
Stop                             // End current wire
Stop(wire-name)                  // End specific wire
```

### Wire State & Scope

```shards
// Parent wire with child access
@wire(parent {
  0 >= counter
  Do(child)                      // child can modify counter directly
  counter | Log                  // Shows child's changes
})

@wire(child {
  counter | Add(1) > counter     // Modifies parent's counter
})

// Detached wire gets COPIES of variables
@wire(parent {
  0 >= counter
  Detach(detached-child)
  counter | Log                  // Still 0 - detached worked on copy
})

@wire(detached-child {
  counter | Add(1) > counter     // Only affects local copy
})
```

## Templates (Reusable Code)

```shards
// Definition
@template(greet [name greeting] {
  [greeting " " name "!"] | String.Format | Log
})

// Usage
@greet("World" "Hello")          // Logs: "Hello World!"

// Templates can contain complex logic
@template(retry-operation [operation max-retries] {
  0 >= attempts
  none >= result
  Repeat({
    Maybe({
      operation > result
      Return                     // Success, exit
    } {
      Inc(attempts)
      When({attempts | IsMoreEqual(max-retries)} {
        "Max retries exceeded" | Fail
      })
    })
  } Times: max-retries)
  result
})
```

## Defines & Compile-Time

```shards
// Simple define
@define(api-url "https://api.example.com")
@define(max-retries 3)

// Define with type
@define(person-type @type({name: Type::String age: Type::Int}))

// Define table (expressions evaluated when used)
@define(headers {
  "Content-Type": "application/json"
  "Authorization": (["Bearer " api-key] | String.Join)
})

// Allow redefinition
@define(config-value 42 IgnoreRedefined: true)

// Compile-time evaluation with #()
@define(collision-mask #(1 | Math.LShift(4)))
@define(computed #(100 | Mul(2)))

// Compile-time conditionals
@if(@platform | IsNot("watchos") {
  // Code for non-watchOS
} {
  // Fallback code
})

@if(@apple-but-not-watch {
  WebKit.Fetch
} {
  Http.Get(url)
})
```

## Includes & Modules

```shards
// Basic include
@include("utils.shs")

// Include once (prevents double-inclusion)
@include("shared-types.shs" Once: true)

// Namespaced variables for organization
ext/api-key                      // External config
g/models                         // Global state
tools/working-dir                // Tool-specific
mcp/session-id                   // MCP server state
```

## Error Handling

```shards
// Try-catch pattern
Maybe({
  risky-operation
  "Success" | Log
} {
  "Error occurred" | Log
})

// Fail with message
condition | When(IsFalse {
  "Validation failed" | Fail
})

// Capture logs for error details
CaptureLog({
  Maybe({
    operation
  } {
    CurrentCaptureLog = error-log
    error-log | Log("Captured errors")
  })
} MinLevel: LogLevel::Error)

// Assertions
value | Assert.Is(expected)
value | Assert.IsNot(forbidden)
```

## Async Operations

```shards
// Await async operations
data | Await(ToJson)
json | Await(FromJson)

// Await block (multiple operations)
Await({
  LoadImage(path)
  ResizeImage(Width: 256)
  WriteJPG
  ToBase64
})

// Pause execution
Pause(1.0)                       // Pause 1 second

// Periodic execution
Once({
  periodic-task
} Every: 10.0)                   // Every 10 seconds

// Producer/Consumer pattern
audio-data | Produce("audio-channel")
Consume("audio-channel" @type([Type::Float]))
```

## HTTP Operations

```shards
// GET request
Http.Get(url Timeout: 30)

// POST request
payload | ToJson | Http.Post(
  url 
  Headers: {"Content-Type": "application/json"}
  Timeout: 30
  Retry: 3
  Backoff: 5
)

// Streaming
Http.Post(url Headers: headers Streaming: true FullResponse: true)
{Take("stream") = stream}
{Take("status") = status}
Http.Stream(stream) | BytesToString

// Server
Http.Server(Port: 8080 SSL: false Handler: handler-wire)
Http.Response(200 Headers: {"Content-Type": "text/plain"})
Http.Chunk(Headers: {"Content-Type": "text/event-stream"})
```

## File System

```shards
// Path operations
path | FS.IsFile
path | FS.IsAbsolute
[dir file] | FS.Join
path | FS.Filename(NoExtension: true)

// Read/Write
path | FS.Read                   // Read text
path | FS.Read(Bytes: true)      // Read bytes
path | FS.Write(content Overwrite: true)  // Write to path

// Embedded files (compile-time)
@read("data.json")               // Embed file content
@read("image.png" Bytes: true)   // Embed as bytes
```

## Database

```shards
[actor-id] | DB.Query(
  "SELECT * FROM users WHERE id = ?"
  AsRows: true
  Database: "/path/to/db.sqlite"
  ReadOnly: true
)
```

## Events System

```shards
// Send event
data | Events.Send("channel-name" session-id)

// Receive events
Events.Receive("channel-name" session-id) | ForEach({
  process-event
})

// Update event system (call in loop)
Events.Update("channel-name")
```

## Logging

```shards
// Basic
"message" | Log
value | Log("Label")

// With level and category
data | Log("Debug info" Level: LogLevel::Debug Name: "my-category")

// Simple message
Msg("Starting process")

// Common pattern: debug templates
value | @debug-log("context")    // Defined elsewhere as template
```

## JSON Handling

```shards
// Serialize
{name: "test" value: 42} | ToJson        // Usually needs Await
data | Await(ToJson)

// Parse
json-string | FromJson                    // Usually needs Await
json-string | Await(FromJson) | ExpectTable

// Common pattern
Maybe({
  input | Await(FromJson) | ExpectTable
} {
  "Invalid JSON" | Fail
})
```

## Passthrough & Sub Blocks

Passthrough allows data to pass through operations unchanged. Use `{}` sub blocks to save input, run operations, then restore the original input.

```shards
// Sub block preserves original value in flow
1 | {Add(2) | Log("Inside")}     // Logs 3, but...
  | Log("After")                 // Logs 1 (original preserved!)

// Practical use - debug logging without disrupting flow
user-data 
| {Log("Debug: processing")}     // Logs user-data, doesn't change flow
| ProcessUser
| {Log("Debug: done")}           // Logs processed result
| SaveUser

// Passthrough parameter on shards
1 | Match([
  1 {"One"}
  2 {"Two"}
] Passthrough: false)            // Returns "One" (match result)

1 | Match([
  1 {"One"}
  2 {"Two"}
] Passthrough: true)             // Returns 1 (original input passed through)
```

## Advanced Patterns

### Pipe-in-Params
```shards
// Pipes work directly in parameters (no extra parens needed)
orbit-angle | Add(time | Mul(0.5)) > orbit-angle

// Also in table literals
{
  a: 1 | Add(1 | Add(1))         // Computed: 3
  b: input | Transform
}
```

### The $0 Syntax
```shards
// $0 is a special variable injected ONLY by: ForEach, ForRange, Map
// It refers to the current element being processed

items | ForEach({ $0 | Log })              // $0 is each item
items | Map({ $0 | Mul(2) })               // $0 is each item, returns transformed seq
0 | ForRange(To: 10 { $0 | Log })          // $0 is current index (0-9)

// ForEach with TABLE input - get $0 (key) and $1 (value)
{name: "Alice" age: 30} | ForEach({
  $0 | ExpectString | Log("Key")           // "name", "age"
  $1 | Log("Value")                        // "Alice", 30 (mixed types - use ExpectX)
})

// $0 does NOT exist in other contexts - use explicit variable assignment
items | ForEach({ ExpectTable = item ... })  // alternative: assign to variable
```

### Const Shard
```shards
// Returns constant regardless of input
Match([
  "a" Const(1)
  "b" Const(2)
  none Const(0)
])
```

### Global Variables & Scope

```shards
// Local variable (default) - only visible in current wire
0 >= local-counter

// Global variable - visible across all wires on mesh
0 | Set(shared-counter Global: true)

// Access global from any wire
shared-counter | Add(1) > shared-counter

// Get with default (safe access)
Get(Name: maybe-undefined Default: 0)

// @define, @template, @wire declared at top-level are global
@define(config-value 42)

// BUT if declared inside a wire, they're scoped to that wire!
@wire(outer {
  @define(inner-only 123)        // Only visible inside 'outer'
  @template(local-helper [] {})  // Only visible inside 'outer'
  @wire(nested {})               // Only visible inside 'outer'
})
// inner-only, local-helper, nested are NOT accessible here
```

### Sub Blocks for Parallel Operations
```shards
// When multiple operations need same input independently
data | ForEach({
  {operation1 | Log}             // Gets original input
  {operation2 | Log}             // Gets same original input
})
```

## Mesh & Scheduling

```shards
@wire(main-loop {
  game-update
} Looped: true)

@mesh(root)
@schedule(root main-loop)
@run(root FPS: 60)
```

## Common Idioms

```shards
// Initialize once
Once({
  0 >= counter
  "" >= buffer
  Sequence(items Type: @type([Type::String]))
})

// Safe variable access with default
Get(maybe-undefined-var Default: "fallback")

// Chain type assertions
data | ExpectTable | Take("key") | ExpectString

// Conditional table field
When({has-value} {
  value | Set(table "optional-field")
})

// Alternative: inline condition
has-value | When(IsTrue {
  value | Set(table "field")
})

// Build response incrementally
"" >= output
"Header\n" | AppendTo(output)
content | AppendTo(output)
"Footer" | AppendTo(output)

// Process and filter
items | ForEach({
  ExpectTable = item                 // needed if items is dynamic/unknown type
  item.valid | ExpectBool            // needed if table is dynamic
  When(IsTrue {
    item | process >> results
  })
})

// If types are known at compose time, Expect calls are not needed:
typed-items | ForEach({
  = item                             // type already known
  item.valid | When(IsTrue {         // no ExpectBool needed
    item | process >> results
  })
})
```

## Platform-Specific Code

```shards
@if(@apple-but-not-watch {
  // iOS/macOS specific
  WebKit.Fetch
} {
  // Fallback
  Http.Get(url)
})

@if(@platform | Is("watchos") {
  "Limited mode" | Log
})

// App paths
Apple.AppGroupPath               // iOS/macOS app group
```

## Shards Lifecycle

When a Shards script runs, it goes through these stages:

1. **Read** - `.shs` file is read
2. **Parse** - Converted to AST format
3. **Construct** - Computational graph built (`#()` expressions evaluated here)
4. **Compose** - Types validated, optimizations applied
5. **Warmup** - Objects created, memory allocated (once per wire lifetime)
6. **Activation** - Actual execution (minimal work due to warmup)
7. **Cleanup** - Memory freed, resources cleaned

The **Warmup** stage is key to Shards' efficiency - for looped wires, warmup happens once, then activation reuses those resources each iteration.
