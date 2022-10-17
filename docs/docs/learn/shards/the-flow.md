# Shards Flow
A Shards program flows from left to right, top to bottom.
Wires are scheduled on the Mesh, and they are run in the order they are scheduled in.

To gain better control of the flow in your Shards program, you can employ some of the functions described here.


## Do / Dispatch ##
[`Do`](../../docs/shards/General/Do) and [`Dispatch`](../../docs/shards/General/Dispatch) allows you to run a Wire without having to schedule it on a Mesh. This is useful when you wish to reuse a Wire multiple times, similar to a function.


??? "`Do` or `Dispatch`?"
    `Do` disables passthrough, while `Dispatch` has it enabled. This means that `Dispatch` will have its output ignored at the end, while an output can be retrieved from the end of a Wire started with `Do`. 
    
    To learn more about passthrough, see the segment [here](./working-with-data.md/#passthrough).


??? "What is a function?"
    It is a block of code that can be reused to accomplish a specific task.

??? "Why not `defn`?"
    Since [`defn`](../../docs/functions/macros/#defn) is used for creating functions in Shards, you might be wondering why it is not used instead. 
    
    This is due to how a Wire's state persists, unlike a function. A Wire will remember what happened when it was last called, while a function's only role is to take an input, execute its code, and produce an output.

    For the example below, note that we used a Wire so that the number of apples can be tracked within the Wire.

=== "Command"

    ```
    (defmesh main)
    (defwire take-an-apple
      (Setup 10 >= .apples)
      ;decrease the number of apples by 1
      (Math.Dec .apples) 
      .apples (Log "Apples Remaining"))

    (defwire john
      (Msg "Taking an apple!")
      (Do take-an-apple))

    (defwire lucy
      (Msg "Taking an apple!")
      (Do take-an-apple))

    (schedule main john)
    (schedule main lucy)
    (run main)
    ```

=== "Output"

    ```
    [info] [2022-10-17 16:51:39.372] [T-26044] [logging.cpp::98] [john] Taking an apple!
    [info] [2022-10-17 16:51:39.373] [T-26044] [logging.cpp::53] [take-an-apple] Apples Remaining: 9
    [info] [2022-10-17 16:51:39.373] [T-26044] [logging.cpp::98] [lucy] Taking an apple!
    [info] [2022-10-17 16:51:39.373] [T-26044] [logging.cpp::53] [take-an-apple] Apples Remaining: 8
    ```