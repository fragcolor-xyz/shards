# Controlling the flow

Now that we have an understanding of the building blocks of a Shards program, let's see how we can manipulate its control flow.

## Stay DRY with `(defshards)`

We've already seen [macros](https://docs.fragcolor.xyz/functions/macros/) like `(defmesh)`, `(defwire)`, `(defloop)`, `(schedule)`, and `(run)` in action - now, let's look at [`(defshards)`](https://docs.fragcolor.xyz/functions/macros/#defshards).

Imagine we need to program a bot to welcome people to our Shards Bootcamp. The following code is one way to do it.

*Code example 15*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defwire jekyll
        (Msg "Mr. Jekyll")
        (Msg "Welcome to the Shards Bootcamp!")                     ;; common-logic starts
        (Msg "We are delighted to have you with us.")               ;; ...cont'd
        (Msg "Which module would you like to practice today?")      ;; common-logic ends
        )

    (defwire hyde
        (Msg "Mr. Hyde")
        (Msg "Welcome to the Shards Bootcamp!")                     ;; common-logic starts
        (Msg "We are delighted to have you with us.")               ;; ...cont'd
        (Msg "Which module would you like to practice today?")      ;; common-logic ends
        )

    (schedule root jekyll)
    (schedule root hyde)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [trace] [2022-07-28 22:44:42.008] [T-12432] [runtime.cpp::2026] wire jekyll starting
    [info] [2022-07-28 22:44:42.011] [T-12432] [logging.cpp::98] [my-wire-1] Mr. Jekyll
    [info] [2022-07-28 22:44:42.012] [T-12432] [logging.cpp::98] [my-wire-1] Welcome to the Shards Bootcamp!
    [info] [2022-07-28 22:44:42.015] [T-12432] [logging.cpp::98] [my-wire-1] We are delighted to have you with us.
    [info] [2022-07-28 22:44:42.017] [T-12432] [logging.cpp::98] [my-wire-1] Which module would you like to practice today?
    ...
    [trace] [2022-07-28 22:44:42.025] [T-12432] [runtime.cpp::2026] wire hyde starting
    [info] [2022-07-28 22:44:42.026] [T-12432] [logging.cpp::98] [my-wire-2] Mr. Hyde
    [info] [2022-07-28 22:44:42.027] [T-12432] [logging.cpp::98] [my-wire-2] Welcome to the Shards Bootcamp!
    [info] [2022-07-28 22:44:42.029] [T-12432] [logging.cpp::98] [my-wire-2] We are delighted to have you with us.
    [info] [2022-07-28 22:44:42.030] [T-12432] [logging.cpp::98] [my-wire-2] Which module would you like to practice today?
    ```

This is quite verbose.

### Reducing verbosity

So, let's take the common shards (all the `(Msg)` statements except for the first one, in both wires) and group them together, declaring them only *once*, using `(defshards)` (that's what `defshards` stands for - define a group of shards, give it a name, invoke it from anywhere).

*Code example 16*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defshards welcome []
        (Msg "Welcome to the Shards Bootcamp!")                     ;; common-logic starts
        (Msg "We are delighted to have you with us.")               ;; ...cont'd
        (Msg "Which module would you like to practice today?")      ;; common-logic ends
        )

    (defwire jekyll
        (Msg "Mr. Jekyll")
        (welcome)                                                   ;; invoking common-logic group of shards
        )

    (defwire hyde
        (Msg "Mr. Hyde")
        (welcome)                                                   ;; invoking common-logic group of shards
        )

    (schedule root jekyll)
    (schedule root hyde)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [trace] [2022-07-28 23:12:59.817] [T-8028] [runtime.cpp::2026] wire jekyll starting
    [info] [2022-07-28 23:12:59.818] [T-8028] [logging.cpp::98] [jekyll] Mr. Jekyll
    [info] [2022-07-28 23:12:59.819] [T-8028] [logging.cpp::98] [jekyll] Welcome to the Shards Bootcamp!
    [info] [2022-07-28 23:12:59.820] [T-8028] [logging.cpp::98] [jekyll] We are delighted to have you with us.
    [info] [2022-07-28 23:12:59.821] [T-8028] [logging.cpp::98] [jekyll] Which module would you like to practice today?
    ...
    [trace] [2022-07-28 23:12:59.826] [T-8028] [runtime.cpp::2026] wire hyde starting
    [info] [2022-07-28 23:12:59.827] [T-8028] [logging.cpp::98] [hyde] Mr. Hyde
    [info] [2022-07-28 23:12:59.828] [T-8028] [logging.cpp::98] [hyde] Welcome to the Shards Bootcamp!
    [info] [2022-07-28 23:12:59.829] [T-8028] [logging.cpp::98] [hyde] We are delighted to have you with us.
    [info] [2022-07-28 23:12:59.830] [T-8028] [logging.cpp::98] [hyde] Which module would you like to practice today?
    ```

