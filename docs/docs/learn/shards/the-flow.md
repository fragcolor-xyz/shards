# Shards Flow
A Shards program flows from left to right, top to bottom.
Wires are scheduled on the Mesh, and they are run in the order they are scheduled in.

To gain better control of the flow in your Shards program, you can employ some of the functions described here.


## Do / Dispatch ##
[`Do`](../../../docs/shards/General/Do) and [`Dispatch`](../../../docs/shards/General/Dispatch) allows you to run a Wire without having to schedule it on a Mesh. This is useful when you wish to reuse a Wire multiple times, similar to a function.


??? "`Do` or `Dispatch`?"
    `Do` disables passthrough, while `Dispatch` has it enabled. This means that `Dispatch` will have its output ignored at the end, while an output can be retrieved from the end of a Wire started with `Do`. 
    
    To learn more about passthrough, see the segment [here](../working-with-data/#passthrough).

??? "Why not `defn`?"
    Since [`defn`](../../../docs/functions/macros/#defn) is used for creating functions in Shards, you might be wondering why is it not used instead. 
    
    This is due to how a Wire's state persists, unlike a function. A Wire will remember what happened when it was last called, while a function's only role is to take an input, execute its code, and produce an output.

    For the example below, note that we used a Wire so that the number of apples can be tracked within the Wire. You can also check out [this](../what-is-shards/#to-wire-or-not) segment in the chapter before for another example.


=== "Command"

    ```{.clojure .annotate linenums="1"}
    (defmesh main) ;; (1)

    (defwire take-an-apple ;; (2)
      (Setup 10 >= .apples) ;; (3)
      (Math.Dec .apples) ;; (4)
      .apples (Log "Apples Remaining")) ;; (5)

    (defwire john
      (Msg "Taking an apple!") ;; (6)
      (Dispatch take-an-apple))

    (defwire lucy
      (Msg "Taking an apple!")
      (Dispatch take-an-apple))

    (schedule main john) ;; (7)
    (schedule main lucy)
    (run main) ;; (8)
    ```

    1. [`defmesh`](../../../docs/functions/macros/#defmesh) is used to define a Mesh.
    2. [`defwire`](../../../docs/functions/macros/#defwire) is used to define a Wire.
    3. The [`Setup`](../../../docs/shards/General/Once/) shard will only be executed once, even within a Looped Wire. This makes it ideal to hold code that is only used once to ready the game.
    4. [`Math.Dec`](../../../docs/shards/Math/Dec/) decreases the value passed in by 1.
    5. ['Log'](../../../docs/shards/General/Log/) displays a value to the user's console. You can pass in a prefix that will be placed before the value.
    6. [`Msg`](../../../docs/shards/General/Msg/) prints out the string passed into it.
    7. [`schedule`](../../../docs/functions/misc/#schedule) queues a Wire on the Mesh.
    8. [`run`](../../../docs/functions/misc/#run) executes Wires on the Mesh.

=== "Output"

    ```
    [john] Taking an apple!
    [take-an-apple] Apples Remaining: 9
    [lucy] Taking an apple!
    [take-an-apple] Apples Remaining: 8
    ```


## Detach / Spawn ##
[`Detach`](../../../docs/shards/General/Detach) and [`Spawn`](../../../docs/shards/General/Spawn) schedules a Wire to run on the same Mesh. 

The difference between `Detach` and `Spawn` is that `Detach` schedules the original Wire itself, while `Spawn` schedules clones of the Wire. This means that there can only be one instance of the detached Wire running, while you can have many instances of the spawned Wire.

`Detach` allows you to pause your current Wire to run the detached Wire by calling the `Wait` shard. It is useful when you have to pause a Wire's execution to wait for something. Use cases would include pausing a program to wait for a file to upload, or waiting for an online transaction to go through.

Back to our previous example with apples, if John now requires some time to juice each apple before taking another, we could use `Detach` and `Wait` to implement this. Note how Lucy continues to take apples while John is still making apple juice. 

=== "Command"

    ```{.clojure .annotate linenums="1"}
    (defmesh main)

    (defwire take-an-apple
      = .name (Log "Actor") ;; (1)
      (Setup 10 >= .apples (Log "Setup"))
      (Math.Dec .apples)
      .apples (Log "Apples Remaining"))

    (defwire juice-apple
      = .name (Log "Actor")
      (Msg "Juicing Apple...") (Pause 1)
      (Msg "Made some Apple Juice!"))

    (defloop john
      (Msg "Taking an apple!")
      "John" (Do take-an-apple)
      "John" (Detach juice-apple)
      (Wait "juice-apple"))

    (defloop lucy
      (Msg "Taking an apple!")
      "Lucy" (Do take-an-apple))

    (schedule main john)
    (schedule main lucy)
    (run main 1 4)
    ```

    1. Save values passed into the Wire by associating them with a variable.

=== "Output"

    ```
    [john] Taking an apple!
    [take-an-apple] Actor: John
    [take-an-apple] Setup: 10
    [take-an-apple] Apples Remaining: 9
    [lucy] Taking an apple!
    [take-an-apple] Actor: Lucy
    [take-an-apple] Apples Remaining: 8
    [juice-apple] Actor: John
    [juice-apple] Juicing Apple...
    [lucy] Taking an apple!
    [take-an-apple] Actor: Lucy
    [take-an-apple] Apples Remaining: 7
    [juice-apple] Made some Apple Juice!
    [lucy] Taking an apple!
    [take-an-apple] Actor: Lucy
    [take-an-apple] Apples Remaining: 6
    [john] Taking an apple!
    [take-an-apple] Actor: John
    [take-an-apple] Apples Remaining: 5
    [lucy] Taking an apple!
    [take-an-apple] Actor: Lucy
    [take-an-apple] Apples Remaining: 4
    [juice-apple] Actor: John
    [juice-apple] Juicing Apple...

    ```

If you tried `(Detach juice-apple)` for Lucy too, you will notice that the juicing does not happen on Lucy's end. This is due to how `Detach` is scheduling the original Wire, and only one instance of it can be scheduled at any time. When John is using the juicer to make apple juice, Lucy cannot use it.

Now say we have a large oven which can bake multiple apples concurrently. We can use `Spawn` to make clones of a `bake-apple` Wire that can be scheduled to run together.

=== "Command"

    ```{.clojure .annotate linenums="1"}
    (defmesh main)

    (defwire take-an-apple
      = .name (Log "Actor")
      (Setup 10 >= .apples)
      (Math.Dec .apples)
      .apples (Log "Apples Remaining"))

    (defwire bake-apple
      = .name (Log "Actor")
      (Msg "Baking Apple...") (Pause 1)
      (Msg "Made a Baked Apple!"))

    (defloop john
      (Msg "Taking an apple!")
      "John" (Do take-an-apple)
      "John" (Spawn bake-apple))

    (defloop lucy
      (Msg "Taking an apple!")
      "Lucy" (Do take-an-apple))

    (schedule main john)
    (schedule main lucy)
    (run main 1 4)

    ```

=== "Output"

    ```
    [john] Taking an apple!
    [take-an-apple] Actor: John
    [take-an-apple] Apples Remaining: 9
    [lucy] Taking an apple!
    [take-an-apple] Apples Remaining: 8
    [bake-apple-1] Actor: John
    [bake-apple-1] Baking Apple...
    [john] Taking an apple!
    [take-an-apple] Actor: John
    [take-an-apple] Apples Remaining: 7
    [lucy] Taking an apple!
    [take-an-apple] Actor: Lucy
    [take-an-apple] Apples Remaining: 6
    [bake-apple-2] Actor: John
    [bake-apple-2] Baking Apple...
    [john] Taking an apple!
    [take-an-apple] Actor: John
    [take-an-apple] Apples Remaining: 5
    [lucy] Taking an apple!
    [take-an-apple] Actor: Lucy
    [take-an-apple] Apples Remaining: 4
    [bake-apple-1] Made a Baked Apple!
    [bake-apple-2] Made a Baked Apple!
    [bake-apple-3] Actor: John
    [bake-apple-3] Baking Apple...
    [john] Taking an apple!
    [take-an-apple] Actor: John
    [take-an-apple] Apples Remaining: 3
    [lucy] Taking an apple!
    [take-an-apple] Actor: Lucy
    [take-an-apple] Apples Remaining: 2
    [bake-apple-2] Actor: John
    [bake-apple-2] Baking Apple...
    ```

If you added `(Spawn bake-apple)` for Lucy, you will notice that Lucy starts to bake apples along with John! Unlike `Detach`, you can have multiple instances of a spawned Wire running.

Use cases would include spawning multiple same projectiles (such as bullets fired from a gun), or spawning monster mobs with many instances of one monster type.

## Start / Resume ##
[`Start`](../../../docs/shards/General/Start) schedules a Wire to run on the same Mesh, in place of the current Wire.

[`Resume`](../../../docs/shards/General/Resume) will resume a suspended Wire from where it was last paused at.

!!! note
    If `Resume` is used on a Wire that has not been scheduled yet, it will behave as `Start` would and schedule the Wire on the Mesh before starting it.

`Start` and `Resume` are useful when managing different states. 

For example, your game might `Start` the player in Zone 1. When the player moves to Zone 2, you could `Start` Zone 2's Wire. When the player returns to Zone 1, you could simply `Resume` Zone 1's Wire and any previous changes made by the player in Zone 1 would still persist.

In the example below, we use `Start` and `Resume` to toggle between John's and Lucy's turn. Note how `Resume` redirects the flow back to exactly where `john` was paused at.

=== "Command"
    ```{.clojure .annotate linenums="1"}
    (defmesh main)

    (defwire take-an-apple
      (Setup 10 >= .apples)
      (Math.Dec .apples)
      .apples (Log "Apples Remaining"))

    (defloop lucy
      (Setup 0 >= .apple-count) 
      (Msg "Taking an apple!") 
      (Dispatch take-an-apple)
      (Math.Inc .apple-count)
  
      (When
       :Predicate (-> .apple-count (IsMore 2))
       :Action 
       (-> (Msg "I have enough, you can have the rest.") 
           (Resume)))) ;; (1)

    (defloop john
      (Setup
       (-> (Msg "Lucy, you can take as much as you want first.")
           (Start lucy) ;; (2)
           (Msg "It's my turn now!"))) 
      (Msg "Taking an apple!")
      (Dispatch take-an-apple))

    (schedule main john)
    (run main (/ 1 60) 6) ;; (3)
    
    ```

    1. Returns the flow back to the Wire that started it, which is `john` in this case.
    2. Starts the `lucy` Wire and redirects the program's flow to it.
    3. `(/ 1 60)` is read as "1 divided by 60". It is used to get the program to run at 60 FPS (Frames Per Second).

=== "Output"

    ```
    [john] Lucy, you can take as much as you want first.
    [lucy] Taking an apple!
    [take-an-apple] Apples Remaining: 9
    [lucy] Taking an apple!
    [take-an-apple] Apples Remaining: 8
    [lucy] Taking an apple!
    [take-an-apple] Apples Remaining: 7
    [lucy] I have enough, you can have the rest.
    [john] It's my turn now!
    [john] Taking an apple!
    [take-an-apple] Apples Remaining: 6
    [john] Taking an apple!
    [take-an-apple] Apples Remaining: 5
    ```

## Stop ##

[`Stop`](../../../docs/shards/General/Stop) is used to end Wires. It is very useful for managing Wires created with `Detach` or `Spawn`.
For example, if you have spawned multiple monsters, you could set them to `Stop` running once their health reaches 0.

!!! note
    If `Stop` is used on a Wire that is running from `Start` or `Resume`, the Wire itself is stopped and the entire program will end.

For our example, we use `Stop` to end `bake-apple` looped Wires after they iterate twice.

=== "Command"
    ```{.clojure .annotate linenums="1"}
    (defmesh main)

    (defloop bake-apple
      (Setup
       (-> (Msg "Started Baking")
           0 >= .timer))
      (Math.Inc .timer)
      .timer (Log "Time Baked")

      (When
       :Predicate (-> .timer (Is 2))
       :Action (-> 
                (Msg "Apple is Baked!")
                (Stop))))

    (defloop john
      (Msg "Baking Apple...")
      (Spawn bake-apple))

    (schedule main john)
    (run main 1 3)
    ```

=== "Output"

    ```
    [john] Baking Apple...
    [bake-apple-1] Started Baking
    [bake-apple-1] Time Baked: 1
    [john] Baking Apple...
    [bake-apple-1] Time Baked: 2
    [bake-apple-1] Apple is Baked!
    [bake-apple-2] Started Baking
    [bake-apple-2] Time Baked: 1
    [john] Baking Apple...
    [bake-apple-2] Time Baked: 2
    [bake-apple-2] Apple is Baked!
    [bake-apple-1] Started Baking
    [bake-apple-1] Time Baked: 1
    ```

<!-- ## Step / Branch / Stepmany ##

[`Step`](../../docs/shards/General/Step) schedules another Wire on the Wire calling `Step` itself. That is, if `X Step Y`, Y  is scheduled to run on X itself. -->

<!-- TODO: Clarify that understanding is correct first -->

## Expand ##

- Creates and schedules copies of a Wire

- Returns an array of the output from all the copies 

[`Expand`](../../../docs/shards/General/Expand) is useful when you need to run code in bulk. The results produced can then be evaluated, which is useful in Machine Learning for example.

??? "Multithreading with `Expand`"
    Simple programs are usually run on a single thread. You can think of a thread as a thought process. For a Computer to be able to "multitask", they require multiple threads. 
    
    `Expand` has the parameter `:Threads` which allows you to specify the number of threads to use. Multithreading can improve performance when attempting to `Expand` a Wire to a large size.

In our example below, we will be using `Expand` to teach John about multiplication with zeros.

=== "Command"
    ```{.clojure .annotate linenums="1"}
    (defmesh main)

    (defwire learn-zero-multiplication 
      (Expand
       :Size 100 ;; (1)
       :Wire (defwire zero-multiplication 
               (RandomInt :Max 100)(Math.Multiply 0))) ;; (2)
      (ForEach (-> (Is 0)(Log)))) ;; (3)

    (defwire john
      (Do learn-zero-multiplication))

    (schedule main john)
    (run main)

    ```

    1. Creates and runs 100 copies of the Wire `zero-multiplication`.
    2. Generates a random number from 0 to 99 and multiplies it with 0.
    3. `Expand` outputs an array of the results. We use [`ForEach`](../../../docs/shards/General/ForEach/) to check if each result [`Is`](../../../docs/shards/General/Is/) 0.

=== "Output"

    ```
    [learn-zero-multiplication] true
    [learn-zero-multiplication] true
    [learn-zero-multiplication] true
    [learn-zero-multiplication] true
    ...

    ```

<!-- ## TryMany ##

- Creates and schedules copies of a Wire -->

<!-- TODO: TryMany is not working yet -->

--8<-- "includes/license.md"
