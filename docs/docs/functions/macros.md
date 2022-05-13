---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Macros

## def

Defines an alias for a value.

This value may be data, the result of an expression, or the return value of a block.

=== "Code"

    ```clojure linenums="1"
    ;; define an alias for data
    (def sequence1 [2 4 6 8])
    (def string1 "I'm a string")

    ;; define an alias for an evaluated expression result
    (def xresult (* 2 4))   

    ;; define an alias for a block's return value
    (def my-chain (Chain "my-chain"))   
    (def my-looped-chain (Chain "my-looped-chain" :Looped))   
    (def Root (Node))
    (def - Math.Subtract)
    ```

??? info "See also"
    * [defchain](#defchain)
    * [defloop](#defloop)
    * [defnode](#defnode)

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

=== "Code"

    ```clojure linenums="1"
    ;; defchain
    (defchain my-chain
        ;; blocks here
    )
    ```

`(defchain <chain-name>)` is actually a shorthand for the more verbose non-looped chain definition: `def <chain-name>` + `Chain "chain-name"`.
=== "Code"

    ```clojure linenums="1"
    ;; def + Chain
    (def my-chain
    (Chain "my-chain"
        ;; blocks here
    ))
    ```

A chain is a stateful function.

To run it you must first schedule it on a node. When you run this node, all the chains scheduled on it are executed (in the order of scheduling).

A node will execute a non-looped chain only once (even though the node itself may continue running).

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

`defchain` can also parse and save the chain's input into a variable.

=== "Code"

    ```clojure linenums="1"
    (defnode main)
    (defchain mychain
        = .chainvar                    ;; save mychain input to chainvar
        .chainvar (Log "chain input")) ;; log mychain input to screen
    (defchain mainchain 
        "chainblocks" (Do mychain))    ;; invoke mychain with an input
    (schedule main mainchain)
    (run main)
    ```

=== "Result"

    ```
    [trace] [2022-05-12 21:25:49.714] [T-17336] [runtime.cpp::1998] chain mainchain starting
    [info] [2022-05-12 21:25:49.715] [T-17336] [logging.cpp::53] [mychain] chain input: chainblocks
    [debug] [2022-05-12 21:25:49.716] [T-17336] [runtime.cpp::2741] Running cleanup on chain: mainchain users count: 0
    ```
    

??? info "See also"
    * [def](#def)
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

=== "Code"

    ```clojure linenums="1"
    ;; defloop
    (defloop my-loop       ;; define a looped chain
        ;; blocks here
    ))
    ```

`(defloop <looped chain-name)` is actually a shorthand for the more verbose looped chain definition: `def <looped chain-name>` + `Chain "looped chain-name"`.
=== "Code"

    ```clojure linenums="1"
    ;; def + Chain
    (def my-loop
    (Chain "my-loop" :Looped
        ;; blocks here
    ))
    ```
    
For a chain to be executed, it must first be scheduled on a `node` and then that that node needs to run.

=== "Code"

    ```clojure linenums="1"
    (defnode main)          ;; define a node
    (defloop my-loop        ;; define a looped chain
        (Msg "Hello World!"))
    (schedule main my-loop) ;; 
    (run main)              ;; run chains scheduled on this node
    ```
=== "Result"

    ```
    [info] [2022-05-13 09:48:01.692] [T-15068] [logging.cpp::98] [my-loop] Hello World!
    [info] [2022-05-13 09:48:01.693] [T-15068] [logging.cpp::98] [my-loop] Hello World!
    [info] [2022-05-13 09:48:01.694] [T-15068] [logging.cpp::98] [my-loop] Hello World!
    .
    .
    .

    ```

A node will continue executing a looped chain till the node itself stops running (or the chain execution is stopped via a logic condition).

=== "Code"

    ```clojure linenums="1"
    (defnode main)             ;; define a node
    (defloop chain-hi          ;; define a looped chain (chain-hello)
        (Msg "Hello World!"))
    (defloop chain-bye         ;; define another looped chain (chain-bye)
        (Msg "Goodbye World"))
    (schedule main chain-hi)   ;; schedule the chain (chain-hi) on the node
    (schedule main chain-bye)  ;; schedule the chain (chain-bye) on the node
    (run main)                 ;; run all the chains scheduled on this node
    ```
=== "Result"

    ```
    [info] [2022-03-07 22:28:54.682] [T-8432] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:28:54.682] [T-8432] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 22:28:54.715] [T-8432] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:28:54.715] [T-8432] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 22:28:54.731] [T-8432] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:28:54.732] [T-8432] [logging.cpp::94] [chain-bye] Goodbye World
    .
    .
    .
    ```

??? info "See also"
    * [def](#def)
    * [defchain](#defchain)


--8<-- "includes/license.md"
