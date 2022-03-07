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

Executes all the chains that have been scheduled on a given node.

`run` takes three arguments: the name of the node, number of seconds to elapse between chain executions, and the maximum number of chain executions allowed. 

*The last two arguments apply only to looped (defloop) chains and are ignored for other (defchain) chains.*

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
    ;; run the scheduled chains:
    ;; 1. the looped chain will run once every 0.02 secs (i.e. at 50 FPS) and for 5 iterations
    ;; 2. the non-looped chain will run only once (FPS and iterations are ignored)
    (run main 0.02 5)
    ;; The last two args may also be passed as mathematical expressions
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
