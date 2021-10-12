---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Values

!!! info
    All these functions have an alias starting with an uppercase character.
    
    In other words, `(int 42)` and `(Int 42)` are both valid, and represent the same value.

## bytes

Represents the raw bytes value of a string.

=== "Code"

    ```clojure linenums="1"
    (bytes "string")
    ```

## color

Represents a RGBA color where each component is within the \[0, 255\] range.

=== "Code"

    ```clojure linenums="1"
    (color 255 255 255 255)
    ```

## context-var

Creates a contextual variable with the given name.

??? note
    The uppercase alias for this function is `(ContextVar)`.

=== "Code"

    ```clojure linenums="1"
    (context-var "myVar")
    ```

## float

Represents a floating point number.

=== "Code"

    ```clojure linenums="1"
    (float 3.14)
    ```

## float2

Represents a vector of 2 floating point numbers.

=== "Code"

    ```clojure linenums="1"
    (float2 -1.0 1.0)
    ```

## float3

Represents a vector of 3 floating point numbers.

=== "Code"

    ```clojure linenums="1"
    (float3 -1.0 0.0 1.0)
    ```

## float4

Represents a vector of 4 floating point numbers.

=== "Code"

    ```clojure linenums="1"
    (float4 1.0 1.0 1.0 0.0)
    ```

## int

Represents a signed integer.

=== "Code"

    ```clojure linenums="1"
    (int 1)
    ```

## int2

Represents a vector of 2 signed integers.

=== "Code"

    ```clojure linenums="1"
    (int2 1 2)
    ```

## int3

Represents a vector of 3 signed integers.

=== "Code"

    ```clojure linenums="1"
    (int3 1 2 3)
    ```

## int4

Represents a vector of 4 signed integers.

=== "Code"

    ```clojure linenums="1"
    (int4 1 2 3 4)
    ```

## string

Represents a string.

=== "Code"

    ```clojure linenums="1"
    (string "Hello World!")
    ```


--8<-- "includes/license.md"
