---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Miscellaneous functions

!!! note "Uncategorized"
    This page references all other functions that don't fit in the previous categories. Over time, they might be moved to a different or new category.

## ->

Groups multiple shards together (as if they were a single shard). Also called shard-container.

This is useful for cases where you need multiple shards (to transform your data) but is allowed to have only one: for example, as the return value of a function (see function `print-messages` below).  

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

```
(apply f args) (apply f x args) (apply f x y args) (apply f x y z args)(apply f a b c d & args)
```

Applies fn f to the argument list formed by prepending intervening arguments to args.

```clojure linenums="1"
(apply str ["cherry " "banana " "apple"])
;;=> "cherry banana apple"

(apply str "cherry " ["banana " "apple"])
;;=> "cherry banana apple"

(apply str "cherry " "banana " ["apple"])
;;=> "cherry banana apple"

(apply str "cherry " "banana " "apple" [])
;;=> "cherry banana apple"
```

## atom

```
(atom x)
```

  Creates and returns an Atom with an initial value of x and zero or more options (in any order).
  An Atom is a data type in Clojure that provides a way to manage shared, synchronous, independent state. An atom is just like any reference type in any other programming language.

```clojure linenums="1"
(atom {})
```

## deref

```
(deref ref)
```

Returns the most current state of the atom.

```clojure linenums="1"
(deref (atom [1 2]))
;;=> [1 2]
```

## fn?

```
(fn? f)
```

Return true if f is a function.

```clojure linenums="1"
(fn? String.Join)
;;=> true
```

## hex

```
(hex number)
```

Returns a hexadecimal representation for an input number.

```clojure linenums="1"
(hex 2)
;;=> "0x0000000000000002"
```

## keyword

```
(keyword s)
```

Returns a keyword with a given s string. Do not use `:` in the keyword strings, it will be added automatically.

```clojure linenums="1"
(keyword "foo/bar")
;;=> :foo/bar
```

## macro?

```
(macro? value)
```

Checks if a value is a macro.

```clojure linenums="1"
(macro? defn)
;;=> true
```

## meta

```clojure linenums="1"

```

## read-string

```
(read-string s)
```

Reads one object from the string s.

```clojure linenums="1"
(read-string "2.3") ;;=> 2.300000

(read-string "(+ 1 1)") ;;=> (+ 1 1)
```

## readline

```
(readline prompt)
```

Reads a line from stdin and returns it.
Accepts prompt string.

```clojure linenums="1"
(readline "Enter a line: ")
```

## reset!

```
(reset! atom newval)
```

Sets the value of the atom to newval without regard for the current value. Returns newval.

```clojure linenums="1"
(reset! (atom "apple") "mango")
```

## run

Executes all the wires that have been scheduled on a given `mesh`.

`(run <params>)` takes two arguments: name of the mesh, and mesh iteration interval (no. of seconds between two mesh iterations).

*An optional 3rd argument defines the maximum mesh iterations to run. This argument is a debug parameter - do not use it for production.*

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

```
(swap! atom f) (swap! atom f x) (swap! atom f x y) (swap! atom f x y & args)
```

Atomically swaps the value of atom to be: (apply f current-value-of-atom args). Returns the value that was swapped in.

```clojure linenums="1"
(swap! (atom []) conj "banana")
;;=> ["banana"]
```

## symbol

```
(symbol name)
```

Convert string name to a symbol.

```clojure linenums="1"
(symbol "apple")
;;=> apple
```

## throw

Throws Error using input argument as an error message.

```clojure linenums="1"
(throw "This is an error..")
;; Error: "This is an error.." Line: 1
```

## time-ms

```clojure linenums="1"

```

## with-meta

```clojure linenums="1"

```


--8<-- "includes/license.md"
