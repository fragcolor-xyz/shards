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

Defines a new non-looped chain.

`(defchain my-chain)` is actually a shorthand for the more verbose non-looped chain definition.
=== "Code"

    ```clojure linenums="1"
    ;; defchain
    (def my-chain
    (Chain "my-chain"
        ;; blocks here
    ))
    ```

For a chain to be executed, it must first be scheduled on a `node` and then that that node needs to run.

A node will execute a non-looped chain only once (even though node itself may continue running).

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

Defines a new `node` on which chains can be scheduled and then run.

A node is a self-contained execution-context and software environment (like a server) that executes the chain code logic. It can run both on the local hardware as well as peer-to-peer hardware over the network (blockchain). 

=== "Code"

    ```clojure linenums="1"
    ;; define a node (main)
    (defnode main)
    ;; define a looped chain
    (defloop chain-hi
        (Msg "Hello World!"))
    ;; define a non-looped chain
    (defchain chain-bye
        (Msg "Goodbye World"))
    ;; schedule both the chains on this node
    (schedule main chain-hi)
    (schedule main chain-bye)
    ;; run all the scheduled chains on this node
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-03-07 22:14:51.730] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:14:51.731] [T-14836] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 22:14:51.760] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:14:51.776] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:14:51.791] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:14:51.823] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    ```

## defloop

Defines a new looped chain.

`(defloop my-loop)` is actually a shorthand for the more verbose looped chain definition.
=== "Code"

    ```clojure linenums="1"
    ;; defloop
    (def my-loop
    (Chain "my-loop" :Looped
        ;; blocks here
    ))
    ```
    
For a chain to be executed, it must first be scheduled on a `node` and then that that node needs to run.

A node will continue executing a looped chain till the node itself stops running (or the chain execution is stopped via a logic condition).

=== "Code"

    ```clojure linenums="1"
    (defnode main)
    ;; define a looped chain (chain-hello)
    (defloop chain-hi
        (Msg "Hello World!"))
    ;; define another looped chain (chain-bye)
    (defloop chain-bye
        (Msg "Goodbye World"))
    ;; schedule the looped chains on this node
    (schedule main chain-hi)
    (schedule main chain-bye)
    ;; run all the chains on this node
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-03-07 22:28:54.682] [T-8432] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:28:54.682] [T-8432] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 22:28:54.715] [T-8432] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:28:54.715] [T-8432] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 22:28:54.731] [T-8432] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:28:54.732] [T-8432] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 22:28:54.746] [T-8432] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:28:54.747] [T-8432] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 22:28:54.763] [T-8432] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:28:54.763] [T-8432] [logging.cpp::94] [chain-bye] Goodbye World
    ```

??? info "See also"
    * [defchain](#defchain)


--8<-- "includes/license.md"
