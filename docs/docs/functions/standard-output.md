---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Standard output

## println

Prints a variable number of values into a the standard output, separated by a whitespace.
Each collection type follows its own printing rule.

=== "Code"

    ```clojure linenums="1"
    (println "hello" "world")

    (println [1 2 3 4])
    (println (vector 1 2 3 4))

    (println (list 1 2 3 4))

    (println {"key" "value"})
    (println (hash-map "key" "value"))

    (println {:key1 (vector 1 2 3)} {:key2 (list 1 2 3 4)})
    ```

=== "Output"

    ```
    hello world

    [1 2 3 4]
    [1 2 3 4]

    (1 2 3 4)

    {"key" value}
    {"key" value}

    {:key1 [1 2 3]} {:key2 (1 2 3 4)}
    ```

??? info "See also"
    * [prn](#prn)
    * [pr-str](strings.md#pr-str)
    * [str](strings.md#str)

## prn

Prints a variable number of values into a the standard output, separated by a whitespace.
Each collection type follows its own printing rule.
In addition, strings are escaped.

=== "Code"

    ```clojure linenums="1"
    (prn "hello" "world")

    (prn [1 2 3 4])
    (prn (vector 1 2 3 4))

    (prn (list 1 2 3 4))

    (prn {"key" "value"})
    (prn (hash-map "key" "value"))

    (prn {:key1 (vector 1 2 3)} {:key2 (list 1 2 3 4)})
    ```

=== "Output"

    ```
    "hello" "world"

    [1 2 3 4]
    [1 2 3 4]

    (1 2 3 4)

    {"key" "value"}
    {"key" "value"}

    {:key1 [1 2 3]} {:key2 (1 2 3 4)}
    ```

??? info "See also"
    * [println](#println)
    * [pr-str](strings.md#pr-str)
    * [str](strings.md#str)


--8<-- "includes/license.md"