The output remains the same but the code looks more compact!

The common shards are now declared just once (with `defshards`) and given a name, 'welcome'. Now wherever you need this group of shards, you simply need to invoke their name in parentheses i.e., `(welcome)`, and Shards will copy and place this group of shards at the point of the invocation (this happens during the [compose phase](../whats-shards/#composition-phase) of Shards).

!!! note
    Ensure that you invoke the group of shards *after* you have defined them (in your wire/shard flow). If try invoking them before your declare `defshards`, Shards will not be able to locate the group and will throw an error (during the compose phase).

### Passing parameters

There is still room for improvement, though.

See the `[]` next to the `(defshards)` name? That's like an input value for the `defshards` construct. Anything passed into it is available for the `defshards` shards to use. The idea is to move the remaining `(Msg)` shard too to 'welcome', but along with that pass the name as input to the `defshards`.

For this two things need to be done - add a variable to hold the input that will come to 'welcome' - `(defshards welcome [name]...)`. And, pass the appropriate name when invoking 'welcome' from the wires - `(welcome "Mr. Jekyll")`, and `(welcome "Mr. Hyde")`.

Let's see how this pans out in our *Code example 17*.

*Code example 17*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defshards welcome [name]                                       ;; all shards inside 'welcome' have access to the 'name' value
        (Msg name)                                                  ;; yes, `Msg` can print variable values too!
        (Msg "Welcome to the Shards Bootcamp!")
        (Msg "We are delighted to have you with us.")
        (Msg "Which module would you like to practice today?")
        )

    (defwire jekyll
        (welcome "Mr. Jekyll")                                      ;; invoking 'welcome' with name value
        )

    (defwire hyde
        (welcome "Mr. Hyde")                                        ;; invoking 'welcome' with name value
        )

    (schedule root jekyll)
    (schedule root hyde)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [info] [2022-07-28 23:38:14.121] [T-38388] [logging.cpp::98] [jekyll] Mr. Jekyll
    [info] [2022-07-28 23:38:14.122] [T-38388] [logging.cpp::98] [jekyll] Welcome to the Shards Bootcamp!
    [info] [2022-07-28 23:38:14.123] [T-38388] [logging.cpp::98] [jekyll] We are delighted to have you with us.
    [info] [2022-07-28 23:38:14.125] [T-38388] [logging.cpp::98] [jekyll] Which module would you like to practice today?
    ...
    [trace] [2022-07-28 23:38:14.132] [T-38388] [runtime.cpp::2026] wire hyde starting
    [info] [2022-07-28 23:38:14.136] [T-38388] [logging.cpp::98] [hyde] Mr. Hyde
    [info] [2022-07-28 23:38:14.138] [T-38388] [logging.cpp::98] [hyde] Welcome to the Shards Bootcamp!
    [info] [2022-07-28 23:38:14.139] [T-38388] [logging.cpp::98] [hyde] We are delighted to have you with us.
    [info] [2022-07-28 23:38:14.140] [T-38388] [logging.cpp::98] [hyde] Which module would you like to practice today?
    ```

What if you wanted to include a greeting too? Well, pass two parameters to `defshard` 'welcome!
So we make a provision for accepting two parameters in 'welcome'(`defshards welcome [greeting, name]`) and invoke 'welcome' with two values (`(welcome "Good morning!" "Mr. Jekyll")`).

*Code example 18*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defshards welcome [greeting, name]                     ;; all shards inside 'welcome' have access to 'greeting' and 'name' values
        (Msg greeting)                                      ;; yes, `Msg` can print variable values too!
        (Msg name)                                          ;; yes, `Msg` can print variable values too!
        (Msg "Welcome to the Shards Bootcamp!")
        (Msg "We are delighted to have you with us.")
        (Msg "Which module would you like to practice today?")
        )

    (defwire jekyll
        (welcome "Good morning!" "Mr. Jekyll")              ;; invoking 'welcome' with name value
        )

    (defwire hyde
        (welcome "Good evening!" "Mr. Hyde")                ;; invoking 'welcome' with name value
        )

    (schedule root jekyll)
    (schedule root hyde)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [trace] [2022-07-29 11:22:57.578] [T-19808] [runtime.cpp::2026] wire jekyll starting
    [info] [2022-07-29 11:22:57.579] [T-19808] [logging.cpp::98] [jekyll] Good morning!
    [info] [2022-07-29 11:22:57.580] [T-19808] [logging.cpp::98] [jekyll] Mr. Jekyll
    [info] [2022-07-29 11:22:57.583] [T-19808] [logging.cpp::98] [jekyll] Welcome to the Shards Bootcamp!
    [info] [2022-07-29 11:22:57.584] [T-19808] [logging.cpp::98] [jekyll] We are delighted to have you with us.
    [info] [2022-07-29 11:22:57.585] [T-19808] [logging.cpp::98] [jekyll] Which module would you like to practice today?  
    ...
    [trace] [2022-07-29 11:22:57.592] [T-19808] [runtime.cpp::2026] wire hyde starting
    [info] [2022-07-29 11:22:57.594] [T-19808] [logging.cpp::98] [hyde] Good evening!
    [info] [2022-07-29 11:22:57.595] [T-19808] [logging.cpp::98] [hyde] Mr. Hyde
    [info] [2022-07-29 11:22:57.596] [T-19808] [logging.cpp::98] [hyde] Welcome to the Shards Bootcamp!
    [info] [2022-07-29 11:22:57.598] [T-19808] [logging.cpp::98] [hyde] We are delighted to have you with us.
    [info] [2022-07-29 11:22:57.599] [T-19808] [logging.cpp::98] [hyde] Which module would you like to practice today? 
    ```

As you can see, `defshards` is a powerful way to declare a group of shards once but use it multiple times.

!!! note
    In software engineering, this principle is called "Don't repeat yourself" or DRY.

## KISS and `(Sub)` 

`(Sub)` is another useful shard that makes life easy for a Shards programmer. Let's take a look at what it can do.

### Parallel shards

`(Sub)` groups together multiple shards and can pass the same input to all of them. This simulates a form of parallel processing where each shard executes independently and their outputs are not consumed at all.

Also, the `(Sub)` has a passthrough set to `true` internally, hence the output of the whole `(Sub)` block is the input that was passed to it in the first place.

So if your logic requires you to execute multiple shards in parallel, consuming the same input, and executing independently, then you should reach out for `(Sub)`.

Syntactically, `(Sub)` also requires the use of the symbol `->`.

Let's see an example of `(Sub)` in action.

*Code example 19*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defwire mywire
        ;; Using `Sub` with `->` to group together 3 shards
        (int 5) (Log)                       ;; input to the `Sub` and its 3 shards => 5
        (Sub (-> (Math.Multiply 2) (Log)))  ;; output of 1st `Sub` shard => 10
        (Sub (-> (Math.Multiply 3) (Log)))  ;; output of 2nd `Sub` shard => 15  
        (Sub (-> (Math.Multiply 4) (Log)))  ;; output of 3rd `Sub` shard => 20  
        (Log)                               ;; output of all the `Sub`s is passthrough => 5 
        )

    (schedule root mywire)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [info] [2022-08-02 21:14:43.078] [T-24444] [logging.cpp::55] [mywire] 5
    [info] [2022-08-02 21:14:43.079] [T-24444] [logging.cpp::55] [mywire] 10
    [info] [2022-08-02 21:14:43.080] [T-24444] [logging.cpp::55] [mywire] 15
    [info] [2022-08-02 21:14:43.081] [T-24444] [logging.cpp::55] [mywire] 20
    [info] [2022-08-02 21:14:43.082] [T-24444] [logging.cpp::55] [mywire] 5
    ```

### Keeping it simple

As you might notice, the combination of `(Sub)` and `->` tends to get verbose. Fortunately, we have an alternative in the form of `|`. This keyword symbol can replace both `(Sub)` and `->` leading to more succinct and easy-to-read code.

Let's rewrite *Code example 19* using `|` instead (note that `|` replaces the construct `Sub (->`).

*Code example 20*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defwire mywire
        ;; `|` replaces `Sub (->`
        (int 5) (Log)                       ;; input to the `|` and its 3 shards => 5
        (| (Math.Multiply 2) (Log))         ;; output of 1st `|` shard => 10
        (| (Math.Multiply 3) (Log))         ;; output of 2nd `|` shard => 15  
        (| (Math.Multiply 4) (Log))         ;; output of 3rd `|` shard => 20  
        (Log)                               ;; output of all the `|`s is passthrough => 5 
        )

    (schedule root mywire)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [info] [2022-08-02 21:14:43.078] [T-24444] [logging.cpp::55] [mywire] 5
    [info] [2022-08-02 21:14:43.079] [T-24444] [logging.cpp::55] [mywire] 10
    [info] [2022-08-02 21:14:43.080] [T-24444] [logging.cpp::55] [mywire] 15
    [info] [2022-08-02 21:14:43.081] [T-24444] [logging.cpp::55] [mywire] 20
    [info] [2022-08-02 21:14:43.082] [T-24444] [logging.cpp::55] [mywire] 5
    ```

### Simulating passthrough

In section [Null input and passthrough](../anatomy-shard/#null-input-and-passthrough) we talked about passthrough and touched upon the fact that `(Sub)` can simulate passthrough for those shards which do not have this parameter natively.

If you look at *Code example 19* or *Code example 20*, you'll notice how `(Sub)` (and hence by extension, `|`) is able to achieve this feat. For each shard that is wrapped in a `(Sub)` or `(|)`, the input is automatically passing through because the wrapper `(Sub)` or `(|)` has passthrough enabled for itself.

Hence, if you want to simulate passthrough for any shard that doesn't have this parameter natively, simply wrap it in a `(Sub)` + `(->)` shard, or in a `(|)` shard.

!!! note
    KISS is a popular acronym for "Keep it simple, silly!". For this section, however, a more appropriate expansion would be "Killing it with Shards' Sub!".

## Decisions, decisions!

When you're writing code you'll often find the need to check a certain condition and then execute some logic based on the result.

Depending on the condition/check, you might have just two paths (if the result is a yes/no) or multiple paths (if you're matching the input against a finite set of values).

In programming language terminology, these are called control statements. Shards has mnany such constructs that can check for conditions and route the program's control accordingly.

Let's take a look at the most commonly used ones.

### `(If)`-`(Then)`-`(Else)`

```{.clojure .annotate linenums="1"}
(If
  :Predicate [(Shard) (Seq [(Shard)]) (None)]
  :Then [(Shard) (Seq [(Shard)]) (None)]
  :Else [(Shard) (Seq [(Shard)]) (None)]
  :Passthrough [(Bool)]
)
```

The [`(If)`](https://docs.fragcolor.xyz/shards/General/If/) shard evaluates the expression in its `:Predicate` parameter and depending on the output (`true`/`false`), it executes the shards in the `:Then` parameter (if the `:Predicate` expression evaluates to `true`) or the shards in the `:Else` parameter (if the `:Predicate` expression evaluates to `false`). 

This control construct is knows as if-then-else statement in other programming languages.

This shard is useful when you have at least two logic paths and depending on the condition you're checking you need to mandatorily execute *one* of those paths.

*Code example 21*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defshards greetme []               
        (Msg "Hi")                      
    )

    (defshards greetmetoo []
        (Msg "Hello")
    )

    (defshards saybye []                ;; `saybye` invoked from mywire
        (Msg "Bye")                     ;; prints message => Bye
    )

    (defwire mywire
        10 (If (IsLess 2)               ;; 10 > 2 hence condition evaluates to `false`
                :Then (greetme)         
                :Else (saybye)          ;; hence `:Else` shard executed
                )
        )

    (schedule root mywire)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 00:07:46.153] [T-18520] [logging.cpp::98] [mywire] Bye
    ```

### `(When)` and `(WhenNot)`

```{.clojure .annotate linenums="1"}
(When
  :Predicate [(Shard) (Seq [(Shard)]) (None)]
  :Action [(Shard) (Seq [(Shard)]) (None)]
  :Passthrough [(Bool)]
)
```

```{.clojure .annotate linenums="1"}
(WhenNot
  :Predicate [(Shard) (Seq [(Shard)]) (None)]
  :Action [(Shard) (Seq [(Shard)]) (None)]
  :Passthrough [(Bool)]
)
```

The [`(When)`](https://docs.fragcolor.xyz/shards/General/When/) and [`(WhenNot)`](https://docs.fragcolor.xyz/shards/General/WhenNot/) shards work like *If-Then* and *If-Else* components of the `(If)` shard. 

`(When)` executes its `:Action` shards if its `:Predicate` evaluates to `true`; while `(WhenNot)` executes its `:Action` shards if its `:Predicate` evaluates to `false`. Taken together they do what the `(If)` shard does alone.

These shards are useful when you have only one logic path and you need to figure out whether or not to execute it.

*Code example 22*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defshards greetme []                   ;; `greetme` invoked from mywire
        (Msg "Hi")                          ;; prints message => Hi
    )

    (defshards greetmetoo []
        (Msg "Hello")
    )

    (defshards saybye []
        (Msg "Bye")
    )

    (defwire mywire
        10 (When :Predicate (IsMore 2)      ;; 10 > 2 so predicate evaluates to `true`
                 :Action (greetme)          ;; hence `:Action` shard executed
                )
  
        10 (WhenNot :Predicate (IsMore 2)   ;; 10 > 2 so predicate evaluates to `false`
                    :Action (greetme)       ;; hence `:Action` shard not executed
                )                           ;; 'greetme` invoked only once => "Hi" printed only once
        )

    (schedule root mywire)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 00:16:11.860] [T-24532] [logging.cpp::98] [mywire] Hi
    ```

### `(Match)`

```{.clojure .annotate linenums="1"}
(Match
  :Cases [(Seq [(Any)])]
  :Passthrough [(Bool)]
)
```

The [`(Match)`](https://docs.fragcolor.xyz/shards/General/Match/) shard matches the incoming input against the evaluation of its `:Cases` shards. Every case contains a value (or an expression that evaluates to a value) and a sequence of shards.

If the input matches a case's value, the shards for that case are activated. Once a match is found, the rest of the cases are ignored and the control moves on to the next shard. If no match is found, the shard processing ends with it matching with a mandatorily declared `nil` case.

The `(Match)` shard is your best bet when you have multiple logic paths to consider but no complicated conditions to evaluate as`(Match)` can only check for *equality* between the input and the case values.

*Code example 23*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defshards greetme []                   
        (Msg "Hi")                          
    )

    (defshards greetmetoo []                    ;; `greetmetoo` invoked from mywire
        (Msg "Hello")                           ;; prints message => Hello
    )

    (defshards saybye []
        (Msg "Bye")
    )

    (defwire mywire
        10
        (Match [
                2 (-> (greetme))                ;; case processed, match not found
                5 (-> (saybye))                 ;; case processed, match found
                10 (-> (greetmetoo))            ;; case matched hence 'greetmetoo' invoked 
                nil (-> (Msg "Matched nil"))]   ;; case ignored 
            )             
        )

    (schedule root mywire)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 00:33:05.072] [T-2652] [logging.cpp::98] [mywire] Hello
    ```

### `(Cond)`

```{.clojure .annotate linenums="1"}
(Cond
  :Wires [(Seq [(Shard) (Seq [(Shard)]) (None)])]
  :Passthrough [(Bool)]
  :Threading [(Bool)]
)
```

The [`(Cond)`](https://docs.fragcolor.xyz/shards/General/Cond/) contains multiple cases (`:Wires`), each with a condition to evaluate and a sequence of shards to execute. This shard takes the input and evaluates the condition of each case till it reaches a condition that evaluates to true (with the shard's input).

Once a case's condition has been evaluated to be true its shards are triggered for execution and further cases are not considered. If even the last case's condition does not evaluate to `true` the control is passed on to the next shard. 

The `(Cond)` shard is like a combination of `(If)` and `(Match)`. It has the custom logic to flexibly check conditions like `(If)` and a straightforward multi-path switch like `(Match)`. Whether you have a single logic path to consider or multiple logic paths, this shard is a fit for most scenarios. 

*Code example 24*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defshards greetme []                   
        (Msg "Hi")                          
    )

    (defshards greetmetoo []                
        (Msg "Hello")                       
    )

    (defshards saybye []                            ;; `saybye` invoked from mywire                                  
        (Msg "Bye")                                 ;; prints message => Bye
    )

    (defwire mywire
        [10 20 30]
        (Cond
            [(-> (AnyLess 10)) (-> (greetme))       ;; condition `false`
             (-> (AllMore 20)) (-> (greetmetoo))    ;; condition `false`
             (-> (AnyNot 20)) (-> (saybye))         ;; condition `true` hence `saybye` invoked
             (-> true) (-> (Msg "No cond true"))]
            )            
        )

    (schedule root mywire)
    (run root 1 1)
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 00:41:50.215] [T-9552] [logging.cpp::98] [mywire] Bye
    ```

## Loops for everyone

At times you will want to run some logic in a loop until a condition becomes true or a match is found. 

Shards already gives you the ability to run a `(defloop)` wire (and its logic) in a loop with `(run)` control parameters (time interval between two iterations, maximum number of iterations)

*Code example 25*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)
    (defwire mywire
        (Msg "Life in a loop!")     ;; Msg prints 3 times, with 2 sec intervals      
        )

    (schedule root mywire)
    (run root 2 3)                  ;; `mywire` will run 3 times with 2 secs intervals
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 00:52:16.166] [T-15868] [logging.cpp::98] [mywire] Life in a loop!
    [info] [2022-08-03 00:52:18.174] [T-15868] [logging.cpp::98] [mywire] Life in a loop!
    [info] [2022-08-03 00:52:20.178] [T-15868] [logging.cpp::98] [mywire] Life in a loop!
    ```

