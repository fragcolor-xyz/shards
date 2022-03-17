---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Miscellaneous functions

!!! note "Uncategorized"
    This page references all other functions that don't fit in the previous categories. Overtime, they might be moved to a different or new category.

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

Executes all the chains that have been scheduled on a given `node`.

`(run <params>)` takes two arguments: name of the node, and node iteration interval (no. of seconds between two node iterations).

*An optional 3rd argument defines the maximum node iterations to run. This argument is a debug parameter - do not use for production.*

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
    ;; run the node to execute scheduled chains
    ;; time between node iterations - 0.02 secs
    ;; max node iterations allowed - 5
    (run main 0.02 5)
    ;; The last two args  may also be passed as mathematical expressions
    ;; (run main (/ 1 50) (+ 2 3))
    ```

=== "Result"

    ```
    [info] [2022-03-07 21:42:12.324] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 21:42:12.325] [T-18096] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 21:42:12.351] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 21:42:12.367] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 21:42:12.397] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 21:42:12.413] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    ```

## schedule

Queues a chain for execution on a given node.

Multiple chains can be scheduled on the same `node`. When a node is run, all the chains on it are executed.

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
    ;; run all the scheduled chains on this node (main)
    (run main)
    ```

=== "Result"

    ```
    [info] [2022-03-07 21:42:12.324] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 21:42:12.325] [T-18096] [logging.cpp::94] [chain-bye] Goodbye World
    [info] [2022-03-07 21:42:12.351] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 21:42:12.367] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 21:42:12.397] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
    [info] [2022-03-07 21:42:12.413] [T-18096] [logging.cpp::94] [chain-hi] Hello World!
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
