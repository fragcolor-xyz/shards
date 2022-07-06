---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Macros

## def

Defines an alias for a value.

This value may be data, the result of an expression, or the return value of a shard.

=== "Code"

    ```clojure linenums="1"
    ;; define an alias for data
    (def sequence1 [2 4 6 8])
    (def string1 "I'm a string")

    ;; define an alias for an evaluated expression result
    (def xresult (* 2 4))

    ;; define an alias for a shard's return value
    (def my-wire (Wire "my-wire"))
    (def my-looped-wire (Wire "my-looped-wire" :Looped))
    (def Root (Mesh))
    (def - Math.Subtract)
    ```

??? info "See also"
    * [defwire](#defwire)
    * [defloop](#defloop)
    * [defmesh](#defmesh)

## def!

Defines an alias.

=== "Code"

    ```clojure linenums="1"

    ```

## defshards

Defines new shards that can be grouped together and inserted into an existing wire program.

A `defshards` shard looks structurally similar to a function (see [defn](#defn)), but unlike a `defn`, it can contain multiple shards in its body without needing to use `->`.

=== "Code"

    ```clojure linenums="1"
    (defshards MsgParms [input]
        (Msg "My name is")
        (Msg input))
    ```

Just like a function a `defshards` shard can be invoked by name and can process input parameters.

=== "Code"

    ```clojure linenums="1"
    (defmesh main)
    (defshards MsgParms [input]
        (Msg "My name is")
        (Msg input))
    (defwire mywire
        (MsgParms "Shards"))
    (schedule main mywire)
    (run main)
    ```

=== "Result"

    ```
    [info] [2022-05-13 15:31:09.231] [T-4796] [logging.cpp::98] [mywire] My name is
    [info] [2022-05-13 15:31:09.232] [T-4796] [logging.cpp::98] [mywire] Shards
    ```

`(defshards)` can be used in place of a `(defn)` (function declaration) plus `(->)` (shard-container).

See the last two code examples in [`(defn)`](#defn) for a comparison of these use cases.

## defwire

Defines a new non-looped wire.

=== "Code"

    ```clojure linenums="1"
    ;; defwire
    (defwire my-wire
        ;; shards here
    )
    ```

`(defwire <wire-name>)` is actually a shorthand for the more verbose non-looped wire definition: `def <wire-name>` + `Wire "wire-name"`.
=== "Code"

    ```clojure linenums="1"
    ;; def + Wire
    (def my-wire
    (Wire "my-wire"
        ;; shards here
    ))
    ```

A wire is a stateful function (read more about this [here](https://docs.fragcolor.xyz/architecture-guides/shards#stateful-functions) and [here](https://learn.fragcolor.xyz/how-to/shards-primer#stateful-functions)).

To run it you must first schedule it on a mesh. When you run this mesh, all the wires scheduled on it are executed (in the order of scheduling).

A mesh will execute a non-looped wire only once (even though the mesh itself may continue running).

=== "Code"

    ```clojure linenums="1"
    ;; define a mesh (main)
    (defmesh main)
    ;; define a non-looped wire (wire-hello)
    (defwire wire-hello
        (Msg "Hello World!"))
    ;; define another non-looped wire (wire-bye)
    (defwire wire-bye
        (Msg "Goodbye World"))
    ;; schedule the non-looped wires on the mesh
    (schedule main wire-hello)
    (schedule main wire-bye)
    ;; run all the scheduled wires on the mesh
    (run main)
    ```

=== "Result"

    ```
    [info] [2022-03-07 20:45:35.681] [T-9044] [logging.cpp::94] [wire-hello] Hello World!
    [info] [2022-03-07 20:45:35.678] [T-9044] [logging.cpp::94] [wire-bye] Goodbye World
    ```

`defwire` can also parse and save the wire's input into a variable.

=== "Code"

    ```clojure linenums="1"
    (defmesh main)
    (defwire mywire
        = .wirevar                    ;; save mywire input to wirevar
        .wirevar (Log "wire input")) ;; log mywire input to screen
    (defwire mainwire
        "shards" (Do mywire))    ;; invoke mywire with an input
    (schedule main mainwire)
    (run main)
    ```

=== "Result"

    ```
    [trace] [2022-05-12 21:25:49.714] [T-17336] [runtime.cpp::1998] wire mainwire starting
    [info] [2022-05-12 21:25:49.715] [T-17336] [logging.cpp::53] [mywire] wire input: shards
    [debug] [2022-05-12 21:25:49.716] [T-17336] [runtime.cpp::2741] Running cleanup on wire: mainwire users count: 0
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

The processing statements (value/expression/shards) following the `[]` is the function's body and its evaluation is the function's return value. A function may return a single or none at all.

A function can be invoked by calling it by name and passing its required parameters.

Function with no input parameters:

=== "Code"

    ```clojure linenums="1"
    (defmesh main)
    (defn func []
        (Msg "I got no parameters"))    ;; prints string text to screen
    (defwire mywire
        (func))                         ;; function invoked without any parameters
    (schedule main mywire)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:04:22.268] [T-1204] [logging.cpp::98] [mywire] I got no parameters
    ```

Function with one input parameter:

=== "Code"

    ```clojure linenums="1"
    (defmesh main)
    (defn func [param]
        (Msg param))                     ;; prints the parameter to screen
    (defwire mywire
        (func "The only parameter"))     ;; function invoked with a single parameter
    (schedule main mywire)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:03:58.125] [T-21336] [logging.cpp::98] [mywire] The only parameter
    ```

Function with multiple input parameters:

=== "Code"

    ```clojure linenums="1"
    (defmesh main)
    (defn func [param1 param2]
        (Msg param2))                           ;; prints the 2nd parameter to screen
    (defwire mywire
        (func "1st parameter" "2nd parameter")) ;; function invoked with multiple parameters
    (schedule main mywire)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:03:38.570] [T-11468] [logging.cpp::98] [mywire] 2nd parameter
    ```

A function cannot return multiple values. So if you need to process multiple shards in the function's body then you'll have to either use `(defshards)` instead of `(defn)`,

=== "Code"

    ```clojure linenums="1"
    (defmesh main)
    (defshards MsgParams [input]    ;; defshards groups multiple shards for processing
        (Msg "name is:")
        (Msg input))
    (defn letslog [name]
        (MsgParams name))           ;; defshards takes function input and returns single value
    (defwire mywire
        (letslog "shards"))
    (schedule main mywire)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:44:51.293] [T-16928] [logging.cpp::98] [mywire] name is:
    [info] [2022-05-13 14:44:51.294] [T-16928] [logging.cpp::98] [mywire] shards
    ```

or use `(->)` to group and process the multiple shards inside the `(defn)`:

=== "Code"

    ```clojure linenums="1"
    (defmesh main)
    (defn letslog [name]
        (->                             ;; defshards replaced with `->`
            (Msg "name is:")            ;; multiple shards can now be written down sequentially
            (Msg name)
        )
    )
    (defwire mywire
        (letslog "shards"))
    (schedule main mywire)
    (run main)
    ```
=== "Result"

    ```
    [info] [2022-05-13 14:47:44.543] [T-21048] [logging.cpp::98] [mywire] name is:
    [info] [2022-05-13 14:47:44.544] [T-21048] [logging.cpp::98] [mywire] shards
    ```

??? info "See also"
    * [defshards](#defshards)

## defmacro!

Defines a new macro.

=== "Code"

    ```clojure linenums="1"

    ```

## defmesh

Defines a new `mesh` on which wires can be scheduled and then run.

=== "Code"

    ```clojure linenums="1"
    ;; defmesh
    (defmesh main)     ;; define a mesh named 'main'
    ```

`(defmesh <mesh-name>)` is actually a shorthand for the more verbose mesh definition: `def <mesh-name> (Mesh)`.
=== "Code"

    ```clojure linenums="1"
    ;; def + Mesh
    (def main (Mesh))   ;; define a mesh named 'main'
    ```

A mesh is a self-contained execution-context and software environment (like a server) that executes the wire code logic. It can run both on the local hardware as well as peer-to-peer hardware over the network (shardwire).

=== "Code"

    ```clojure linenums="1"
    (defmesh main)             ;; define a mesh (main)
    (defloop wire-hi          ;; define a looped wire
        (Msg "Hello World!"))
    (defwire wire-bye        ;; define a non-looped wire
        (Msg "Goodbye World"))
    (schedule main wire-hi)   ;; schedule looped wire (wire-hi) on this mesh
    (schedule main wire-bye)  ;; schedule non-looped wire (wire-hi) on this mesh
    (run main)                 ;; run all the scheduled wires on this mesh
    ```
=== "Result"

    ```
    [info] [2022-03-07 22:14:51.730] [T-14836] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 22:14:51.731] [T-14836] [logging.cpp::94] [wire-bye] Goodbye World
    [info] [2022-03-07 22:14:51.760] [T-14836] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 22:14:51.776] [T-14836] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 22:14:51.791] [T-14836] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 22:14:51.823] [T-14836] [logging.cpp::94] [wire-hi] Hello World!
    .
    .
    .
    ```

??? info "See also"
    * [def](#def)

## defloop

Defines a new looped wire.

=== "Code"

    ```clojure linenums="1"
    ;; defloop
    (defloop my-loop       ;; define a looped wire
        ;; shards here
    ))
    ```

`(defloop <looped wire-name)` is actually a shorthand for the more verbose looped wire definition: `def <looped wire-name>` + `Wire "looped wire-name"`.
=== "Code"

    ```clojure linenums="1"
    ;; def + Wire
    (def my-loop
    (Wire "my-loop" :Looped
        ;; shards here
    ))
    ```

For a wire to be executed, it must first be scheduled on a `mesh` and then that that mesh needs to run.

=== "Code"

    ```clojure linenums="1"
    (defmesh main)          ;; define a mesh
    (defloop my-loop        ;; define a looped wire
        (Msg "Hello World!"))
    (schedule main my-loop) ;;
    (run main)              ;; run wires scheduled on this mesh
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

