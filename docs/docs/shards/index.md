---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Introduction

Shards aims to be a meta language or a visual language rather then a pure programming language. To nicely describe
wires and the data they contain, we adopt a simple language based on EDN and Clojure.

Notice that in theory any language can be used, this was just a pragmatic choice. In fact you will also find wires written in C++ digging the Shards codebase.

One can think of it as a comprehensible JSON on steroids that can be evaluated or the Ansible of general programming.

The execution of Shards scripts is meant to be fast: the scripting language is just a description of the computational graph.
The runtime itself is completely detached.

??? note "Running code samples"
    Many shard code samples (like for the shard [Take](/General/Take)) do not include the wire/mesh context they need to run. To run these samples you need to navigate to `../shards/docs/samples/`and use the following command to provide the necessary boilerplate code to these samples.

    ```
        cd docs/samples
        ../../build/shards.exe run_sample.edn --file "<relative-path-of-sample.edn>"
    ```

## Some examples

### Super simple example

=== "Code"

    ```clojure linenums="1"
    ; define a mesh
    (defmesh main)
    ; define a simple hello world looped wire
    (defloop hello-world
      (Msg "Hello world"))
    ; schedule the wire for exection
    (schedule main hello-world)
    ; run the mesh at 1 second intervals
    (run main 1.0) ; <-- execution will block here
    ```

=== "Output"

    ```
    [info] [2021-04-04 16:43:22.290] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:23.299] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:24.291] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:25.289] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:26.291] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:27.299] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:28.296] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:29.292] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:30.295] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:31.298] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:32.298] [T-148944] [logging.cpp::94] [hello-world] Hello world
    ```

### Another example

=== "Code"

    ```clojure linenums="1"
    ; define a mesh
    (defmesh main)
    ; define a simple counter looped wire
    (defloop counter
    ; Setup runs only once at wire start
    (Setup
    0 >= .counter)
    ; get counter variable and print
    .counter (Log)
    ; increase the counter
    (Math.Inc .counter)
    ; notice that Math.Inc will passthrough the value
    ; so the value will be before increment
    ; if we wanted to pick up the increment uncomment the next line
    ; .counter
    (When (Is 10) (Stop)))
    ; schedule the wire for exection
    (schedule main counter)
    ; run the mesh at 1 second intervals
    (run main 1.0) ; <-- execution will block here
    ```

=== "Output"

    ```
    [info] [2021-04-04 19:00:43.045] [T-244992] [logging.cpp::53] [counter] 0
    [info] [2021-04-04 19:00:44.055] [T-244992] [logging.cpp::53] [counter] 1
    [info] [2021-04-04 19:00:45.058] [T-244992] [logging.cpp::53] [counter] 2
    [info] [2021-04-04 19:00:46.057] [T-244992] [logging.cpp::53] [counter] 3
    [info] [2021-04-04 19:00:47.049] [T-244992] [logging.cpp::53] [counter] 4
    [info] [2021-04-04 19:00:48.057] [T-244992] [logging.cpp::53] [counter] 5
    [info] [2021-04-04 19:00:49.046] [T-244992] [logging.cpp::53] [counter] 6
    [info] [2021-04-04 19:00:50.049] [T-244992] [logging.cpp::53] [counter] 7
    [info] [2021-04-04 19:00:51.058] [T-244992] [logging.cpp::53] [counter] 8
    [info] [2021-04-04 19:00:52.054] [T-244992] [logging.cpp::53] [counter] 9
    [info] [2021-04-04 19:00:53.055] [T-244992] [logging.cpp::53] [counter] 10
    ```

### A more complex example

Showing how we mix the Clojure-like language as a template language

