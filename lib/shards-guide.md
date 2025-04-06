# Shards Programming Language: Comprehensive Guide

## Core Concept: Data Flow Programming

Shards is a data flow programming language where data flows through operations using the pipe `|` operator. Think of it as a river system rather than containers:

> You might already know an old version of the syntax that was inspired by LISP but actually now Shards uses a completely different custom syntax!

```shards
; Traditional imperative thinking (WRONG):
x = 5          ❌ Not a container being filled
x += 1         ❌ Not incrementing a stored value

; Shards data flow thinking (CORRECT):
5 >= x         ✅ Creates a channel named 'x' and flows 5 into it
x | Add(1) > x   ✅ Takes flow from x, adds 1, and channels it back
```

## Basic Syntax

- **Comments**: Begin with semicolon `;`
- **Operations**: Chain with pipe `|`
- **No semicolons** at line ends
- **No braces** for basic code blocks
- **Whitespace** is flexible

## Variables and Data Flow

### Variable Assignment

```shards
; Immutable assignment (=)
5 = x                       ; Immutable integer
"Hello" = greeting          ; Immutable string
@f3(1.0 2.0 3.0) = position  ; Immutable vector

; Mutable assignment (>=)
10 >= counter               ; Mutable integer
[1 2 3] >= numbers          ; Mutable sequence
{name: "Alice" age: 30} >= person  ; Mutable table
```

### Updating Variables

```shards
; Update mutable variables (>)
counter | Add(5) > counter     ; Add 5 to counter
4 >> numbers                 ; Append to sequence (shorthand)
"Bob" | Update(person "name")  ; Update table field
```

## Data Types

### Basic Types

```shards
42                ; Integer
3.14              ; Float
"Hello"           ; String
true, false       ; Boolean
none              ; Null/None
0x1A3F            ; Hexadecimal
```

### Vector Types

```shards
@f2(1.0 2.0)               ; 2D float vector
@f3(1.0 2.0 3.0)           ; 3D float vector
@f4(1.0 2.0 3.0 4.0)       ; 4D float vector
@i2(1 2), @i3(1 2 3), @i4(1 2 3 4)  ; Integer vectors
```

### Sequences

```shards
[1 2 3 4]         ; Sequence of integers
["a" "b" "c"]     ; Sequence of strings
[1 "mixed" true]  ; Mixed types (valid)

; Accessing elements
sequence | Take(2)       ; Get element at index 2
sequence | RTake(0)      ; Get last element
sequence | Slice(1 3)    ; Get elements from index 1 to 3 (exclusive)
sequence | Take([0 2 4]) ; Get elements at indices 0, 2, and 4

; Modifying
value >> sequence        ; Append to sequence
Erase(2 sequence)        ; Remove element at index 2
Erase([0 1] sequence)    ; Remove elements at multiple indices
```

### Tables

```shards
{name: "Alice" age: 30 city: "NY" optional: none} >= person  ; Table creation

; or in full JSON compatible format
{
  "name": "Alice",
  "age": 30,
  "city": "NY",
  "optional": null
} >= person

; Accessing
person:name              ; Access via colon syntax
person | Take("age")     ; Access via Take

; Modifying
"Bob" | Update(person "name")   ; Update field
Erase("city" person)            ; Remove field
Erase(["name" "age"] person)    ; Remove multiple fields
```

## String Operations

```shards
; String.Join - combine string sequences
["Hello" "World"] | String.Join            ; "HelloWorld"
["Hello" " " "World"] | String.Join      ; "Hello World"

; String.Format - mix strings with other types
["Score: " 42] | String.Format           ; "Score: 42"
["Pos: " @f3(1 2 3)] | String.Format     ; "Pos: @f3(1 2 3)"
```

## Control Flow

### If / When

```shards
; If with then/else
value | If({IsMore(10)}
  {"Greater than 10" | Log}
  {"Less or equal to 10" | Log}
)

; When (if without else)
value | When({IsMore(10)} {
  "Greater than 10" | Log
})
```

### Loops

```shards
; Repeat a specified number of times
Repeat({
  "Repeated action" | Log
} Times: 3)

; ForEach on sequence elements
[1 2 3] | ForEach({
  Mul(2) | Log
})
```

### Pattern Matching

```shards
value | Match([
  "A" {"Matched A" | Log}
  "B" {"Matched B" | Log}
  none {"No match" | Log}  ; Default case
])

; or with passthrough off, we flow the output of the match to the next operation
value | Match([
  "A" {"Matched A"} ; value & action shards
  "B" {"Matched B"} ; value & action shards
  none {"No match"}  ; Default case
] Passthrough: false) | Log

; using the even more flexible Cond shard
value | Cond([
  {Is("A")} {"Matched A" | Log} ; condition shards & action shards
  {Is("B")} {"Matched B" | Log} ; condition shards & action shards
  {true} {"No match" | Log}  ; Default case
])
```