A mesh will continue executing a looped wire till the mesh itself stops running (or the wire execution is stopped via a logic condition).

=== "Code"

    ```clojure linenums="1"
    (defmesh main)             ;; define a mesh
    (defloop wire-hi          ;; define a looped wire (wire-hello)
        (Msg "Hello World!"))
    (defloop wire-bye         ;; define another looped wire (wire-bye)
        (Msg "Goodbye World"))
    (schedule main wire-hi)   ;; schedule the wire (wire-hi) on the mesh
    (schedule main wire-bye)  ;; schedule the wire (wire-bye) on the mesh
    (run main)                 ;; run all the wires scheduled on this mesh
    ```
=== "Result"

    ```
    [info] [2022-03-07 22:28:54.682] [T-8432] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 22:28:54.682] [T-8432] [logging.cpp::94] [wire-bye] Goodbye World
    [info] [2022-03-07 22:28:54.715] [T-8432] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 22:28:54.715] [T-8432] [logging.cpp::94] [wire-bye] Goodbye World
    [info] [2022-03-07 22:28:54.731] [T-8432] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 22:28:54.732] [T-8432] [logging.cpp::94] [wire-bye] Goodbye World
    .
    .
    .
    ```

??? info "See also"
    * [def](#def)
    * [defwire](#defwire)


--8<-- "includes/license.md"
