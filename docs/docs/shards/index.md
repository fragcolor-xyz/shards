---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Introduction

Shards aims to be a meta language or a visual language rather then a pure programming language. To nicely describe
wires and the data they contain, we adopt a simple language based on EDN and Clojure.

Notice that in theory any language can be used, this was just a pragmatic choice. In fact you will also find wires written in C++ digging the Shards codebase.

One can think of it as a comprehensible JSON on steroids that can be evaluated or the Ansible of general programming.

The execution of Shards scripts is meant to be fast: the scripting language is just a description of the computational graph.
The runtime itself is completely detached.

??? note "Running code samples"
    Many shard code samples (like for the shard [Take](/General/Take)) do not include the wire/mesh context they need to run. To run these samples you need to navigate to `../shards/docs/samples/`and use the following command to provide the necessary boilerplate code to these samples.

    ```
        cd docs/samples
        ../../build/shards.exe run_sample.edn --file "<relative-path-of-sample.edn>"
    ```

## Some examples

### Super simple example

=== "Code"

    ```clojure linenums="1"
    ; define a mesh
    (defmesh main)
    ; define a simple hello world looped wire
    (defloop hello-world
      (Msg "Hello world"))
    ; schedule the wire for exection
    (schedule main hello-world)
    ; run the mesh at 1 second intervals
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

    ```clojure linenums="1"
    ; define a mesh
    (defmesh main)
    ; define a simple counter looped wire
    (defloop counter
    ; Setup runs only once at wire start
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
    ; schedule the wire for exection
    (schedule main counter)
    ; run the mesh at 1 second intervals
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

    ```clojure linenums="1"
    ; define a mesh
    (defmesh main)
    ; define a counter template
    (defn make-counter [start]
    ; this time we use the Wire symbol as we want to make a function
    (Wire
    ; customize the wire's name
    (str "counter-" start) :Looped
    ; Setup runs only once at wire start
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
    ; also at wire definition time compute the end value
    ; (+) is not a shard, it will be evaluated only once when composing the wire
    (When (Is (+ start 10)) (Stop))))
    ; schedule 2 wires for exection
    (schedule main (make-counter 0))
    (schedule main (make-counter 10))
    ; run the mesh at 1 second intervals
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

## Valid data types for shards

This section documents all the valid data types that are accepted by various shards either as their input value or as their parameter values. These data types also apply to the output created by any shard.

Valid data types for every shard are listed under the `Type` column of their Parameters, Input, and Output sections (types are enclosed within parenthesis and if multiple types apply then they are separated by a space).

!!! note
    Types are helpful as they reduce errors in programming. They are also very useful in visual programming as type-matching can be used to reduce the dropdown options when prompting a user on what shard to use next (depending on which shard's input type matches with the current shard's output type).

### Any

Type `Any` indicates that all valid data types are allowed.

For example, `Any` as the allowed data type for input and `:Value` parameter of shard [`(All)`](https://docs.fragcolor.xyz/shards/General/All/) means that `(All)` accepts and compares across all data types.

```clojure linenums="1"
(All
  :Value [(Any)]
)
```

`(All)` compares the input and`:Value` parameter values and returns `true` only if both the value and data type of these entities is equal/same.

=== "Code"

    ```clojure linenums="1"
    [4 5 6] (All :Value [4 5 6])
    (Log)   ;; value and type match => true

    "I'm a string" >= .var1
    "I'm a string" >= .var2
    .var1 (All :Value .var2)
    (Log)   ;; value and type match => true

    "I'm a string" >= .var3
    "I'm a different string" >= .var4
    .var3 (All :Value .var4)
    (Log)   ;; value mismatch => false

    (Float 4.0) >= .var5
    (Int 4) >= .var6
    .var5 (All :Value .var6)
    (Log)   ;; type mismatch => false
    ```

=== "Output"

    ```
    [info] [2022-07-22 13:05:25.848] [T-18072] [logging.cpp::55] [mywire] true
    [info] [2022-07-22 13:05:25.861] [T-18072] [logging.cpp::55] [mywire] true
    [info] [2022-07-22 13:05:25.862] [T-18072] [logging.cpp::55] [mywire] false
    [info] [2022-07-22 13:05:25.864] [T-18072] [logging.cpp::55] [mywire] false
    ```

### None

Type `None` indicates that no data type is expected. This implies that no value is expected.

For example, `None` as one of the valid data types for `:Max` parameter in shard [`(RandomInt)`](https://docs.fragcolor.xyz/shards/General/RandomInt/) means that setting a value for this parameter is not mandatory.

```clojure linenums="1"
(RandomInt
:Max [(None) (Int) (ContextVar [(Int)])]
)
```

`(RandomInt)` generates a random integer and the `:Max` parameter is the upper limit (not inclusive) of the value that can be generated. So it makes sense to have `None` as one of the valid types for this `:Max` parameter for cases when you do not want an upper limit on the random integer.

=== "Code"

    ```clojure linenums="1"
    (RandomInt 8)
    (Log)   ;; max int that can be generated is 7
    
    (RandomInt)
    (Log)   ;; no upper limit on the generated int
    ```

=== "Output"

    ```
    [info] [2022-07-22 13:45:03.282] [T-19992] [logging.cpp::55] [mywire] 4
    [info] [2022-07-22 13:45:03.293] [T-19992] [logging.cpp::55] [mywire] 311828859
    ```

### Enum

`Enum` stands for enumerated data type. 

What this means is that the value you pass to an enumerated variable can only take certain 'states' or named constant values.

For example, in [`(Math.Mean)`](https://docs.fragcolor.xyz/shards/Math/Mean/) the value for `:Kind` parameter needs to be of type `Enum`.

```clojure linenums="1"
(Math.Mean
  :Kind [(Enum)]
)
```

`(Math.Mean)` computes the mean of a sequence of floating-point numbers. But there are three kinds of means - Arithmetic mean, Geometric mean, and Harmonic mean.

So the parameter `:Kind` is defined as an enum variable with these three fixed states :
` :Kind = {Arithmtic mean, Geometric mean, Harmonic mean}`

And hence `:Kind` expects a value that matches one of its possible states. In other words the value you pass in for `:Kind` needs to be an enumerated data type.

In simple terms it just means that you pass in one of the allowed named constant values. Anything else will fail validation.

=== "Code"

    ```clojure linenums="1"
    [2.0 10.0]
    (Math.Mean :Kind Mean.Arithmetic)
    (Log)   ;; AM => 6

    [2.0 10.0]
    (Math.Mean :Kind Mean.Geometric)
    (Log)   ;; GM => 4.47214

    [2.0 10.0]
    (Math.Mean :Kind Mean.Harmonic)
    (Log)   ;; HM => 3.33333
    
    [2.0 10.0]
    (Math.Mean 
    ;; AM is default, anything else will throw error
        ;; :Kind 123
        )   
    (Log)   ;; AM => 6
    ```

=== "Output"

    ```
    [info] [2022-07-22 15:35:00.868] [T-15316] [logging.cpp::55] [mywire] 6
    [info] [2022-07-22 15:35:00.881] [T-15316] [logging.cpp::55] [mywire] 4.47214
    [info] [2022-07-22 15:35:00.882] [T-15316] [logging.cpp::55] [mywire] 3.33333
    [info] [2022-07-22 15:35:00.883] [T-15316] [logging.cpp::55] [mywire] 6
    ```

--8<-- "includes/license.md"
