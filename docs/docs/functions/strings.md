---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Strings

## pr-str

Prints a variable number of values into a string, separated by a whitespace.
Each collection type follows its own printing rule.
In addition, strings are escaped.

=== "Code"

    ```clojure linenums="1"
    (pr-str "hello" "world")

    (pr-str [1 2 3 4])
    (pr-str (vector 1 2 3 4))

    (pr-str (list 1 2 3 4))

    (pr-str {"key" "value"})
    (pr-str (hash-map "key" "value"))

    (pr-str {:key1 (vector 1 2 3)} {:key2 (list 1 2 3 4)})
    ```

=== "Result"

    ```
    ""hello" "world""

    "[1 2 3 4]"
    "[1 2 3 4]"

    "(1 2 3 4)"

    "{"key" "value"}"
    "{"key" "value"}"

    "{:key1 [1 2 3]} {:key2 (1 2 3 4)}"
    ```

??? info "See also"
    * [println](standard-output.md#println)
    * [prn](standard-output.md#prn)
    * [str](#str)

## slurp

Reads the content of a file into a string. The path can be absolute or relative to the script.

=== "Code"

    ```clojure
    (slurp "file.txt")
    ```

## str

Prints a variable number of values into a string, without whitespace separation.
Each collection type follows its own printing rule.

=== "Code"

    ```clojure linenums="1"
    (str "hello" "world")

    (str [1 2 3 4])
    (str (vector 1 2 3 4))

    (str (list 1 2 3 4))

    (str {"key" "value"})
    (str (hash-map "key" "value"))

    (str {:key1 (vector 1 2 3)} {:key2 (list 1 2 3 4)})
    ```

=== "Result"

    ```
    "helloworld"

    "[1 2 3 4]"
    "[1 2 3 4]"

    "(1 2 3 4)"

    "{"key" value}"
    "{"key" value}"

    "{:key1 [1 2 3]}{:key2 (1 2 3 4)}"
    ```

??? info "See also"
    * [println](standard-output.md#println)
    * [pr-str](#pr-str)
    * [prn](standard-output.md#prn)


--8<-- "includes/license.md"
