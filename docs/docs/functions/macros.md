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

Such a chain executes only once when invoked (via `run <node>`).

=== "Code"

    ```clojure linenums="1"
    ;; define a node (main)
    (defnode main)
    ;; define a non-looped chain (chain-hello)
    (defchain chain-hello
        (Msg "Hello World!"))
    ;; define another non-looped chain (chain-bye)
    (defchain chain-bye
        (Msg "Goodbye World"))
    ;; schedule the non-looped chains on the node
    (schedule main chain-hello)
    (schedule main chain-bye)
    ;; run all the scheduled chains on the node
    (run main)
    ;; Even if provided, the FPS and iteration parameters are ignored for non-looped chains
    ;; (run main 0.02 5)
    ```

=== "Result"

    ```
    [info] [2022-03-07 20:45:35.681] [T-9044] [logging.cpp::94] [chain-hello] Hello World!
    [info] [2022-03-07 20:45:35.678] [T-9044] [logging.cpp::94] [chain-bye] Goodbye World
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
