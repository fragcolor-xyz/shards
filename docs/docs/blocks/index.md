---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Introduction

Chainblocks aims to be a meta language or a visual language rather then a pure programming language. To nicely describe
chains and the data they contain, we adopt a simple language based on EDN and Clojure.

Notice that in theory any language can be used, this was just a pragmatic choice. In fact you will also find chains written in C++ digging the Chainblocks codebase.

One can think of it as a comprehensible JSON on steroids that can be evaluated or the Ansible of general programming.

The execution of Chainblocks scripts is meant to be fast: the scripting language is just a description of the computational graph.
The runtime itself is completely detached.

## Some examples

### Super simple example

=== "Code"

    ```clojure
    ; define a node
    (defnode main)
    ; define a simple hello world looped chain
    (defloop hello-world
      (Msg "Hello world"))
    ; schedule the chain for exection
    (schedule main hello-world)
    ; run the node at 1 second intervals
    (run main 1.0) ; <-- execution will block here
    ```

=== "Output"

    ```
    [info] [2021-04-04 16:43:22.290] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:23.299] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:24.291] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:25.289] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:26.291] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:27.299] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:28.296] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:29.292] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:30.295] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:31.298] [T-148944] [logging.cpp::94] [hello-world] Hello world
    [info] [2021-04-04 16:43:32.298] [T-148944] [logging.cpp::94] [hello-world] Hello world
    ```

### Another example

=== "Code"

    ```clojure
    ; define a node
    (defnode main)
    ; define a simple counter looped chain
    (defloop counter
    ; Setup runs only once at chain start
    (Setup
    0 >= .counter)
    ; get counter variable and print
    .counter (Log)
    ; increase the counter
    (Math.Inc .counter)
    ; notice that Math.Inc will passthrough the value
    ; so the value will be before increment
    ; if we wanted to pick up the increment uncomment the next line
    ; .counter
    (When (Is 10) (Stop)))
    ; schedule the chain for exection
    (schedule main counter)
    ; run the node at 1 second intervals
    (run main 1.0) ; <-- execution will block here
    ```

=== "Output"

    ```
    [info] [2021-04-04 19:00:43.045] [T-244992] [logging.cpp::53] [counter] 0
    [info] [2021-04-04 19:00:44.055] [T-244992] [logging.cpp::53] [counter] 1
    [info] [2021-04-04 19:00:45.058] [T-244992] [logging.cpp::53] [counter] 2
    [info] [2021-04-04 19:00:46.057] [T-244992] [logging.cpp::53] [counter] 3
    [info] [2021-04-04 19:00:47.049] [T-244992] [logging.cpp::53] [counter] 4
    [info] [2021-04-04 19:00:48.057] [T-244992] [logging.cpp::53] [counter] 5
    [info] [2021-04-04 19:00:49.046] [T-244992] [logging.cpp::53] [counter] 6
    [info] [2021-04-04 19:00:50.049] [T-244992] [logging.cpp::53] [counter] 7
    [info] [2021-04-04 19:00:51.058] [T-244992] [logging.cpp::53] [counter] 8
    [info] [2021-04-04 19:00:52.054] [T-244992] [logging.cpp::53] [counter] 9
    [info] [2021-04-04 19:00:53.055] [T-244992] [logging.cpp::53] [counter] 10
    ```

### A more complex example

Showing how we mix the Clojure-like language as a template language

=== "Code"

    ```clojure
    ; define a node
    (defnode main)
    ; define a counter template
    (defn make-counter [start]
    ; this time we use the Chain symbol as we want to make a function
    (Chain
    ; customize the chain's name
    (str "counter-" start) :Looped
    ; Setup runs only once at chain start
    (Setup
        ; set start with the start function argument
        start >= .counter)
    ; get counter variable and print
    .counter (Log)
    ; increase the counter
    (Math.Inc .counter)
    ; notice that Math.Inc will passthrough the value
    ; so the value will be before increment
    ; if we wanted to pick up the increment uncomment the next line
    ; .counter
    ; also at chain definition time compute the end value
    ; (+) is not a block, it will be evaluated only once when composing the chain
    (When (Is (+ start 10)) (Stop))))
    ; schedule 2 chains for exection
    (schedule main (make-counter 0))
    (schedule main (make-counter 10))
    ; run the node at 1 second intervals
    (run main 1.0) ; <-- execution will block here
    ```

=== "Output"

    ```
    [info] [2021-04-04 21:05:40.121] [T-285088] [logging.cpp::53] [counter-0] 0
    [info] [2021-04-04 21:05:40.122] [T-285088] [logging.cpp::53] [counter-10] 10
    [info] [2021-04-04 21:05:41.133] [T-285088] [logging.cpp::53] [counter-0] 1
    [info] [2021-04-04 21:05:41.133] [T-285088] [logging.cpp::53] [counter-10] 11
    [info] [2021-04-04 21:05:42.130] [T-285088] [logging.cpp::53] [counter-0] 2
    [info] [2021-04-04 21:05:42.130] [T-285088] [logging.cpp::53] [counter-10] 12
    [info] [2021-04-04 21:05:43.129] [T-285088] [logging.cpp::53] [counter-0] 3
    [info] [2021-04-04 21:05:43.129] [T-285088] [logging.cpp::53] [counter-10] 13
    [info] [2021-04-04 21:05:44.135] [T-285088] [logging.cpp::53] [counter-0] 4
    [info] [2021-04-04 21:05:44.135] [T-285088] [logging.cpp::53] [counter-10] 14
    [info] [2021-04-04 21:05:45.133] [T-285088] [logging.cpp::53] [counter-0] 5
    [info] [2021-04-04 21:05:45.133] [T-285088] [logging.cpp::53] [counter-10] 15
    [info] [2021-04-04 21:05:46.128] [T-285088] [logging.cpp::53] [counter-0] 6
    [info] [2021-04-04 21:05:46.128] [T-285088] [logging.cpp::53] [counter-10] 16
    [info] [2021-04-04 21:05:47.135] [T-285088] [logging.cpp::53] [counter-0] 7
    [info] [2021-04-04 21:05:47.135] [T-285088] [logging.cpp::53] [counter-10] 17
    [info] [2021-04-04 21:05:48.133] [T-285088] [logging.cpp::53] [counter-0] 8
    [info] [2021-04-04 21:05:48.133] [T-285088] [logging.cpp::53] [counter-10] 18
    [info] [2021-04-04 21:05:49.134] [T-285088] [logging.cpp::53] [counter-0] 9
    [info] [2021-04-04 21:05:49.134] [T-285088] [logging.cpp::53] [counter-10] 19
    [info] [2021-04-04 21:05:50.121] [T-285088] [logging.cpp::53] [counter-0] 10
    [info] [2021-04-04 21:05:50.123] [T-285088] [logging.cpp::53] [counter-10] 20
    ```

--8<-- "includes/license.md"
