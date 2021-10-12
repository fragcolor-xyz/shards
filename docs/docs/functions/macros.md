---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Macros

## def

Defines an alias for a value.

=== "Code"

    ```clojure linenums="1"

    ```

## def!

Defines an alias.

=== "Code"

    ```clojure linenums="1"

    ```

## defblocks

Defines new blocks that can be inserted into an existing chain.

=== "Code"

    ```clojure linenums="1"

    ```

## defchain

Defines a new chain that isn't looped.

=== "Code"

    ```clojure linenums="1"

    ```

??? info "See also"
    * [defloop](#defloop)

## defn

Defines a new function.

=== "Code"

    ```clojure linenums="1"

    ```

## defmacro!

Defines a new macro.

=== "Code"

    ```clojure linenums="1"

    ```

## defnode

Defines a new node on which chains can be scheduled.

=== "Code"

    ```clojure linenums="1"
    (defnode main)
    ```

## defloop

Defines a new chain that is looped.

=== "Code"

    ```clojure linenums="1"

    ```

??? info "See also"
    * [defchain](#defchain)


--8<-- "includes/license.md"
