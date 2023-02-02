# Working with Data

## Scope ##

Scope determines the visibility of data at different points in your program. When data is assigned to variables, these variables will either have a **local** or **global** scope.

- Local: The variable is only known within the Wire it originated from.

- Global: The variable is known throughout the entire Mesh.

In the example below, the Wire `retrieve-x` attempts to retrieve the value of `.x`. Note how it is able to retrieve the value of 1 even though it was defined in a separate Wire. This is due to how `.x` has been defined as a global variable, making its value available to all Wires on the Mesh.

=== "Command"
    ```{.clojure .annotate linenums="1"}
    (defmesh main)

    (defwire get-x
      (Get .x :Default 0) (Log "x"))

    (defwire define-x
      1 >== .x) ;; (1)

    (schedule main define-x)
    (schedule main get-x)
    (run main)
    ```

    1. `>==` is the alias for the [`Set`](../../../reference/shards/General/Set/) shard, with the parameter `Global` set to true. This makes `.x` a global variable.

=== "Output"
    ```{.clojure .annotate linenums="1"}
    [get-x] x: 1
    ```

For the following example, `get-x` fails to retrieve the value of `.x` defined in `define-x` and returns the default value of 0. This is due to how `.x` was locally defined within the Wire `define-x` and cannot be accessed by other Wires separate from it.

=== "Command"
    ```{.clojure .annotate linenums="1"}
    (defmesh main)

    (defwire get-x
      (Get .x :Default 0) (Log "x"))

    (defwire define-x
      1 >= .x) ;; (1)

    (schedule main define-x)
    (schedule main get-x)
    (run main)
    ```

    1. `>=` is the alias for the [`Set`](../../../reference/shards/General/Set/) shard, with the parameter `Global` set to false. This makes `.x` a local variable.

=== "Output"
    ```{.clojure .annotate linenums="1"}
    [get-x] x: 0
    ```

!!! note
    If Wire X is scheduled or run from a separate Wire Y using methods such as [`Do`](../../../reference/shards/General/Do) or [`Detach`](../../../reference/shards/General/Detach), the variables on Wire Y will be snapshotted such that Wire X has access to its value at that moment it was called.

    However, this does not mean that Wire X is in the same scope as Wire Y. Wire X holds only a copy of the value it snapshotted from when it was called by Wire Y. It does not have access to the actual variable.

    If a method such as [`Step`](../../../reference/shards/General/Step) is used instead, Wire X would be scheduled on Wire Y itself, giving it the same scope and access to Wire Y's actual variables.

## Converting Data Types ##

## Passthrough ##
