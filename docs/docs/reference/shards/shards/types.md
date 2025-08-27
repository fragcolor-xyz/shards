---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Types

This section documents all the valid data types that are accepted by various shards either as their input value or as a parameter value. These data types also apply to the output created by any shard.

Valid data types for every shard are listed under the `Type` column of their Parameters, Input, and Output sections (types are enclosed within parentheses and if multiple types apply then they are separated by a space).

??? note "Compound types"
    While this section lists the simple (or primitive) data types, you can combine these to create compound data types. For example, combining `Int`, `String`, and `Seq`, can give you a sequence of sequences `[ [ Any ] ]`, a sequence of integers and strings `[ Int String ]`), and so on.

??? note "Why types?"
    Types are helpful as they reduce errors in programming. They are also very useful in visual programming as type-matching can be used to reduce the dropdown options when prompting a user on what shard to use next (depending on which shard's input type matches with the current shard's output type).

## Any

Type **Any** indicates that all data types are allowed.

For example, **Any** as the allowed data type for input and `Value:` parameter of shard [`Is`](../General/Is/) means that `Is` accepts and compares across all data types.

```shards
[1]
Take(0)
Is(Value: 1)
```

`Is` compares the input and`Value:` parameter values and returns `true` only if both the value and data type of these entities is equal/same.

=== "Code"

    ```shards
    [4 5 6] | Is(Value: [4 5 6])
    Log   ;; value and type match => true

    "I'm a string" >= var1
    "I'm a string" >= var2
    var1 | Is(Value: var2)
    Log   ;; value and type match => true

    "I'm a string" >= var3
    "I'm a different string" >= var4
    var3 | Is(Value: var4)
    Log   ;; value mismatch => false

    Float(4.0) >= var5
    Int(4) >= var6
    var5 | Is(Value: var6)
    Log   ;; type mismatch => false
    ```

=== "Output"

    ```
    [mywire] true
    [mywire] true
    [mywire] false
    [mywire] false
    ```

## Sequence

Type **Sequence** is a collection of values that can be accessed directly via indexes (since items are indexed by contiguous integers).

It is also called a vector or an array. A **Sequence**'s items are accessible by index. Example of a **Sequence** type would be: `[43 6 1]`.

## Audio

Type **Audio** is uncompressed audio data.

Examples of shards that use this type are [`(Audio.Oscillator)`](../Audio/Oscillator/), [`(Audio.ReadFile)`](../Audio/ReadFile/), and [`(Audio.WriteFile)`](../Audio/WriteFile/) all of which generate **Audio** type data as their output.

??? note "Supported formats"
    Shards supports the audio formats WAV, MP3, OGG, and FLAC.

## Bool

