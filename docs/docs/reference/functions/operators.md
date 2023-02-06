---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Operators

!!! danger
    These operators only works on numerical values. Comparing with a string or any other type will produce an error.

## =

Equality operator. Checks whether two values are equal.

=== "Code"

    ```clojure linenums="1"
    (= 4 5)

    (= "5" 5)

    (= 5 5)

    (= 5 5.0)
    ```

=== "Result"

    ```
    false

    Error: "5" is not a malNumber, line: 3

    true

    true
    ```

## <

Comparison operator. Checks whether the first value is lesser than the second value.

=== "Code"

    ```clojure linenums="1"
    (< 4 5)

    (< 5 4)

    (< 5 5)
    ```

=== "Result"

    ```
    true

    false

    false
    ```

## >

Comparison operator. Checks whether the first value is greater than the second value.

=== "Code"

    ```clojure linenums="1"
    (> 4 5)

    (> 5 4)

    (> 5 5)
    ```

=== "Result"

    ```
    false

    true

    false
    ```

## <=

Comparison operator. Checks whether the first value is lesser than or equal to the second value.

=== "Code"

    ```clojure linenums="1"
    (<= 4 5)

    (<= 5 4)

    (<= 5 5)
    ```

=== "Result"

    ```
    true

    false

    true
    ```

## >=

Comparison operator. Checks whether the first value is greater than or equal to the second value.

=== "Code"

    ```clojure linenums="1"
    (>= 4 5)

    (>= 5 4)

    (>= 5 5)
    ```

=== "Result"

    ```
    false

    true

    true
    ```


--8<-- "includes/license.md"
