---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Collections

## assoc

=== "Code"

    ```clojure linenums="1"
    (def _args
        (eval
        (quasiquote
        (assoc {} ~@*command-line-args*))))
    ```

## concat

=== "Code"

    ```clojure linenums="1"

    ```

## conj

=== "Code"

    ```clojure linenums="1"

    ```

## cons

Constructs a list recursively using the pattern `(cons element rest-of-list)`.

=== "Code"

    ```clojure linenums="1"
    (cons 1 (cons 2 (cons 3 nil)))
    ```

=== "Result"

    ```
    (1 2 3)
    ```

## contains?

Checks whether a hashmap contains a record with the given key.

=== "Code"

    ```clojure linenums="1"
    (if (contains? _args "--file")
        (get _args "--file")
        "")
    ```

## count

Counts the number of elements in a list, sequence or vector.

=== "Code"

    ```clojure linenums="1"
    (if (> (count parameters) 0)
        "at least one parameter"
        "no parameters")
    ```

## dissoc

=== "Code"

    ```clojure linenums="1"

    ```

## empty?

Checks whether a list, sequence or vector has no elements.

=== "Code"

    ```clojure linenums="1"
    (if (empty? parameters)
        "no parameters"
        "at least one parameter")
    ```

## get

Gets the value represented by the given key, or `nil` if not found.

=== "Code"

    ```clojure linenums="1"
    (if (contains? _args "--file")
        (get _args "--file")
        "")
    ```

## first

=== "Code"

    ```clojure linenums="1"

    ```

## keys

Gets a list of all the keys from a hashmap.

=== "Code"

    ```clojure linenums="1"

    ```

## list

=== "Code"

    ```clojure linenums="1"

    ```

## hash-map

=== "Code"

    ```clojure linenums="1"

    ```

## map

=== "Code"

    ```clojure linenums="1"

    ```

## nth

=== "Code"

    ```clojure linenums="1"

    ```

## rest

=== "Code"

    ```clojure linenums="1"

    ```

## reverse

Creates a new sequence with the elements in reverse order.

=== "Code"

    ```clojure linenums="1"

    ```

## seq

=== "Code"

    ```clojure linenums="1"

    ```

## vals

Gets a list of all the values from a hashmap.

=== "Code"

    ```clojure linenums="1"

    ```

## vector

=== "Code"

    ```clojure linenums="1"

    ```


--8<-- "includes/license.md"
