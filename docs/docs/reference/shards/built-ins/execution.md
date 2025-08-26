---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---
# Execution

## @wire
Creates a wire that can be scheduled to a mesh to run shards code.

It has the following parameters.

| Order / Name      | Type                         | Default                                                   | What it does                                                                                  |
| ----------------- | ---------------------------- | --------------------------------------------------------- | --------------------------------------------------------------------------------------------- |
| **0 — Name**      | Identifier                   | — (required)                                              | The wire’s name. Must be an identifier.                                                       |
| **1 — Shards**    | `Shards` sequence            | — (required)                                              | The shards code to execute.  |
| **2 — Traits**    | Sequence of **Trait** values | empty                                                     | Optional traits applied to the wire; each element must be a trait.                            |
| **3 — Looped**    | bool                         | `false`                                                   | Whether the wire runs once or is looped and runs infinitely.                                                              |
| **4 — Pure**      | bool                         | `false` (or forced `true` if impure wires are disallowed) | Marks the wire as pure; they do not copy the parent Wire's variables                             |
| **5 — Unsafe**    | bool                         | `false` (ignored if unsafe is disallowed)                 | Enables “unsafe” behavior when the environment allows it.                                     |
| **6 — StackSize** | integer (bytes)              | env min; rounded up to /4                                 | Custom stack size (min clamp and 4-byte alignment applied) when allowed by env.               |
| **7 — Priority**  | integer                      | `0`                                                       | Execution priority.                                                                           |

=== "@wire example"
    ```shards
      @wire(y {
        "Hello world" |
        Log |
        Pause(0.1)
      } Looped: true)
    ```

## @mesh
Creates the mesh to schedule wires on.

It only has the `Name` parameter

| Parameter | Type       | Required? | Description                                 |
| --------- | ---------- | --------- | ------------------------------------------- |
| **Name**  | Identifier | **Yes**   | The unique name for the mesh to be created. |

=== "@mesh example"
    ```shards
    @mesh(Name: main-mesh) ;;  creates a mesh object called main mesh
    ```

## @schedule
Schedules wires on the specified mesh

It has the following parameters.

| Parameter | Type                | Description                          |
| --------- | ------------------- | ------------------------------------ |
| **Mesh**  | Identifier (string) | The name of the mesh to schedule on. |
| **Wire**  | Identifier (string) | The wire to schedule on that mesh.   |

=== "@schedule example"
    ```shards
      @wire(wire-y {
        "Hello world" |
        Log |
        Pause(0.1)
      } Looped: true)

      @mesh(main-mesh)

      @schedule(main-mesh wire-y) ;; wire-y is now scheduled onto main-mesh
    ```

## @run
| Parameter     | Type                | Description                                                                                                                                                |
| ------------- | ------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Mesh**      | Identifier (string) | The name of the mesh on which to execute the wire.                                                                                                         |
| **Wire**      | Identifier (string) | Specific wire to run on that mesh; if omitted, the default behavior is to run all wires currently registered to the mesh.                                                                                                                               |
| **TickTimer** | Float    | Optional — specifies the interval (in seconds or ticks) between each run.                                                                                  |
| **Runs**      | Integer             | Optional — specifies how many times to run the wire before stopping. `0` or leaving it unset typically means run indefinitely (or until manually stopped). |

=== "@schedule example"
    ```shards
      @wire(wire-y {
        "Hello world" |
        Log
      } Looped: true)

      @mesh(main-mesh)

      @schedule(main-mesh wire-y) ;; wire-y is now scheduled onto main-mesh

      @run(main-mesh 1.0 3) ;; runs main mesh 3 times with a 1.0 second interval in between
      
    ```

--8<-- "includes/license.md"