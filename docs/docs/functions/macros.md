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

Defines new blocks that can be grouped together and inserted into an existing chain program.

A `defblocks` block looks structurally similar to a function (see [defn](#defn)), but can directly contain multiple blocks in its body (unlike a function).

=== "Code"

    ```clojure linenums="1"
    (defblocks MsgParms [input]
        (Msg "My name is")
        (Msg input))  
    ```

Just like a function a `defblocks` block can be invoked by name and can process input parameters.

=== "Code"

    ```clojure linenums="1"
    (defnode main)   
    (defblocks MsgParms [input]
        (Msg "My name is")
        (Msg input))     
    (defchain mychain
        (MsgParms "Chainblocks"))
    (schedule main mychain)
    (run main)
    ```

=== "Result"

    ```
    [info] [2022-05-13 15:31:09.231] [T-4796] [logging.cpp::98] [mychain] My name is
    [info] [2022-05-13 15:31:09.232] [T-4796] [logging.cpp::98] [mychain] Chainblocks
    ```

Since `defblocks` serves a very common use case an alias, `->`, is also provided. This alias is equivalent to `defblocks` but is much more succint to use.

See the last two code examples in [defn](#defn) for a use case of `defblocks` and its alias `->`.

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

A chain is a stateful function (read more about this [here](https://docs.fragcolor.xyz/architecture-guides/chainblocks#stateful-functions) and [here](https://learn.fragcolor.xyz/how-to/chainblocks-primer#stateful-functions)).

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
    (defn fn-name [fn-params] ;; function-name followed by input parameters in '[]'
        (Msg fn-params)       ;; function body that returns a value
    )
    ```

The function definition consists of the function name followed by its input parameters (`fn-params`), in `[]`.

If there are no input parameters the `[]` remains empty. Multiple input parameters may be passed as a sequence.

The processing statements (value/expression/blocks) following the `[]` is the function's body and its evaluation is the function's return value. A function may return a single or none at all.

A function can be invoked by calling it by name and passing its required parameters.

Function with no input parameters:

=== "Code"

    ```clojure linenums="1"
    (defnode main)    
    (defn func []
        (Msg "I got no parameters"))    ;; prints string text to screen
    (defchain mychain
        (func))                         ;; function invoked without any parameters
    (schedule main mychain)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:04:22.268] [T-1204] [logging.cpp::98] [mychain] I got no parameters 
    ```

Function with one input parameter:

=== "Code"

    ```clojure linenums="1"
    (defnode main)    
    (defn func [param]
        (Msg param))                     ;; prints the parameter to screen
    (defchain mychain
        (func "The only parameter"))     ;; function invoked with a single parameter
    (schedule main mychain)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:03:58.125] [T-21336] [logging.cpp::98] [mychain] The only parameter    
    ```

Function with multiple input parameters:

=== "Code"

    ```clojure linenums="1"
    (defnode main)    
    (defn func [param1 param2]
        (Msg param2))                           ;; prints the 2nd parameter to screen
    (defchain mychain
        (func "1st parameter" "2nd parameter")) ;; function invoked with multiple parameters
    (schedule main mychain)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:03:38.570] [T-11468] [logging.cpp::98] [mychain] 2nd parameter
    ```

The function cannot return multiple values. So if you need to process multiple blocks in the function's body you'll have to group them by wrapping them with either `defblocks`

=== "Code"

    ```clojure linenums="1"
    (defnode main)   
    (defblocks MsgParams [input]    ;; defblocks groups multiple blocks for processing
        (Msg "name is:")
        (Msg input))     
    (defn letslog [name]
        (MsgParams name))           ;; defblocks takes function input and returns single value
    (defchain mychain
        (letslog "chainblocks"))
    (schedule main mychain)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:44:51.293] [T-16928] [logging.cpp::98] [mychain] name is:
    [info] [2022-05-13 14:44:51.294] [T-16928] [logging.cpp::98] [mychain] chainblocks
    ```

or with its alias, `->` (which is more succint than `defblocks`).

=== "Code"

    ```clojure linenums="1"
    (defnode main)        
    (defn letslog [name]
        (->                             ;; defblocks replaced with `->`
            (Msg "name is:")            ;; multiple blocks can now be written down sequentially
            (Msg name)
        )
    )
    (defchain mychain
        (letslog "chainblocks"))
    (schedule main mychain)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:47:44.543] [T-21048] [logging.cpp::98] [mychain] name is:
    [info] [2022-05-13 14:47:44.544] [T-21048] [logging.cpp::98] [mychain] chainblocks
    ```

??? info "See also"
    * [defblocks](#defblocks)

## defmacro!

Defines a new macro.

=== "Code"

    ```clojure linenums="1"

    ```

## defnode

Defines a new `node` on which chains can be scheduled and then run.

=== "Code"

    ```clojure linenums="1"
    ;; defnode
    (defnode main)     ;; define a node named 'main'
    ```

`(defnode <node-name>)` is actually a shorthand for the more verbose node definition: `def <node-name> (Node)`.
=== "Code"

    ```clojure linenums="1"
    ;; def + Node
    (def main (Node))   ;; define a node named 'main'
    ```

A node is a self-contained execution-context and software environment (like a server) that executes the chain code logic. It can run both on the local hardware as well as peer-to-peer hardware over the network (blockchain). 

=== "Code"

    ```clojure linenums="1"
    (defnode main)             ;; define a node (main)
    (defloop chain-hi          ;; define a looped chain
        (Msg "Hello World!"))  
    (defchain chain-bye        ;; define a non-looped chain
        (Msg "Goodbye World"))
    (schedule main chain-hi)   ;; schedule looped chain (chain-hi) on this node
    (schedule main chain-bye)  ;; schedule non-looped chain (chain-hi) on this node
    (run main)                 ;; run all the scheduled chains on this node
    ```
=== "Result"

    ```
    [info] [2022-03-07 22:14:51.730] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:14:51.731] [T-14836] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 22:14:51.760] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:14:51.776] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:14:51.791] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 22:14:51.823] [T-14836] [logging.cpp::94] [chain-hi] Hello World!
    .
    .
    .
    ```

??? info "See also"
    * [def](#def)

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