=== "Code"

    ```clojure linenums="1"
    ; define a mesh
    (defmesh main)
    ; define a counter template
    (defn make-counter [start]
    ; this time we use the Wire symbol as we want to make a function
    (Wire
    ; customize the wire's name
    (str "counter-" start) :Looped
    ; Setup runs only once at wire start
    (Setup
        ; set start with the start function argument
        start >= .counter)
    ; get counter variable and print
    .counter (Log)
    ; increase the counter
    (Math.Inc .counter)
    ; notice that Math.Inc will passthrough the value
    ; so the value will be before increment
    ; if we wanted to pick up the increment uncomment the next line
    ; .counter
    ; also at wire definition time compute the end value
    ; (+) is not a shard, it will be evaluated only once when composing the wire
    (When (Is (+ start 10)) (Stop))))
    ; schedule 2 wires for exection
    (schedule main (make-counter 0))
    (schedule main (make-counter 10))
    ; run the mesh at 1 second intervals
    (run main 1.0) ; <-- execution will block here
    ```

=== "Output"

    ```
    [info] [2021-04-04 21:05:40.121] [T-285088] [logging.cpp::53] [counter-0] 0
    [info] [2021-04-04 21:05:40.122] [T-285088] [logging.cpp::53] [counter-10] 10
    [info] [2021-04-04 21:05:41.133] [T-285088] [logging.cpp::53] [counter-0] 1
    [info] [2021-04-04 21:05:41.133] [T-285088] [logging.cpp::53] [counter-10] 11
    [info] [2021-04-04 21:05:42.130] [T-285088] [logging.cpp::53] [counter-0] 2
    [info] [2021-04-04 21:05:42.130] [T-285088] [logging.cpp::53] [counter-10] 12
    [info] [2021-04-04 21:05:43.129] [T-285088] [logging.cpp::53] [counter-0] 3
    [info] [2021-04-04 21:05:43.129] [T-285088] [logging.cpp::53] [counter-10] 13
    [info] [2021-04-04 21:05:44.135] [T-285088] [logging.cpp::53] [counter-0] 4
    [info] [2021-04-04 21:05:44.135] [T-285088] [logging.cpp::53] [counter-10] 14
    [info] [2021-04-04 21:05:45.133] [T-285088] [logging.cpp::53] [counter-0] 5
    [info] [2021-04-04 21:05:45.133] [T-285088] [logging.cpp::53] [counter-10] 15
    [info] [2021-04-04 21:05:46.128] [T-285088] [logging.cpp::53] [counter-0] 6
    [info] [2021-04-04 21:05:46.128] [T-285088] [logging.cpp::53] [counter-10] 16
    [info] [2021-04-04 21:05:47.135] [T-285088] [logging.cpp::53] [counter-0] 7
    [info] [2021-04-04 21:05:47.135] [T-285088] [logging.cpp::53] [counter-10] 17
    [info] [2021-04-04 21:05:48.133] [T-285088] [logging.cpp::53] [counter-0] 8
    [info] [2021-04-04 21:05:48.133] [T-285088] [logging.cpp::53] [counter-10] 18
    [info] [2021-04-04 21:05:49.134] [T-285088] [logging.cpp::53] [counter-0] 9
    [info] [2021-04-04 21:05:49.134] [T-285088] [logging.cpp::53] [counter-10] 19
    [info] [2021-04-04 21:05:50.121] [T-285088] [logging.cpp::53] [counter-0] 10
    [info] [2021-04-04 21:05:50.123] [T-285088] [logging.cpp::53] [counter-10] 20
    ```

## Valid data types for shards

This section documents all the valid data types that are accepted by various shards either as their input value or as their parameter values. These data types also apply to the output created by any shard.

Valid data types for every shard are listed under the `Type` column of their Parameters, Input, and Output sections (types are enclosed within parenthesis and if multiple types apply then they are separated by a space).

??? note "Compound types"
    While this section lists the simple (or primitive) data types, you can combine these to create compound data types. For example, combining `Int`, `String`, and `Seq`, can give you a sequence of sequences (`(Seq [(Seq)])`), a sequence of integers and strings (`(Seq [(Int)] [(String)])`), and so on.

??? note "Why types?"
    Types are helpful as they reduce errors in programming. They are also very useful in visual programming as type-matching can be used to reduce the dropdown options when prompting a user on what shard to use next (depending on which shard's input type matches with the current shard's output type).

### Any

Type `Any` indicates that all valid data types are allowed.