However, sometimes this is not enough and you need to control your loop based on parameters other than just the number or frequency of wire iterations.

For this, Shards has a couple of iterator or loop constructs.

### `(Repeat)`

```{.clojure .annotate linenums="1"}
(Repeat
  :Action [(Shard) (Seq [(Shard)])]
  :Times [(Int) (Seq [(Int)]) (ContextVar [(Int)]) (ContextVar [(Seq [(Int)])])]
  :Forever [(Bool)]
  :Until [(Shard) (Seq [(Shard)]) (None)]
)
```

[`(Repeat)`](https://docs.fragcolor.xyz/shards/General/Repeat/) runs the shards passed in its `:Action` parameter `:Times` no. of times, or forever if `:Forever` set to true, or till the `:Until` condition is met.

The presence of 3 control parameters (no. of times, forever, condition) makes this a very flexible iterator/loop shard. Since `:Until` is an optional parameter, either `:Times` or `:Forever` needs to be used alongside `:Until`.

This is because `:Times` or `:Forever` are needed to provide the upper limit of iteratins within which the condition in `:Until` would be checked (in a loop).

*Code example 26*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defloop mywire

        (Repeat
            (-> (Msg "I'm forever"))
                ;; :Forever true            ;; if uncommented, will print 'Msg' forever
                )
        
        (Repeat
            (-> (Msg "Times twice"))
                :Times 2)                   ;; print `Msg` twice
        
        0 >= .n1
        (Repeat
            (-> .n1 (Log "Counting n1")
                (Math.Inc .n1))
            :Forever true                   ;; upper limit of iterations is infinity   
            :Until (-> .n1 (IsMore 3)))     ;; so, this will `Log` to its condition max: 4 times 

        0 >= .n2
        (Repeat
            (-> .n2 (Log "Counting n2")
                (Math.Inc .n2))
            :Times 2                        ;; upper limit of iterations is 2 
            :Until (-> .n2 (IsMore 5)))     ;; so, even though condition max is 6, this will log only 2 times

        )

    (schedule root mywire)
    (run root 1 1)        
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 03:29:27.840] [T-20080] [logging.cpp::98] [mywire] Times twice
    [info] [2022-08-03 03:29:27.841] [T-20080] [logging.cpp::98] [mywire] Times twice
    [info] [2022-08-03 03:29:27.843] [T-20080] [logging.cpp::53] [mywire] Counting n1: 0
    [info] [2022-08-03 03:29:27.844] [T-20080] [logging.cpp::53] [mywire] Counting n1: 1
    [info] [2022-08-03 03:29:27.846] [T-20080] [logging.cpp::53] [mywire] Counting n1: 2
    [info] [2022-08-03 03:29:27.847] [T-20080] [logging.cpp::53] [mywire] Counting n1: 3
    [info] [2022-08-03 03:29:27.848] [T-20080] [logging.cpp::53] [mywire] Counting n2: 0
    [info] [2022-08-03 03:29:27.849] [T-20080] [logging.cpp::53] [mywire] Counting n2: 1
    ```


### `(ForRange)`

```{.clojure .annotate linenums="1"}
(ForRange
  :From [(Int) (ContextVar [(Int)])]
  :To [(Int) (ContextVar [(Int)])]
  :Action [(Shard) (Seq [(Shard)]) (None)]
)
```

[`(ForRange)`](https://docs.fragcolor.xyz/shards/General/ForRange/) executes the sequence of shards in `:Action` parameter while an iteration value is between `:From` and `:To` (both inclusive).

*Code example 27*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)
    (defloop mywire
        (ForRange
            1 4
            (-> (ToFloat) (Math.Sqrt) (Log)))  ;; prints square root of 1.0, 2.0, 3.0, and 4.0
        )
    (schedule root mywire)
    (run root 1 1)  
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 03:47:07.625] [T-8900] [logging.cpp::55] [mywire] 1
    [info] [2022-08-03 03:47:07.627] [T-8900] [logging.cpp::55] [mywire] 1.41421
    [info] [2022-08-03 03:47:07.628] [T-8900] [logging.cpp::55] [mywire] 1.73205
    [info] [2022-08-03 03:47:07.629] [T-8900] [logging.cpp::55] [mywire] 2
    ```