### Initialization

```shards
; One-time initialization block
Once({
  0 >= counter
  "" >= message
})
```

## Sub Blocks for Multiple Operations

```shards
; When multiple operations need to process the same input
[1 2 3] | ForEach({
  {Mul(2) | Log}  ; Each gets original input
  {Add(5) | Log}  ; Processes same input independently
})
```

## Operation Grouping with Parentheses

```shards
; Without grouping: (5+3)*2 = 16
5 | Add(3) | Mul(2)

; With grouping: 5+(3*2) = 11
5 | Add((3 | Mul(2)))

; Nested grouping: 10+(5*(2+1)) = 25
10 | Add((5 | Mul((2 | Add(1)))))

; Within seqs, it will be applied to each element
5 |[
  (Add(1)) ; 6
  (Mul(2)) ; 12
] = my-seq

; Within tables, it will be applied to each element
5 | {
  a: [
    (Add(1)) ; 6
    (Mul(2)) ; 12
  ]
} = my-table
```

## Error Handling

```shards
; Maybe block for error handling (try-catch equivalent)
Maybe({
  ; Try block - code that might cause error
  "42" | FromJson | Log
} {
  ; Catch block - runs only if error occurs
  "Error occurred" | Log
})
```

## Templates for Code Reuse

```shards
; Define a template with parameters
@template(greet [name] {
  ["Hello, " name "!"] | String.Format
})

; Use the template, notice template calls are always prefixed with @ otherwise, Shards will interpret the name as a variable
@greet("Alice") | Log  ; Outputs: "Hello, Alice!"

; Template with multiple parameters
@template(math-op [a b] {
  a | Add(b)
})

@math-op(5 10) | Log  ; Outputs: 15

; Or using shards natural flow syntax
@template(math-op [b] {
  Add(b)
})

5 | @math-op(10) | Log  ; Outputs: 15
```

## Wire Definitions and State Management

```shards
; Define a wire with its own state
@wire(main-wire {
  0 >= counter
  "Initial" = message
  
  counter | Log
  counter | Add(1) > counter
  counter | Log
})
```

### Child Wires and Variable Scope

```shards
@wire(parent {
  0 >= counter
  
  ; Child with direct access to parent variables
  Do(child)
  counter | Log  ; Shows child's modifications
  
  ; Detached child with copies of variables
  Detach(independent-child)
  counter | Log  ; Unchanged by independent child
})

@wire(child {
  ; Modifies parent's counter directly
  counter | Add(5) > counter
})

@wire(independent-child {
  ; Modifies only its local copy
  counter | Add(10) > counter
})
```

### Global Variables

```shards
; Create global variables accessible from any wire
0 | Set(global-counter Global: true)
"shared" | Set(global-message Global: true)

; Access globals from any wire
@wire(some-wire {
  global-counter | Add(1) > global-counter
  global-message | Log
})
```

## Parameter Rules

```shards
; Basic unnamed parameters
Add(5)

; Once you use a named parameter, all subsequent must be named
Http.Get(
  URL: "https://api.example.com"
  Headers: headers
  Timeout: 30
)

; Error - mixing unnamed after named
Http.Get(URL: "https://api.example.com" headers)  ; Error!
```

## Concurrency

```shards
; Detach for non-blocking execution
@wire(background-task {
  Pause(5.0)  ; Simulates long operation
  "Task complete" | Log
})

@wire(main {
  "Main running" | Log
  Detach(background-task)  ; Non-blocking
  "Continued immediately" | Log
})
```

## Common Patterns and Practices

1. **Think in flows, not containers**
2. **Use meaningful variable names in kebab-case**
3. **Shards are Capitalized, variables are lowercase**
4. **Initialize all global variables in a single wire**
5. **Use `Do(wire)` for shared state, `Detach(wire)` for isolation**
6. **Use Tables for structured data**
7. **Use Maybe for robust error handling**
8. **Use templates for reusable code patterns**

## Program Structure Example

```shards
; Wire definitions
@wire(initialization {
  0 | Set(global-counter Global: true)
  ; Initialize other globals...
})

@wire(main-logic {
  ; Application logic here
  Do(sub-process)
})

@wire(sub-process {
  ; Handle part of the logic
})

; Start the program
Do(initialization)
Do(main-logic)
```

Remember: In Shards, variables are not containers holding values but channels directing data flow. The pipe `|` operator shows how data flows through your system.
