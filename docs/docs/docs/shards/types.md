---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Types

This section documents all the valid data types that are accepted by various shards either as their input value or as their parameter values. These data types also apply to the output created by any shard.

Valid data types for every shard are listed under the `Type` column of their Parameters, Input, and Output sections (types are enclosed within parentheses and if multiple types apply then they are separated by a space).

??? note "Compound types"
    While this section lists the simple (or primitive) data types, you can combine these to create compound data types. For example, combining `Int`, `String`, and `Seq`, can give you a sequence of sequences (`(Seq [(Seq)])`), a sequence of integers and strings (`(Seq [(Int)] [(String)])`), and so on.

??? note "Why types?"
    Types are helpful as they reduce errors in programming. They are also very useful in visual programming as type-matching can be used to reduce the dropdown options when prompting a user on what shard to use next (depending on which shard's input type matches with the current shard's output type).

!!! note
    While all the following types are available internally to various shards, only a few are currently accessible in the Shards scripting environment. Consequently, only these types have keywords/aliases.

## Any

Type **Any** indicates that all valid data types are allowed.

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

## Array

Type **Array** is a collection of values that can be accessed directly via indexes (since items are indexed by contiguous integers).

Also called a vector. Looks exactly like the [**Seq**](#seq) type, but an **Array** type's items are accessible by index (and hence accessible randomly). Example of an **Array** type would be: `[43 6 1]`.

## Audio

Type **Audio** is uncompressed audio data.

Examples of shards that use this type are [`(Audio.Oscillator)`](https://docs.fragcolor.xyz/shards/Audio/Oscillator/), [`(Audio.ReadFile)`](https://docs.fragcolor.xyz/shards/Audio/ReadFile/), and [`(Audio.WriteFile)`](https://docs.fragcolor.xyz/shards/Audio/WriteFile/) all of which generate **Audio** type data as their output.

??? note "Supported formats"
    Shards supports the audio formats WAV, MP3, OGG, and FLAC.

## Bool

Type **Bool** allows only two values - `true` or `false`.
In that sense it can be thought of as a special case of an [**Enum**](#enum) data type.

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

## Bytes

Type **Bytes** represents binary data.

!!! note
    Has keyword [`bytes`](https://docs.fragcolor.xyz/functions/values/#bytes) and alias `Bytes`.

A byte is made up of 8 bits (for example, `10111010`) and a **Bytes** type is an array of such bytes: `[11110001 10110111 10000111]`

??? note "Bits and Bytes"
    Bits are how data is stored in a computer at the level of ectrical circuits. A bit can have only two values (1 or 0, representing the circuit is on or off) - hence the name binary data. A group of eight bits make a byte: `11111111`, `10101010`, etc. Since a bit can have only two values, a Byte can represent a total of 256 numbers (2^8): 0 to 255.

Shards like [`(ToBytes)`](https://docs.fragcolor.xyz/shards/General/ToBytes/),  [`(BytesToString)`](https://docs.fragcolor.xyz/shards/General/BytesToString/), [`(BytesToInts)`](https://docs.fragcolor.xyz/shards/General/BytesToInts/), etc. all use the type **Bytes** either for their input or their output.

## Color

Type **Color** represents an RGBA color format and is constructed from four unsigned 8 bit integers (one each for the R, G, B, and A values).

!!! note
    Has keyword [`color`](https://docs.fragcolor.xyz/functions/values/#color) and alias `Color`.

Each of the R, G, B, and A values range from 0 to 255. R, G, and B stand for red, blue, and green components of the color. A represents the *alpha channel* property (how opaqe a pixel is - 0 is fully transparent, 255 is fully opaque). 

The shard [`(ToColor)`](https://docs.fragcolor.xyz/shards/General/ToColor/) converts its input into a **Color** type.

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

## ContextVar

Type **ContextVar** represents a contextual variable (i.e., a variable that is in scope for the shard processing this data).

!!! note
    Has keyword [`context-var`](https://docs.fragcolor.xyz/functions/values/#context-var) and alias `ContextVar`.

The shard [`(Math.Inc)`](https://docs.fragcolor.xyz/shards/Math/Inc/) accepts only **ContextVar** type numeric data (i.e., a variable that holds numeric data) into its `:Value` parameter, and increments it by 1.

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

## Enum

**Enum** stands for enumerated data type. 

!!! note
    Has keyword [`enum`](https://docs.fragcolor.xyz/functions/values/#enum) and alias `Enum`.

The value that you pass to an enumerated variable can only take certain 'states' or named constant values.

For example, in [`(Math.Mean)`](https://docs.fragcolor.xyz/shards/Math/Mean/) the value for `:Kind` parameter needs to be of type **Enum**.

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

## Float

Type **Float** defines a 64-bit signed floating point number.

!!! note
    Has keyword [`float`](https://docs.fragcolor.xyz/functions/values/#float) and alias `Float`.

Floating point means it has the capability to store a decimal point and hence supports decimal numbers.

64 bits of memory allows this data type to support a very large range of positive and negative decimal numbers (16 significant decimal digits and an exponent range of −383 to +384).

A **Float** value looks like this: `(float 2.53)`.
It may also be represented without the keyword `float`, with just the floating-point value: `2.53`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (float 2.4) (Math.Add (float 1.43))
    (Log)   ;; float output => 3.83

    2.4 (Math.Add 1.43)
    (Log)   ;; float output => 3.83
    ```

=== "Output"

    ```
    [info] [2022-07-22 22:06:32.856] [T-20204] [logging.cpp::55] [mywire] 3.83
    [info] [2022-07-22 22:06:32.873] [T-20204] [logging.cpp::55] [mywire] 3.83
    ```

## Float2

Type **Float2** defines a vector of two [**Float**](#float) type numbers.

!!! note
    Has keyword [`float2`](https://docs.fragcolor.xyz/functions/values/#float2) and alias `Float2`.

A vector can be thought of as a group or list of items that are considered together for processing.

A **Float2** type value looks like this: `(float2 3.4 -5.0)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (float2 4.1 5.0) (Math.Add (float2 6.3 9.2))
    (Log)   ;; float2 output => (10.4, 14.2)
    ```

=== "Output"

    ```
[info] [2022-07-22 22:10:00.688] [T-24616] [logging.cpp::55] [mywire] (10.4, 14.2)
    ```

## Float3

Type **Float3** defines a vector of three 32-bit signed floating point numbers.

!!! note
    Has keyword [`float3`](https://docs.fragcolor.xyz/functions/values/#float3) and alias `Float3`.

Floating point means it has the capability to store a decimal point and hence supports decimal numbers.

32 bits of memory allows this data type to support a large range of positive and negative decimal numbers (7 significant decimal digits and an exponent range of −101 to +90).

A **Float3** type value looks like this: `(float3 2.9 -4.23 7.83)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (float3 1.2 3.4 5.6) (Math.Add (float3 6.5 4.3 2.1))
    (Log)   ;; float3 output => (7.7, 7.7, 7.7)
    ```

=== "Output"

    ```
    [info] [2022-07-22 22:19:36.923] [T-16128] [logging.cpp::55] [mywire] (7.7, 7.7, 7.7) 
    ```

## Float4

Type **Float4** is like type [**Float3**](#float3) but is a vector of four 32-bit signed floating point numbers instead.

!!! note
    Has keyword [`float4`](https://docs.fragcolor.xyz/functions/values/#float4) and alias `Float4`.

A **Float4** type value looks like this: `(float4 -8.84 38.2 4.7 0.4)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (float4 3.1 6.4 9.2 4.6)
    (Math.Add (float4 6.8 3.5 0.9 5.3))
    (Log)   ;; Int4 output => (9.9, 9.9, 9.9, 9.9)
    ```

=== "Output"

    ```
    [info] [2022-07-22 22:23:24.076] [T-25152] [logging.cpp::55] [mywire] (9.9, 9.9, 10.1, 9.9)
    ```

## Image

Type **Image** is uncompressed image data.

A shard that uses this type is [`(StripAlpha)`](https://docs.fragcolor.xyz/shards/General/StripAlpha/). This takes an **Image** type input, strips out its alpha (transparency) channel, and outputs an **Image** type (transformed image).

??? note "Supported formats"
    Shards supports the image formats PNG and SVG.

## Int

Type **Int** defines a 64-bit signed integer.

!!! note
    Has keyword [`int`](https://docs.fragcolor.xyz/functions/values/#int) and alias `Int`.

64 bits of memory allows this data type to store integer values ranging from -9,223,372,036,854,775,808 to +9,223,372,036,854,775,807 (no decimals).

An **Int** value looks like this: `(int 2)`.
It may also be represented without the keyword `int`, with just the integer value: `2`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (int 3) (Math.Add (int 2))
    (Log)   ;; int output => 5

    3 (Math.Add 2)
    (Log)   ;; int output => 5
    ```

=== "Output"

    ```
    [info] [2022-07-22 21:20:18.771] [T-4568] [logging.cpp::55] [mywire] 5
    [info] [2022-07-22 21:20:18.782] [T-4568] [logging.cpp::55] [mywire] 5
    ```

## Int2

Type **Int2** defines a vector of two [**Int**](#int) type numbers.

!!! note
    Has keyword [`int2`](https://docs.fragcolor.xyz/functions/values/#int2) and alias `Int2`.

A vector can be thought of as a group or list of items that are considered together for processing.

An **Int2** type value looks like this: `(int2 3 -5)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (int2 4 5) (Math.Add (int2 6 9))
    (Log)   ;; int2 output => (10, 14)
    ```

=== "Output"

    ```
    [info] [2022-07-22 21:22:26.381] [T-17748] [logging.cpp::55] [mywire] (10, 14)
    ```

## Int3

Type **Int3** defines a vector of three 32-bit signed integers.

!!! note
    Has keyword [`int3`](https://docs.fragcolor.xyz/functions/values/#int3) and alias `Int3`.

32 bits of memory for each number allows this data type to store integer values ranging from -2147483648 to +2147483647 (no decimals).

An **Int3** type value looks like this: `(int3 2 4 -4)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (int3 1 10 99) (Math.Add (int3 99 90 1))
    (Log)   ;; int3 output => (100, 100, 100)
    ```

=== "Output"

    ```
    [info] [2022-07-22 21:24:38.132] [T-25580] [logging.cpp::55] [mywire] (100, 100, 100)
    ```

## Int4

Type **Int4** is like type [**Int3**](#int3) but is a vector of four 32-bit signed integers instead.

!!! note
    Has keyword [`int4`](https://docs.fragcolor.xyz/functions/values/#int4) and alias `Int4`.

An **Int4** type value looks like this: `(int4 1 -4 0 3)`.

[`(Math.Add)`](https://docs.fragcolor.xyz/shards/Math/Add/) is an example of a shard that uses this data type for its input, output, and `:Operand` parameter.

=== "Code"

    ```clojure linenums="1"
    (int4 3 6 9 4)
    (Math.Add (int4 6 3 0 5))
    (Log)   ;; int4 output => (9, 9, 9, 9)
    ```

=== "Output"

    ```
    [info] [2022-07-22 21:27:58.115] [T-20884] [logging.cpp::55] [mywire] (9, 9, 9, 9)
    ```

## Int8

Type **Int8** defines a vector of eight 16-bit signed integers.

16 bits of memory for each number allows this data type to store integer values ranging from −32,768 to +32,767 (no decimals).

The shard `(Math.Add)` accepts **Int8** as input and as its `:Operand`. The shard adds these up outputs the sum as another vector of 8 integers or **Int8** data type. 

## Int16

Type **Int16** defines a vector of sixteen 8-bit signed integers.

8 bits of memory for each number allows this data type to store integer values ranging from −128 to +127 (no decimals).

The shard `(Math.Add)` accepts **Int16** as input and as its `:Operand`. The shard adds these up outputs the sum as another vector of 16 integers or **Int16** data type. 

## None

Type **None** indicates that no data type is expected. This implies that no value is expected.

For example, **None** as one of the valid data types for `:Max` parameter in shard [`(RandomInt)`](https://docs.fragcolor.xyz/shards/General/RandomInt/) means that setting a value for this parameter is not mandatory.

```clojure linenums="1"
(RandomInt
:Max [(None) (Int) (ContextVar [(Int)])]
)
```

`(RandomInt)` generates a random integer and the `:Max` parameter is the upper limit (not inclusive) of the value that can be generated. So it makes sense to have **None** as one of the valid types for this `:Max` parameter for cases when you do not want an upper limit on the random integer (though in this case the system will inherently set the upper limit to the maximum value a 64-bit signed integer can hold: 9,223,372,036,854,775,807).

=== "Code"

    ```clojure linenums="1"
    (RandomInt 8)
    (Log)   ;; max int that can be generated is 7
    
    (RandomInt)
    (Log)   ;; now upper limit subject to max capacity of a 64-bit signed integer
    ```

=== "Output"

    ```
    [info] [2022-07-22 13:45:03.282] [T-19992] [logging.cpp::55] [mywire] 4
    [info] [2022-07-22 13:45:03.293] [T-19992] [logging.cpp::55] [mywire] 311828859
    ```

## Object

Type **Object** is an *opaque* data type in Shards.

Opacity in a data type means that the structure of this kind of data is not defined in an interface and is visible only to shards that use this type. What this also implies is that the internal structure of this data type will vary from shard to shard.

For example, the `:Socket` parameter object of [`(WS.ReadString)`](https://docs.fragcolor.xyz/shards/WS/ReadString/) is different from the output object of [`(GFX.DrawQueue)`](https://docs.fragcolor.xyz/shards/GFX/DrawQueue/), even though both are of type **Object**.

## Path

Type **Path** is [**String**](#string) type data that is expected to contain a valid path (your operating system or local machine) for loading resources like script files, images, audio files etc.  

!!! note
    Has keyword [`path`](https://docs.fragcolor.xyz/functions/values/#path) and alias `Path`.

A valid **Path** type data string would look like this: `"../../external/sample-models/Avocado.glb"`

!!! note
    For shards this type is the same as [**String**](#string) type as far as type validations are concerned (when you execute your script Shards first checks the types before running your code). However,if the path-string passed is invalid, malformed, or missing the resource to be loaded, the shard will complain with an error message at runtime (i.e., when your code actually runs).

A shard that uses this type is [`(Process.Run)`](https://docs.fragcolor.xyz/shards/Process/Run/). This shard takes a **Path** type in its `:Executable` parameter.

## Set

Type **Set** is a collection of unique values.

It's different from other collections like [**Seq**](#seq) and [**Array**](#array), both of which can contain non-unique or duplicate items.

An example of a **Set** type data would be `(22 3 378 4)`.

## Seq

Type **Seq** is a collection of values that can be accessed sequentially (i.e., they're iterable).

!!! note
    Has keyword [`seq`](https://docs.fragcolor.xyz/functions/values/#seq).

Also called a sequence. An example of **Seq** type would be `[7 2 54 42]`.

The shard [`(Take)`](https://docs.fragcolor.xyz/shards/General/Take/) works on this type. This shard can access **Seq** elements by their position.

=== "Code"

    ```clojure linenums="1"
    [7 2 54 42] (Take 2)
    (Log)   ;; print the 2nd element => 54
    ```

=== "Output"

    ```
    [info] [2022-07-26 22:24:48.918] [T-20928] [logging.cpp::55] [mywire] 54
    ```

## ShardRef

Type **ShardRef** (also called type **Shard**) represents a shard being passed as data.

This type is an important aspect of the homoiconicity feature (i.e., code/data interchangeability) in Shards. 

!!! note
    What's a [`shard`](https://learn.fragcolor.xyz/primer/#shard)?

The shard [`(ForEach)`](https://docs.fragcolor.xyz/shards/General/ForEach/) expects type **ShardRef** for its `:Apply` parameter (the other option being a sequence of **ShardRef** type values, i.e., a [`Wire`](#wire) type).

`(ForEach)` then applies this shard (or sequence of shards) on its input to transform it into its output.

## String

Type **String** represents string data (any data enclosed within double quotes).

!!! note
    Has keyword [`string`](https://docs.fragcolor.xyz/functions/values/#string) and alias `String`.

A **String** value looks like this: `(string "Hello @Tom!")`. It may also be represented without the keyword `string`, with just the data within double quotes: `"Hello @Tom!"`.

An example of a shard that processes **String** type data is [`(String.ToUpper)`](https://docs.fragcolor.xyz/shards/String/ToUpper/). This shard takes a **String** and converts it into its upper case version.

=== "Code"

    ```clojure linenums="1"
    (string "Hello World!") (String.ToUpper)  
    (Log)   ;; upper case version => HELLO WORLD!

    "Bye Universe!" (String.ToUpper)  
    (Log)   ;; upper case version => BYE UNIVERSE!
    ```

=== "Output"

    ```
    [info] [2022-07-26 19:38:14.813] [T-18168] [logging.cpp::55] [mywire] Bytes: 0x20440058720 size: 11
    ```

## Table

Type **Table** is a collection of key/value pairs.

Its also known as map, data dictionary, or associative array. An example of a **Table** type would be: `{:key1 "Hello" :key2 "World"}`.

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

## Wire

Type **Wire** represents a wire being passed as data. 

!!! note
    Has keyword [`Wire`](https://docs.fragcolor.xyz/functions/values/#wire).

A **Wire** type thus consists of a sequence of shards (which make up the wire), their shared state (memory) context, name of the wire, and other properties that enable operations on the wire like scheduling it on a mesh, starting/stopping/pausing the wire, etc.

!!! note
    What's a [`wire`](https://learn.fragcolor.xyz/primer/#wire)?

For example, the shard [`(Stop)`](https://docs.fragcolor.xyz/shards/General/Stop/) accepts **Wire** type data in its `:Wire` parameter and stops that wire's execution if its currently running.


--8<-- "includes/license.md"