### `(ForEach)` and `(Map)`

```{.clojure .annotate linenums="1"}
(ForEach
  :Apply [(Shard) (Seq [(Shard)])]
)
```

```{.clojure .annotate linenums="1"}
(Map
  :Apply [(Shard) (Seq [(Shard)])]
)
```

[`(ForEach)`](https://docs.fragcolor.xyz/shards/General/ForEach/) executes the shards in `:Apply` on (every element of) an input sequence or on (every key/value pair of) an input table.

[`(Map)`](https://docs.fragcolor.xyz/shards/General/Map/) does the same but it only works on sequences. 

*Code example 28*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)
    (defloop mywire
    
        ;; ForEach on a sequence
        [1 2 3]
        (ForEach
            (->
                (| (Math.Multiply 10) (Log))  ;; => 10, 20, 30
                (| (Math.Multiply 100) (Log))  ;; => 100, 200, 300
            ))

        ;; Map on a sequence
        [1 2 3]
        (Map
            (->
                (| (Math.Multiply 2) (Log))  ;; => 2, 4, 6
                (| (Math.Multiply 4) (Log))  ;; => 4, 8, 12
            ))

        ;; ForEach on a table
        {:k1 "3" :k2 "6"}
        ;; ForEach receives each key/value pair as a sequence in alphabetic order of key
        (ForEach    
            (->
                (| (Take 0) (Log "Key"))    ;; => k1, k2
                (| (Take 1) (Log "Value"))  ;; => 3, 6
            ))

        )
    (schedule root mywire)
    (run root 1 1)       
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 04:09:29.815] [T-30780] [logging.cpp::55] [mywire] 10
    [info] [2022-08-03 04:09:29.816] [T-30780] [logging.cpp::55] [mywire] 100
    [info] [2022-08-03 04:09:29.818] [T-30780] [logging.cpp::55] [mywire] 20
    [info] [2022-08-03 04:09:29.820] [T-30780] [logging.cpp::55] [mywire] 200
    [info] [2022-08-03 04:09:29.822] [T-30780] [logging.cpp::55] [mywire] 30
    [info] [2022-08-03 04:09:29.823] [T-30780] [logging.cpp::55] [mywire] 300
    [info] [2022-08-03 04:09:29.825] [T-30780] [logging.cpp::55] [mywire] 2
    [info] [2022-08-03 04:09:29.826] [T-30780] [logging.cpp::55] [mywire] 4
    [info] [2022-08-03 04:09:29.828] [T-30780] [logging.cpp::55] [mywire] 4
    [info] [2022-08-03 04:09:29.830] [T-30780] [logging.cpp::55] [mywire] 8
    [info] [2022-08-03 04:09:29.831] [T-30780] [logging.cpp::55] [mywire] 6
    [info] [2022-08-03 04:09:29.833] [T-30780] [logging.cpp::55] [mywire] 12
    [info] [2022-08-03 04:09:29.834] [T-30780] [logging.cpp::53] [mywire] Key: k1
    [info] [2022-08-03 04:09:29.838] [T-30780] [logging.cpp::53] [mywire] Value: 3
    [info] [2022-08-03 04:09:29.840] [T-30780] [logging.cpp::53] [mywire] Key: k2
    [info] [2022-08-03 04:09:29.841] [T-30780] [logging.cpp::53] [mywire] Value: 6
    ```


### `(Once)` and `(Setup)`

```{.clojure .annotate linenums="1"}
(Once
  :Action [(Shard) (Seq [(Shard)])]
  :Every [(Float)]
)
```

[`(Once)`](https://docs.fragcolor.xyz/shards/General/Once/) executes a sequence of shards (passed in its `:Action` parameter) in a loop with a frequency given by its `:Every` parameter, per wire flow execution.

Or in other words, `(Once)` executes the shards in `:Action` as many times as it can within the allotted wire flow execution time (wire execution frequency * no. of max wire iterations), while maintaining its `:Every` frequency.

*Code example 29*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)
    (defloop mywire
        (Once
            :Action (Msg "Once is never enough")
            ;; `Msg` should execute once every 0.2 seconds
            ;; total available time for it to run = 1 sec (total wire execution time)
            ;; total no. of runs of `Msg` => 1/(0.2) => 5 times
            :Every 0.2)        
        )
    (schedule root mywire)
    ;; wire executes max of 10 times, once in every 10 seconds
    ;; total wire execution time = 0.1 * 10 => 1 sec
    (run root 0.1 10)    
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 02:06:52.121] [T-20648] [logging.cpp::98] [mywire] Once is never enough      
    [info] [2022-08-03 02:06:52.327] [T-20648] [logging.cpp::98] [mywire] Once is never enough      
    [info] [2022-08-03 02:06:52.530] [T-20648] [logging.cpp::98] [mywire] Once is never enough      
    [info] [2022-08-03 02:06:52.733] [T-20648] [logging.cpp::98] [mywire] Once is never enough      
    [info] [2022-08-03 02:06:52.933] [T-20648] [logging.cpp::98] [mywire] Once is never enough 
    ```

