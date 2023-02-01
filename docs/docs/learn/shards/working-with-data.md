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

For the following example, `get-x` fails to retrieve the value of `.x` defined in `define-x` and returns the default value of 0. This is due to how `.x` was locally defined within the Wire `define-x`.

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

## Converting Data Types ##

## Passthrough ##