For example, `Any` as the allowed data type for input and `:Value` parameter of shard [`(All)`](https://docs.fragcolor.xyz/shards/General/All/) means that `(All)` accepts and compares across all data types.

```clojure linenums="1"
(All
  :Value [(Any)]
)
```

`(All)` compares the input and`:Value` parameter values and returns `true` only if both the value and data type of these entities is equal/same.

=== "Code"

    ```clojure linenums="1"
    [4 5 6] (All :Value [4 5 6])
    (Log)   ;; value and type match => true

    "I'm a string" >= .var1
    "I'm a string" >= .var2
    .var1 (All :Value .var2)
    (Log)   ;; value and type match => true

    "I'm a string" >= .var3
    "I'm a different string" >= .var4
    .var3 (All :Value .var4)
    (Log)   ;; value mismatch => false

    (Float 4.0) >= .var5
    (Int 4) >= .var6
    .var5 (All :Value .var6)
    (Log)   ;; type mismatch => false
    ```

=== "Output"

    ```
    [info] [2022-07-22 13:05:25.848] [T-18072] [logging.cpp::55] [mywire] true
    [info] [2022-07-22 13:05:25.861] [T-18072] [logging.cpp::55] [mywire] true
    [info] [2022-07-22 13:05:25.862] [T-18072] [logging.cpp::55] [mywire] false
    [info] [2022-07-22 13:05:25.864] [T-18072] [logging.cpp::55] [mywire] false
    ```

### None

Type `None` indicates that no data type is expected. This implies that no value is expected.

For example, `None` as one of the valid data types for `:Max` parameter in shard [`(RandomInt)`](https://docs.fragcolor.xyz/shards/General/RandomInt/) means that setting a value for this parameter is not mandatory.

```clojure linenums="1"
(RandomInt
:Max [(None) (Int) (ContextVar [(Int)])]
)
```

`(RandomInt)` generates a random integer and the `:Max` parameter is the upper limit (not inclusive) of the value that can be generated. So it makes sense to have `None` as one of the valid types for this `:Max` parameter for cases when you do not want an upper limit on the random integer.

=== "Code"

    ```clojure linenums="1"
    (RandomInt 8)
    (Log)   ;; max int that can be generated is 7
    
    (RandomInt)
    (Log)   ;; no upper limit on the generated int
    ```

=== "Output"

    ```
    [info] [2022-07-22 13:45:03.282] [T-19992] [logging.cpp::55] [mywire] 4
    [info] [2022-07-22 13:45:03.293] [T-19992] [logging.cpp::55] [mywire] 311828859
    ```

### Enum

`Enum` stands for enumerated data type. 

What this means is that the value you pass to an enumerated variable can only take certain 'states' or named constant values.

For example, in [`(Math.Mean)`](https://docs.fragcolor.xyz/shards/Math/Mean/) the value for `:Kind` parameter needs to be of type `Enum`.

```clojure linenums="1"
(Math.Mean
  :Kind [(Enum)]
)
```

`(Math.Mean)` computes the mean of a sequence of floating-point numbers. But there are three kinds of means - Arithmetic mean, Geometric mean, and Harmonic mean.

So the parameter `:Kind` is defined as an enum variable with these three fixed states :
` :Kind = {Arithmtic mean, Geometric mean, Harmonic mean}`

And hence `:Kind` expects a value that matches one of its possible states. In other words the value you pass in for `:Kind` needs to be an enumerated data type.

In simple terms it just means that you pass in one of the allowed named constant values. Anything else will fail validation.

=== "Code"

    ```clojure linenums="1"
    [2.0 10.0]
    (Math.Mean :Kind Mean.Arithmetic)
    (Log)   ;; AM => 6

    [2.0 10.0]
    (Math.Mean :Kind Mean.Geometric)
    (Log)   ;; GM => 4.47214

    [2.0 10.0]
    (Math.Mean :Kind Mean.Harmonic)
    (Log)   ;; HM => 3.33333
    
    [2.0 10.0]
    (Math.Mean 
    ;; AM is default, anything else will throw error
        ;; :Kind 123
        )   
    (Log)   ;; AM => 6
    ```

=== "Output"

    ```
    [info] [2022-07-22 15:35:00.868] [T-15316] [logging.cpp::55] [mywire] 6
    [info] [2022-07-22 15:35:00.881] [T-15316] [logging.cpp::55] [mywire] 4.47214
    [info] [2022-07-22 15:35:00.882] [T-15316] [logging.cpp::55] [mywire] 3.33333
    [info] [2022-07-22 15:35:00.883] [T-15316] [logging.cpp::55] [mywire] 6
    ```

### Bool

Type `Bool` allows only two values - `true` or `false`.
In that sense it can be thought of as a special case of an [`Enum`](#-enum) data type.

Consider the shard [`(Is)`](https://docs.fragcolor.xyz/shards/General/Is/). This shard compares its input and the value in the `:Value` parameter for equality. After the comparison it needs to communicate its result in a yes/no format (yes values are equal; no values are not equal).

```clojure linenums="1"
(Is
  :Value [(Any)]
)
```

To allow the shard to do this its output type is defined as a `Bool`. If the values are equal this shard emits `true` as its output, if the values are inequal it emits `false`. No other output is allowed.

=== "Code"

    ```clojure linenums="1"
    100 (Is :Value (* 10 10))
    (Log)   ;; Is equal => true

    [20] (Is :Value 20)
    (Log)   ;; Is not equal => false
    ```

=== "Output"

    ```
    [info] [2022-07-22 18:38:24.383] [T-25360] [logging.cpp::55] [mywire] true
    [info] [2022-07-22 18:38:24.395] [T-25360] [logging.cpp::55] [mywire] false
    ```

### Int

Type `Int` (or its alias `int`) defines a 64-bit signed integer.

64 bits of memory allows this data type to store integer values ranging from -9,223,372,036,854,775,808 to +9,223,372,036,854,775,807 (no decimals).

An `Int` value looks like this: `(Int 2)`.
It may also be represented without the keyword `Int`, with just the integer value: `2`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (Int 3) (Math.Add (Int 2))
    (Log)   ;; Int output => 5

    3 (Math.Add 2)
    (Log)   ;; Int output => 5
    ```

=== "Output"

    ```
    [info] [2022-07-22 21:20:18.771] [T-4568] [logging.cpp::55] [mywire] 5
    [info] [2022-07-22 21:20:18.782] [T-4568] [logging.cpp::55] [mywire] 5
    ```

### Int2

Type `Int2` (or its alias `int2`) defines a vector of two [`Int`](#-int) type numbers.

A vector can be thought of as a group or list of items that are considered together for processing.

An `Int2` type value looks like this: `(Int2 3 -5)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (Int2 4 5) (Math.Add (Int2 6 9))
    (Log)   ;; Int2 output => (10, 14)
    ```

=== "Output"

    ```
    [info] [2022-07-22 21:22:26.381] [T-17748] [logging.cpp::55] [mywire] (10, 14)
    ```

### Int3

Type `Int3` (or its alias `int3`) defines a vector of three 32-bit signed integers.

32 bits of memory for each number allows this data type to store integer values ranging from -2147483648 to +2147483647 (no decimals).

An `Int3` type value looks like this: `(Int3 2 4 -4)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (Int3 1 10 99) (Math.Add (Int3 99 90 1))
    (Log)   ;; Int3 output => (100, 100, 100)
    ```

=== "Output"

    ```
    [info] [2022-07-22 21:24:38.132] [T-25580] [logging.cpp::55] [mywire] (100, 100, 100)
    ```

### Int4

Type `Int4` (or its alias `int4`) is like type [`Int3`](#-int3) but a vector of four 32-bit signed integers instead.

An `Int4` type value looks like this: `(Int4 1 -4 0 3)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (Int4 3 6 9 4)
    (Math.Add (Int4 6 3 0 5))
    (Log)   ;; Int4 output => (9, 9, 9, 9)
    ```

=== "Output"

    ```
    [info] [2022-07-22 21:27:58.115] [T-20884] [logging.cpp::55] [mywire] (9, 9, 9, 9)
    ```

### Int8

Type `Int8` (or its alias `int8`) defines a vector of eight 16-bit signed integers.

16 bits of memory for each number allows this data type to store integer values ranging from −32,768 to +32,767 (no decimals).

However, unlike `(Int)`, `(Int2)`, `(Int3)`, and `(Int4)`), there is no keyword yet defined or supported for `Int8` for Shards scripting environment.

An `Int8` type value though, still works like the other integer type values (described above) in the context of a Shard's input, output, or parameter value data.

So, if a vector of 8 integers is fed into `(Math.Add)` as input and as its `:Operand`, the shard would add them up and output it as another vector of 8 integers. 

### Int16

Type `Int16` (or its alias `int16`) defines a vector of sixteen 8-bit signed integers.

8 bits of memory for each number allows this data type to store integer values ranging from −128 to +127 (no decimals).

However, unlike `(Int)`, `(Int2)`, `(Int3)`, and `(Int4)`), there is no keyword yet defined or supported for `Int16` for Shards scripting environment.

An `Int16` type value though, still works like the other integer type values (described above) in the context of a Shard's input, output, or parameter value data.

So, if a vector of 16 integers is fed into `(Math.Add)` as input and as its `:Operand`, the shard would add them up and output it as another vector of 16 integers. 

### Float

Type `Float` (or its alias `float`) defines a 64-bit signed floating point number.

Floating point means it has the capability to store a decimal point and hence supports decimal numbers.

64 bits of memory allows this data type to support a very large range of positive and negative decimal numbers (16 significant decimal digits and an exponent range of −383 to +384).

A `Float` value looks like this: `(Float 2.53)`.
It may also be represented without the keyword `Float`, with just the floating-point value: `2.53`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (Float 2.4) (Math.Add (Float 1.43))
    (Log)   ;; Float output => 3.83

    2.4 (Math.Add 1.43)
    (Log)   ;; Float output => 3.83
    ```

=== "Output"

    ```
    [info] [2022-07-22 22:06:32.856] [T-20204] [logging.cpp::55] [mywire] 3.83
    [info] [2022-07-22 22:06:32.873] [T-20204] [logging.cpp::55] [mywire] 3.83
    ```

### Float2

Type `Float2` (or its alias `float2`) defines a vector of two [`Float`](#-float) type numbers.

A vector can be thought of as a group or list of items that are considered together for processing.

A `Float2` type value looks like this: `(Float2 3.4 -5.0)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (Float2 4.1 5.0) (Math.Add (Float2 6.3 9.2))
    (Log)   ;; Float2 output => (10.4, 14.2)
    ```

=== "Output"

    ```
[info] [2022-07-22 22:10:00.688] [T-24616] [logging.cpp::55] [mywire] (10.4, 14.2)
    ```

### Float3

Type `Float3` (or its alias `float3`) defines a vector of three 32-bit signed floating point numbers.

Floating point means it has the capability to store a decimal point and hence supports decimal numbers.

32 bits of memory allows this data type to support a large range of positive and negative decimal numbers (7 significant decimal digits and an exponent range of −101 to +90).

A `Float3` type value looks like this: `(Float3 2.9 -4.23 7.83)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (Float3 1.2 3.4 5.6) (Math.Add (Float3 6.5 4.3 2.1))
    (Log)   ;; Float3 output => (7.7, 7.7, 7.7)
    ```

=== "Output"

    ```
    [info] [2022-07-22 22:19:36.923] [T-16128] [logging.cpp::55] [mywire] (7.7, 7.7, 7.7) 
    ```

### Float4

Type `Float4` (or its alias `float4`) is like type [`Float3`](#-float3) but a vector of four 32-bit signed floating point numbers instead.

A `Float4` type value looks like this: `(Float4 -8.84 38.2 4.7 0.4)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (Float4 3.1 6.4 9.2 4.6)
    (Math.Add (Float4 6.8 3.5 0.9 5.3))
    (Log)   ;; Int4 output => (9.9, 9.9, 9.9, 9.9)
    ```

=== "Output"

    ```
    [info] [2022-07-22 22:23:24.076] [T-25152] [logging.cpp::55] [mywire] (9.9, 9.9, 10.1, 9.9)
    ```

--8<-- "includes/license.md"
