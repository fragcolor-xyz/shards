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
        ../../build/shards.exe run-sample.edn --file "<relative-path-of-sample.edn>"
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

Valid data types for every shard are listed under the `Type` column of their Parameters, Input, and Output sections (types are enclosed within parentheses and if multiple types apply then they are separated by a space).

??? note "Compound types"
    While this section lists the simple (or primitive) data types, you can combine these to create compound data types. For example, combining `Int`, `String`, and `Seq`, can give you a sequence of sequences (`(Seq [(Seq)])`), a sequence of integers and strings (`(Seq [(Int)] [(String)])`), and so on.

??? note "Why types?"
    Types are helpful as they reduce errors in programming. They are also very useful in visual programming as type-matching can be used to reduce the dropdown options when prompting a user on what shard to use next (depending on which shard's input type matches with the current shard's output type).

!!! note
    While all the following types are available internally to various shards, only a few are currently accessible in the Shards scripting environment. Consequently, only these types have keywords/aliases.

### Any

Type **Any** indicates that all valid data types are allowed.

!!! note
    No keyword or alias exists for this type. [`(Any)`](https://docs.fragcolor.xyz/shards/General/Any/) is an unrelated shard.

For example, **Any** as the allowed data type for input and `:Value` parameter of shard [`(All)`](https://docs.fragcolor.xyz/shards/General/All/) means that `(All)` accepts and compares across all data types.

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

### Array

Type **Array** is a collection of values that can be accessed directly via indexes (since items are indexed by contiguous integers).

!!! note
    No keyword or alias exists for this type.

Also called a vector. Looks exactly like the `seq` type, but an **Array** type's items are accessible by index (and hence accessible randomly). Example of an **Array** type would be: `[43 6 1]`.

### Audio

Type **Audio** is uncompressed audio data.

!!! note
    No keyword or alias exists for this type.

Examples of shards that use this type are [`(Audio.Oscillator)`](https://docs.fragcolor.xyz/shards/Audio/Oscillator/), [`(Audio.ReadFile)`](https://docs.fragcolor.xyz/shards/Audio/ReadFile/), and [`(Audio.WriteFile)`](https://docs.fragcolor.xyz/shards/Audio/WriteFile/) all of which generate **Audio** type data as their output.

??? note "Supported formats"
    Shards supports the audio formats WAV, MP3, OGG, and FLAC.

### Bool

Type **Bool** allows only two values - `true` or `false`.
In that sense it can be thought of as a special case of an [`enum`](#enum) data type.

!!! note
    No keyword or alias exists for this type.

Consider the shard [`(Is)`](https://docs.fragcolor.xyz/shards/General/Is/). This shard compares its input and the value in the `:Value` parameter for equality. After the comparison it needs to communicate its result in a yes/no format (yes values are equal; no values are not equal).

```clojure linenums="1"
(Is
  :Value [(Any)]
)
```

To allow the shard to do this its output type is defined as a **Bool**. If the values are equal this shard emits `true` as its output, if the values are inequal it emits `false`. No other output is allowed.

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

### bytes

Type [`bytes`](https://docs.fragcolor.xyz/functions/values/#bytes) represents binary data.

!!! note
    Has a pascal case alias, `Bytes`.

A byte is made up of 8 bits (for example, `10111010`) and a `bytes` type is an array of such bytes: `[11110001 10110111 10000111]`

??? note "Bits and Bytes"
    Bits are how data is stored in a computer at the level of ectrical circuits. A bit can have only two values (1 or 0, representing the circuit is on or off) - hence the name binary data. A group of eight bits make a byte: `11111111`, `10101010`, etc. Since a bit can have only two values, a Byte can represent a total of 256 numbers (2^8): 0 to 255.

Shards like [`(ToBytes)`](https://docs.fragcolor.xyz/shards/General/ToBytes/),  [`(BytesToString)`](https://docs.fragcolor.xyz/shards/General/BytesToString/), [`(BytesToInts)`](https://docs.fragcolor.xyz/shards/General/BytesToInts/), etc. all use the type `bytes` either for their input or their output.

### color

Type [`color`](https://docs.fragcolor.xyz/functions/values/#color) represents an RGBA color format and is constructed from four unsigned 8 bit integers (one each for the R, G, B, and A values).

!!! note
    Has a pascal case alias, `Color`.

Each of the R, G, B, and A values range from 0 to 255. R, G, and B stand for red, blue, and green components of the color. A represents the *alpha channel* property (how opaqe a pixel is - 0 is fully transparent, 255 is fully opaque).

The shard [`(ToColor)`](https://docs.fragcolor.xyz/shards/General/ToColor/) converts its input into a `color`.

=== "Code"

    ```clojure linenums="1"
    (int4 255 10 10 257) (ToColor)
    (Log)    ;; if input > 255, 256 is subtracted from it => 255, 10, 10, 1

    [23 45 56 78] (ToColor)
    (Log)   ;; input in range 0-255 so => 23, 45, 56, 78

    "Hello" (ToColor)
    (Log)   ;; non-numeric input so => 0, 0, 0, 0
    ```

=== "Output"

    ```
    [info] [2022-07-26 19:08:24.520] [T-24408] [logging.cpp::55] [mywire] 255, 10, 10, 1
    [info] [2022-07-26 19:08:24.533] [T-24408] [logging.cpp::55] [mywire] 23, 45, 56, 78
    [info] [2022-07-26 19:08:24.534] [T-24408] [logging.cpp::55] [mywire] 0, 0, 0, 0
    ```

### context-var

Type [`context-var`](https://docs.fragcolor.xyz/functions/values/#context-var) represents a contextual variable (i.e., a variable that is in scope for the shard processing this data).

!!! note
    Has a pascal case alias, `ContextVar`.

The shard [`(Math.Inc)`](https://docs.fragcolor.xyz/shards/Math/Inc/) accepts only `context-var` type numeric data (i.e., a variable that holds numeric data) into its `:Value` parameter, and increments it by 1.

=== "Code"

    ```clojure linenums="1"
    11 >= .intvar                   ;; .intvar is of type `ContextVar`
    (Math.Inc .intvar)
    .intvar (Log)                   ;; => 12

    (Float2 4.5 5.7) >= .floatvar   ;; .floatvar is of type `ContextVar`
    (Math.Inc .floatvar)
    .floatvar (Log)                 ;; => (5.5, 6.7)
    ```

=== "Output"

    ```
    [info] [2022-07-26 19:30:22.837] [T-27800] [logging.cpp::55] [mywire] 12
    [info] [2022-07-26 19:30:22.843] [T-27800] [logging.cpp::55] [mywire] (5.5, 6.7)
    ```

### enum

[`enum`](https://docs.fragcolor.xyz/functions/values/#enum) stands for enumerated data type.

!!! note
    Has a pascal case alias, `Enum`.

The value that you pass to an enumerated variable can only take certain 'states' or named constant values.

For example, in [`(Math.Mean)`](https://docs.fragcolor.xyz/shards/Math/Mean/) the value for `:Kind` parameter needs to be of type `enum`.

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

### float

Type [`float`](https://docs.fragcolor.xyz/functions/values/#float) defines a 64-bit signed floating point number.

!!! note
    Has a pascal case alias, `Float`.

Floating point means it has the capability to store a decimal point and hence supports decimal numbers.

64 bits of memory allows this data type to support a very large range of positive and negative decimal numbers (16 significant decimal digits and an exponent range of −383 to +384).

A `float` value looks like this: `(float 2.53)`.
It may also be represented without the keyword `float`, with just the floating-point value: `2.53`.

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

### float2

Type [`float2`](https://docs.fragcolor.xyz/functions/values/#float2) defines a vector of two [`float`](#float) type numbers.

!!! note
    Has a pascal case alias, `Float2`.

A vector can be thought of as a group or list of items that are considered together for processing.

A `float2` type value looks like this: `(float2 3.4 -5.0)`.

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

### float3

Type [`float3`](https://docs.fragcolor.xyz/functions/values/#float3) defines a vector of three 32-bit signed floating point numbers.

!!! note
    Has a pascal case alias, `Float3`.

Floating point means it has the capability to store a decimal point and hence supports decimal numbers.

32 bits of memory allows this data type to support a large range of positive and negative decimal numbers (7 significant decimal digits and an exponent range of −101 to +90).

A `float3` type value looks like this: `(float3 2.9 -4.23 7.83)`.

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

### float4

Type [`float4`](https://docs.fragcolor.xyz/functions/values/#float4) is like type [`float3`](#float3) but a vector of four 32-bit signed floating point numbers instead.

!!! note
    Has a pascal case alias, `Float4`.

A `float4` type value looks like this: `(float4 -8.84 38.2 4.7 0.4)`.

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

### Image

Type **Image** is uncompressed image data.

!!! note
    No keyword or alias exists for this type.

A shard that uses this type is [`(StripAlpha)`](https://docs.fragcolor.xyz/shards/General/StripAlpha/). This takes an **Image** type input, strips out its alpha (transparency) channel, and outputs an **Image** type (transformed image).

??? note "Supported formats"
    Shards supports the image formats PNG and SVG.

### int

Type [`int`](https://docs.fragcolor.xyz/functions/values/#int) defines a 64-bit signed integer.

!!! note
    Has a pascal case alias, `Int`.

64 bits of memory allows this data type to store integer values ranging from -9,223,372,036,854,775,808 to +9,223,372,036,854,775,807 (no decimals).

An `int` value looks like this: `(int 2)`.
It may also be represented without the keyword `int`, with just the integer value: `2`.

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

### int2

Type [`int2`](https://docs.fragcolor.xyz/functions/values/#int2) defines a vector of two [`int`](#int) type numbers.

!!! note
    Has a pascal case alias, `Int2`.

A vector can be thought of as a group or list of items that are considered together for processing.

An `int2` type value looks like this: `(int2 3 -5)`.

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

### int3

Type [`int3`](https://docs.fragcolor.xyz/functions/values/#int3) defines a vector of three 32-bit signed integers.

!!! note
    Has a pascal case alias, `Int3`.

32 bits of memory for each number allows this data type to store integer values ranging from -2147483648 to +2147483647 (no decimals).

An `int3` type value looks like this: `(int3 2 4 -4)`.

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

### int4

Type [`int4`](https://docs.fragcolor.xyz/functions/values/#int4) is like type [`int3`](#int3) but a vector of four 32-bit signed integers instead.

!!! note
    Has a pascal case alias, `Int4`.

An `int4` type value looks like this: `(int4 1 -4 0 3)`.

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

Type **Int8** defines a vector of eight 16-bit signed integers.

!!! note
    No keyword or alias exists for this type.

16 bits of memory for each number allows this data type to store integer values ranging from −32,768 to +32,767 (no decimals).

The shard `(Math.Add)` accepts **Int8** as input and as its `:Operand`. The shard adds these up outputs the sum as another vector of 8 integers or **Int8** data type.

### Int16

Type **Int16** defines a vector of sixteen 8-bit signed integers.

!!! note
    No keyword or alias exists for this type.

8 bits of memory for each number allows this data type to store integer values ranging from −128 to +127 (no decimals).

The shard `(Math.Add)` accepts **Int16** as input and as its `:Operand`. The shard adds these up outputs the sum as another vector of 16 integers or **Int16** data type.

### None

Type **None** indicates that no data type is expected. This implies that no value is expected.

!!! note
    No keyword or alias exists for this type.

For example, **None** as one of the valid data types for `:Max` parameter in shard [`(RandomInt)`](https://docs.fragcolor.xyz/shards/General/RandomInt/) means that setting a value for this parameter is not mandatory.

```clojure linenums="1"
(RandomInt
:Max [(None) (Int) (ContextVar [(Int)])]
)
```

`(RandomInt)` generates a random integer and the `:Max` parameter is the upper limit (not inclusive) of the value that can be generated. So it makes sense to have **None** as one of the valid types for this `:Max` parameter for cases when you do not want an upper limit on the random integer.

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

### Object

Type **Object** is an *opaque* data type in Shards.

!!! note
    No keyword or alias exists for this type.

Opacit in a data type means that the structure of this kind of data is not defined in an interface and is visible only to shards that use this type. What this also implies is that the internal structure of this data type will vary from shard to shard.

For example, the `:Socket` parameter object of [`(WS.ReadString)`](https://docs.fragcolor.xyz/shards/WS/ReadString/) is different from the output object of [`(GFX.DrawQueue)`](https://docs.fragcolor.xyz/shards/GFX/DrawQueue/), even though both are of type **Object**.

### path

Type [`path`](https://docs.fragcolor.xyz/functions/values/#path) is string data that is expected to contain a valid path (your operating system or local machine) for loading resources like script files, images, audio files etc.

!!! note
    Has a pascal case alias, `Path`.

A valid `path` type data string would look like this: `"../../external/sample-models/Avocado.glb"`

!!! note
    For shards this type is the same as [`string`](#string) type as far as type validations are concerned (when you execute your script Shards first checks the types before running your code). However,if the path-string passed is invalid, malformed, or missing the resource to be loaded, the shard will complain with an error message at runtime (i.e., when your code actually runs).

A shard that uses this type is [`(Process.Run)`](https://docs.fragcolor.xyz/shards/Process/Run/). This shard takes a `path` (as well as a `string`) type in its `:Executable` parameter.

### Set

Type **Set** is a collection of unique values.

!!! note
    No keyword or alias exists for this type. [`(Set)`](https://docs.fragcolor.xyz/shards/General/Set/) is an unrelated shard.

It's different from other collections like [`seq`](#seq) and [**Array**](#array), both of which can contain non-unique or duplicate items.

An example of a **Set** type data would be `(22 3 378 4)`.

### seq

Type [`seq`](https://docs.fragcolor.xyz/functions/values/#seq) is a collection of values that can be accessed sequentially (i.e., they're iterable).

Also called a sequence. An example of `seq` type would be `[7 2 54 42]`.

The shard [`(Take)`](https://docs.fragcolor.xyz/shards/General/Take/) works on this type. This shard can access `seq` elements by their position.

=== "Code"

    ```clojure linenums="1"
    [7 2 54 42] (Take 2)
    (Log)   ;; print the 2nd element => 54
    ```

=== "Output"

    ```
    [info] [2022-07-26 22:24:48.918] [T-20928] [logging.cpp::55] [mywire] 54
    ```

### ShardRef

Type **ShardRef** (or type **Shard**) represents a shard being passed as data.

!!! note
    No keyword or alias exists for this type.

This type is an important aspect of the homoiconicity feature (i.e., code/data interchangeability) in Shards.

The shard [`(ForEach)`](https://docs.fragcolor.xyz/shards/General/ForEach/) expects type **ShardRef** for its `:Apply` parameter (the other option being a sequence of **ShardRef** type values, i.e., a [`Wire`](#wire) type).

`(ForEach)` then applies this shard (or sequence of shards) on its input to transform it into its output.

### string

Type [`string`](https://docs.fragcolor.xyz/functions/values/#string) represents string data.

!!! note
    Has a pascal case alias, `String`.

An example of a shard that processes `string` type data is [`(StringToBytes)`](https://docs.fragcolor.xyz/shards/General/StringToBytes/). It converts its `string` type input into a `bytes` type output.

=== "Code"

    ```clojure linenums="1"
    "Hello World" (StringToBytes)
    (Log)   ;; memory address of the string data => Bytes: 0x20440058720 size: 11
    ```

=== "Output"

    ```
    [info] [2022-07-26 19:38:14.813] [T-18168] [logging.cpp::55] [mywire] Bytes: 0x20440058720 size: 11
    ```

### Table

Type **Table** is a collection of key/value pairs.

!!! note
    No keyword or alias exists for this type. [`(Table)`](https://docs.fragcolor.xyz/shards/General/Table/) is a shard for creating a table; it does not denote the data type **Table**.

Also called a map, a data dictionary, or an associative array. An example of a **Table** type would be: `{:key1 "Hello" :key2 "World"}`.

=== "Code"

    ```clojure linenums="1"
    {:k1 123} >= .tabvar    ;; .tabvar is type `Table` now
    .tabvar (ExpectTable)
    (Log)                   ;; `ExpectTable` outputs `Table` type  =>   {k1: 123}
    ```

=== "Output"

    ```
    [info] [2022-07-26 22:46:17.194] [T-26104] [logging.cpp::55] [mywire] {k1: 123}
    ```

### Wire

Type [`Wire`](https://docs.fragcolor.xyz/functions/values/#wire) is a sequence of shards (i.e., a sequence of [**ShardRef**](#shardref) types).

In addition to the **Shard** type, [`(ForEach)`](https://docs.fragcolor.xyz/shards/General/ForEach/) also accepts a sequence of **Shard** type values, or in other words a `Wire` type, for its `:Apply` parameter.

--8<-- "includes/license.md"