Type **Bool** allows only two values - `true` or `false`.
In that sense, it can be thought of as a special case of an [**Enum**](#enum) data type.

Consider the shard [`(Is)`](../General/Is/). This shard compares its input and the value in the `Value:` parameter for equality and returns `true` if values are equal, otherwise `false` if values are not equal. Examples:

=== "Code"

    ```shards
    100 | Is(Value: (10 | Math.Multiply(10)))
    Log   ;; Is equal => true

    [20] | Is(Value: 20)
    Log   ;; Is not equal => false
    ```

=== "Output"

    ```
    [mywire] true
    [mywire] false
    ```

## Bytes

Type **Bytes** represents binary data.

A byte is made up of 8 bits (for example, `10111010`) and a **Bytes** type is an sequence of such bytes: `[11110001 10110111 10000111]`

??? note "Bits and Bytes"
    Bits are how data is stored in a computer at the level of electrical circuits. A bit can have only two values (1 or 0, representing the circuit is on or off) - hence the name binary data. A group of eight bits make a byte: `11111111`, `10101010`, etc. Since a bit can have only two values, a Byte can represent a total of 256 numbers (2^8): 0 to 255.

Shards like [`(ToBytes)`](../General/ToBytes/),  [`(BytesToString)`](../General/BytesToString/), [`(BytesToInts)`](../General/BytesToInts/), etc, all use the type **Bytes** either for their input or their output.

## Color

Type **Color** represents an RGBA color format and is constructed from four unsigned 8 bit integers (one each for the R, G, B, and A values).

Each of the R, G, B, and A values range from 0 to 255. R, G, and B stand for red, blue, and green components of the color. A represents the *alpha channel* property (how opaqe a pixel is - 0 is fully transparent, 255 is fully opaque).

The shard [`ToColor`](../General/ToColor/) converts its input into a **Color** type.

=== "Code"

    ```shards
    @i4(255 10 10 257) | ToColor
    Log    ;; if input > 255, 256 is subtracted from it => 255, 10, 10, 1

    [23 45 56 78] | ToColor
    Log   ;; input in range 0-255 so => 23, 45, 56, 78

    "Hello" ToColor
    Log   ;; non-numeric input so => 0, 0, 0, 0
    ```

=== "Output"

    ```
    [mywire] 255, 10, 10, 1
    [mywire] 23, 45, 56, 78
    [mywire] 0, 0, 0, 0
    ```

## ContextVar

Type **ContextVar** represents a contextual variable (i.e., a variable that is in scope for the shard processing this data).

The shard [`Math.Inc`](../Math/Inc/) accepts only **ContextVar** type numeric data (i.e., a variable that holds numeric data) into its `Value:` parameter, and increments it by 1.

=== "Code"

    ```shards
    11 >= intvar                   ;; intvar is of type `ContextVar`
    Math.Inc(intvar)
    intvar | Log                   ;; => 12

    @f24.5 5.7) >= floatvar   ;; floatvar is of type `ContextVar`
    Math.Inc(floatvar)
    floatvar | Log                 ;; => (5.5, 6.7)
    ```

=== "Output"

    ```
    [mywire] 12
    [mywire] (5.5, 6.7)
    ```

## Enum

**Enum** stands for enumerated data type.

The value that you pass to an enumerated variable can only take certain 'states' or named constant values.

For an overview of all enums and valid values check the [Enums](../../enums) page

For example, in [`Math.Mean`](../Math/Mean/) the value for `Kind:` parameter needs to be of type [**Mean**](../../enums/Mean/).

```shards
Math.Mean(Kind: Mean::Arithmetic)
```

`Math.Mean` computes the mean of a sequence of floating-point numbers. But there are three kinds of means - Arithmetic mean, Geometric mean, and Harmonic mean.

So the parameter `Kind:` is defined as an enum variable with these three fixed states :
` Kind: = {Mean::Arithmetic, Mean::Geometric, Mean::Harmonic}`

And hence `Kind:` expects a value that matches one of its possible states. In other words the value you pass in for `Kind:` needs to be an enumerated data type.

In simple terms it just means that you pass in one of the allowed named constant values. Anything else will fail validation.

=== "Code"

    ```shards
    [2.0 10.0]
    Math.Mean(Kind: Mean::Arithmetic)
    Log   ;; AM => 6

    [2.0 10.0]
    Math.Mean(Kind: Mean::Geometric)
    Log   ;; GM => 4.47214

    [2.0 10.0]
    Math.Mean(Kind: Mean::Harmonic)
    Log   ;; HM => 3.33333
    ```

=== "Output"

    ```
    [info] [2022-07-22 15:35:00.868] [T-15316] [logging.cpp::55] [mywire] 6
    [info] [2022-07-22 15:35:00.881] [T-15316] [logging.cpp::55] [mywire] 4.47214
    [info] [2022-07-22 15:35:00.882] [T-15316] [logging.cpp::55] [mywire] 3.33333
    ```

## Float

Type **Float** defines a 64-bit signed floating point number.

Floating point means it has the capability to store a decimal point and hence supports decimal numbers.

64 bits of memory allows this data type to support a very large range of positive and negative decimal numbers (16 significant decimal digits and an exponent range of −383 to +384).

A **Float** value looks like this: `Float(2.53)`.
It may also be represented without the keyword `float`, with just the floating-point value: `2.53`.

[`Math.Add`](../Math/Add/) is an example of a shard that uses this data type for its input, output, and `Operand:` parameter.

=== "Code"

    ```shards
    2.4 | Math.Add(1.43)
    Log   ;; float output => 3.83
    ```

=== "Output"

    ```
    [mywire] 3.83
    ```

## Float2

Type **Float2** defines a vector of two [**Float**](#float) type numbers.

A vector can be thought of as a group or list of items that are considered together for processing.

A **Float2** type value looks like this: `@f23.4 -5.0)`.

[`Math.Add`](../Math/Add/) is an example of a shard that uses this data type for its input, output, and `Operand:` parameter.

=== "Code"

    ```shards
    @f2(4.1 5.0) | Math.Add(@f2(6.3 9.2))
    Log   ;; float2 output => (10.4, 14.2)
    ```

=== "Output"

    ```
    [mywire] (10.4, 14.2)
    ```

## Float3

Type **Float3** defines a vector of three 32-bit signed floating point numbers.

Floating point means it has the capability to store a decimal point and hence supports decimal numbers.

32 bits of memory allows this data type to support a large range of positive and negative decimal numbers (7 significant decimal digits and an exponent range of −101 to +90).

A **Float3** type value looks like this: `@f3(2.9 -4.23 7.83)`.

[`Math.Add`](../Math/Add/) is an example of a shard that uses this data type for its input, output, and `Operand:` parameter.

=== "Code"

    ```shards
    @f3(1.2 3.4 5.6) | Math.Add(@f3(6.5 4.3 2.1))
    Log   ;; float3 output => (7.7, 7.7, 7.7)
    ```

=== "Output"

    ```
    [mywire] (7.7, 7.7, 7.7)
    ```

## Float4

Type **Float4** is like type [**Float3**](#float3) but is a vector of four 32-bit signed floating point numbers instead.

A **Float4** type value looks like this: `@f4(-8.84 38.2 4.7 0.4)`.

[`Math.Add`](../Math/Add/) is an example of a shard that uses this data type for its input, output, and `Operand:` parameter.

=== "Code"

    ```shards
    @f4(3.1 6.4 9.2 4.6)
    Math.Add(@f4(6.8 3.5 0.9 5.3))
    Log   ;; Int4 output => (9.9, 9.9, 9.9, 9.9)
    ```

=== "Output"

    ```
    [info] [2022-07-22 22:23:24.076] [T-25152] [logging.cpp::55] [mywire] (9.9, 9.9, 10.1, 9.9)
    ```

## Image

Type **Image** is uncompressed image data.

A shard that uses this type is [`StripAlpha`](../General/StripAlpha/). This takes an **Image** type input, strips out its alpha (transparency) channel, and outputs an **Image** type (transformed image).

??? note "Supported formats"
    Shards supports the image formats PNG and SVG.

## Int

Type **Int** defines a 64-bit signed integer.

64 bits of memory allows this data type to store integer values ranging from -9,223,372,036,854,775,808 to +9,223,372,036,854,775,807 (no decimals).

An **Int** value looks like this: `2`.

[`Math.Add`](../Math/Add/) is an example of a shard that uses this data type for its input, output, and `Operand:` parameter.

=== "Code"

    ```shards
    3 | Math.Add(2)
    Log   ;; int output => 5
    ```

=== "Output"

    ```
    [mywire] 5
    ```

## Int2

Type **Int2** defines a vector of two [**Int**](#int) type numbers.

A vector can be thought of as a group or list of items that are considered together for processing.

An **Int2** type value looks like this: `@i2(3 -5)`.

[`Math.Add`](../Math/Add/) is an example of a shard that uses this data type for its input, output, and `Operand:` parameter.

=== "Code"

    ```shards
    @i2(4 5) | Math.Add(@i2(6 9))
    Log   ;; int2 output => (10, 14)
    ```

=== "Output"

    ```
    [mywire] (10, 14)
    ```

## Int3

Type **Int3** defines a vector of three 32-bit signed integers.

32 bits of memory for each number allows this data type to store integer values ranging from -2147483648 to +2147483647 (no decimals).

An **Int3** type value looks like this: `@i3(2 4 -4)`.

[`Math.Add`](../Math/Add/) is an example of a shard that uses this data type for its input, output, and `Operand:` parameter.

=== "Code"

    ```shards
    @i3(1 10 99) | Math.Add(@i3(99 90 1))
    Log   ;; int3 output => (100, 100, 100)
    ```

=== "Output"

    ```
    [mywire] (100, 100, 100)
    ```

## Int4

Type **Int4** is like type [**Int3**](#int3) but is a vector of four 32-bit signed integers instead.

An **Int4** type value looks like this: `@i4(1 -4 0 3)`.

[`Math.Add`](../Math/Add/) is an example of a shard that uses this data type for its input, output, and `Operand:` parameter.

=== "Code"

    ```shards
    @i4(3 6 9 4)
    Math.Add(@i4(6 3 0 5))
    Log   ;; int4 output => (9, 9, 9, 9)
    ```

=== "Output"

    ```
    [mywire] (9, 9, 9, 9)
    ```

## Int8

Type **Int8** defines a vector of eight 16-bit signed integers.

16 bits of memory for each number allows this data type to store integer values ranging from −32,768 to +32,767 (no decimals).

The shard `Math.Add` accepts **Int8** as input and as its `Operand:`. The shard adds these up outputs the sum as another vector of 8 integers or **Int8** data type.

## Int16

Type **Int16** defines a vector of sixteen 8-bit signed integers.

8 bits of memory for each number allows this data type to store integer values ranging from −128 to +127 (no decimals).

The shard `Math.Add` accepts **Int16** as input and as its `Operand:`. The shard adds these up outputs the sum as another vector of 16 integers or **Int16** data type.

## None

Type **None** indicates that no data type is expected. This implies that no value is expected.

For example, **None** as one of the valid data types for `Max:` parameter in shard [`RandomInt`](../General/RandomInt/) means that setting a value for this parameter is not mandatory.

```shards
RandomInt(Max: [(None) (Int) (ContextVar [(Int)])])
```

`RandomInt` generates a random integer and the `Max:` parameter is the upper limit (not inclusive) of the value that can be generated. So it makes sense to have **None** as one of the valid types for this `Max:` parameter for cases when you do not want an upper limit on the random integer (though in this case the system will inherently set the upper limit to the maximum value a 64-bit signed integer can hold: 9,223,372,036,854,775,807).

=== "Code"

    ```shards
    RandomInt(Max: 8)
    Log   ;; max int that can be generated is 7

    RandomInt
    Log   ;; now upper limit subject to max capacity of a 64-bit signed integer
    ```

=== "Output"

    ```
    [mywire] 4
    [mywire] 311828859
    ```

## Object

Type **Object** is an *opaque* data type in Shards.

Opacity in a data type means that the structure of this kind of data is not defined in an interface and is visible only to shards that use this type. What this also implies is that the internal structure of this data type will vary from shard to shard.

For example, the `Socket:` parameter object of [`WS.ReadString`](../WS/ReadString/) is different from the output object of [`(GFX.DrawQueue)`](../GFX/DrawQueue/), even though both are of type **Object**.

## Path

Type **Path** is [**String**](#string) type data that is expected to contain a valid path (your operating system or local machine) for loading resources like script files, images, audio files etc.

A valid **Path** type data string would look like this: `"../../external/sample-models/Avocado.glb"`

!!! note
    For shards this type is the same as [**String**](#string) type as far as type validations are concerned (when you execute your script Shards first checks the types before running your code). However, if the path-string passed is invalid, malformed, or missing the resource to be loaded, the shard will complain with an error message at runtime (i.e., when your code actually runs).

A shard that uses this type is [`Process.Run`](../Process/Run/). This shard takes a **Path** type in its `Executable:` parameter.

## Set

Type **Set** is a collection of unique values.

It's different from other collections like [**Seq**](#seq) and [**Sequence**](#sequence), both of which can contain non-unique or duplicate items.

An example of a **Set** type data would be `(22 3 378 4)`.

## Seq

Type **Seq** is a collection of values that can be accessed sequentially (i.e., they're iterable).

!!! note
    Has keyword `seq`.

Also called a sequence. An example of **Seq** type would be `[7 2 54 42]`.

The shard [`Take`](../General/Take/) works on this type. This shard can access **Seq** elements by their position.

=== "Code"

    ```shards
    [7 2 54 42] | Take(2)
    Log   ;; print the 2nd element => 54
    ```

=== "Output"

    ```
    [mywire] 54
    ```

## Shard

The type **Shard** (also called **ShardRef**) represents a shard being passed as data.

This type is an important aspect of the homoiconicity feature (i.e., code/data interchangeability) in Shards.

!!! note
    What's a [`shard`](../../../../learn/shards/primer/what-is-shards/#the-shard)?

The shard [`ForEach`](../General/ForEach/) expects a value with the type **Shard** for its `Apply:` parameter (the other option being a sequence of **Shard** type values, i.e., a [`Wire`](#wire) type).

`ForEach` then applies this shard (or sequence of shards) on its input to transform it into its output.

## [ Shard ]

When more that one shard is accepted it is indicated as a sequence of shards, any description can be turned into a sequence of shards by wrapping it with `{}`, for example:

```shards
Once( Action:{
    Msg("Hello")
    Msg("World")
})
```

## String

Type **String** represents string data (any data enclosed within double quotes).

A **String** value looks like this: `"Hello @Tom!"`.

An example of a shard that processes **String** type data is [`String.ToUpper`](../String/ToUpper/). This shard takes a **String** and converts it into its upper case version.

=== "Code"

    ```shards
    "Bye Universe!" | String.ToUpper
    Log   ;; upper case version => BYE UNIVERSE!
    ```

=== "Output"

    ```
    [mywire] BYE UNIVERSE!
    ```

## Table

Type **Table** is a collection of key/value pairs.

Its also known as map, data dictionary, or associative array. An example of a **Table** type would be: `{key1: "Hello" key2: "World"}`.

=== "Code"

    ```shards
    {k1: 123} >= tabvar    ;; tabvar is type `Table` now
    tabvar | ExpectTable
    Log                   ;; `ExpectTable` outputs `Table` type  =>   {k1: 123}
    ```

=== "Output"

    ```
    [mywire] {k1: 123}
    ```

## Wire

Type **Wire** represents a wire being passed as data.

!!! note
    Has keyword `Wire`.

A **Wire** type thus consists of a sequence of shards (which make up the wire), their shared state (memory) context, name of the wire, and other properties that enable operations on the wire like scheduling it on a mesh, starting/stopping/pausing the wire, etc.

!!! note
    What's a [`wire`](../../../../learn/shards/primer/what-is-shards/#the-wire)?

For example, the shard [`Stop`](../General/Stop/) accepts **Wire** type data in its `:Wire` parameter and stops that wire's execution if its currently running.


--8<-- "includes/license.md"
