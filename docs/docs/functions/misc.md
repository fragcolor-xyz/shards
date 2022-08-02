---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Miscellaneous functions

!!! note "Uncategorized"
    This page references all other functions that don't fit in the previous categories. Overtime, they might be moved to a different or new category.

## ->

Groups multiple shards together (as if they were a single shard). Also called shard-container.

This is useful for cases where you need multiple shards (to transform your data) but are allowed to have only one: for example, as the return value of a function (see function `print-messages` below).  

=== "Code"

    ```clojure linenums="1"
    (defn print-messages []
        (-> 
            (Msg "Hello World!")
            (Msg "Hello Universe!")
        ))
    ```

=== "Result"

    ```
    [info] [2022-07-06 14:14:43.056] [T-2168] [logging.cpp::98] [main] Hello World!
    [info] [2022-07-06 14:14:43.058] [T-2168] [logging.cpp::98] [main] Hello Universe! 
    ```

## apply

```clojure linenums="1"

```

## atom

```clojure linenums="1"

```

## deref

```clojure linenums="1"

```

## fn?

```clojure linenums="1"

```

## hex

```clojure linenums="1"

```

## keyword

```clojure linenums="1"

```

## macro?

```clojure linenums="1"

```

## meta

```clojure linenums="1"

```

## read-string

```clojure linenums="1"

```

## readline

```clojure linenums="1"

```

## reset!

```clojure linenums="1"

```

## run

Executes all the wires that have been scheduled on a given `mesh`.

`(run <params>)` takes two arguments: name of the mesh, and mesh iteration interval (no. of seconds between two mesh iterations).

*An optional 3rd argument defines the maximum mesh iterations to run. This argument is a debug parameter - do not use for production.*

=== "Code"

    ```clojure linenums="1"
    ;; define a mesh (main)
    (defmesh main)
    ;; define a looped wire
    (defloop wire-hi
        (Msg "Hello World!"))
    ;; define a non-looped wire
    (defwire wire-bye
        (Msg "Goodbye World"))
    ;; schedule both the wires on this mesh
    (schedule main wire-hi)
    (schedule main wire-bye)
    ;; run the mesh to execute scheduled wires
    ;; time between mesh iterations - 0.02 secs
    ;; max mesh iterations allowed - 5
    (run main 0.02 5)
    ;; The last two args  may also be passed as mathematical expressions
    ;; (run main (/ 1 50) (+ 2 3))
    ```

=== "Result"

    ```
    [info] [2022-03-07 21:42:12.324] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 21:42:12.325] [T-18096] [logging.cpp::94] [wire-bye] Goodbye World
    [info] [2022-03-07 21:42:12.351] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 21:42:12.367] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 21:42:12.397] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 21:42:12.413] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    ```

## schedule

Queues a wire for execution on a given mesh.

Multiple wires can be scheduled on the same `mesh`. When a mesh is run, all the wires on it are executed.

=== "Code"

    ```clojure linenums="1"
    ;; define a mesh (main)
    (defmesh main)
    ;; define a looped wire
    (defloop wire-hi
        (Msg "Hello World!"))
    ;; define a non-looped wire
    (defwire wire-bye
        (Msg "Goodbye World"))
    ;; schedule both the wires on this mesh
    (schedule main wire-hi)
    (schedule main wire-bye)
    ;; run all the scheduled wires on this mesh (main)
    (run main)
    ```

=== "Result"

    ```
    [info] [2022-03-07 21:42:12.324] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 21:42:12.325] [T-18096] [logging.cpp::94] [wire-bye] Goodbye World
    [info] [2022-03-07 21:42:12.351] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 21:42:12.367] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 21:42:12.397] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    [info] [2022-03-07 21:42:12.413] [T-18096] [logging.cpp::94] [wire-hi] Hello World!
    ```

## swap!

```clojure linenums="1"

```

## symbol

```clojure linenums="1"

```

## throw

```clojure linenums="1"

```

## time-ms

```clojure linenums="1"

```

## with-meta

```clojure linenums="1"

```


--8<-- "includes/license.md"