When `:Every` is set to 0, `(Once)` runs its `:Action` shards only once per wire flow execution (because an infinite frequency has no meaning and the maximum wire loop execution frequency is the upper limit of `(Once)` execution frequency).

So dropping the `:Every` parameter (i.e., effectively setting it to 0) and using a combination of `(Sub)` and `->` to run multiple shards in `:Action` parameter, we get the following code:

 *Code example 30*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defloop mywire
        (Once
            :Action
            (Sub (->
                (Msg "Just this once!")
                (Msg "No second chances!")
                )))        
        )

    (schedule root mywire)
    (run root 0.1 10)         
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 02:28:14.244] [T-25096] [logging.cpp::98] [mywire] Just this once!
    [info] [2022-08-03 02:28:14.245] [T-25096] [logging.cpp::98] [mywire] No second chances! 
    `
```

As you can see, now the `:Action` shards execute only once for the whole wire flow execution.

This is a useful configuration as we would want our variable and counter initialization code to run only once, and not every time the wire loops (since that would reset our counters and variable initial values). 

Hence, this configuration of `(Once)` is first tuned to replace `Sub` + `->` with `|`, and then aliased as `(Setup)`.

So now we can just use `(Setup)` whenever we want some code to run only for the first time (or once) in wire flow execution.

 *Code example 31*

=== "EDN"

    ```{.clojure .annotate linenums="1"}
    (defmesh root)

    (defloop mywire
        (Setup
            (Msg "Just this once!")
            (Msg "No second chances!")
            )
        )

    (schedule root mywire)
    (run root 0.1 10)          
    ```
    
=== "Result"

    ```
    [info] [2022-08-03 02:40:05.658] [T-1832] [logging.cpp::98] [mywire] Just this once!
    [info] [2022-08-03 02:40:05.661] [T-1832] [logging.cpp::98] [mywire] No second chances!

    ```


--8<-- "includes/license.md"

Built on {{ (git.date or now()).strftime("%b %d, %Y at %H:%M:%S") }}{% if git.status %} from commit [{{ git.short_commit }}](https://github.com/fragcolor-xyz/shards-examples/commit/{{ git.commit }}){% endif %}.

