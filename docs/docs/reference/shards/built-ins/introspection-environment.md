---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Introspection & Environment

## @platform
returns a string for the current system ("android", "ios", "visionos", "emscripten")

=== "@namespace example"
    ```shards
    @wire(wire1 {
        @platform | Log
    } Looped: false)
    ```

=== "Outputs"
    ```
      [wire1] windows ;; dependent on what platform you are on
    ```

## @namespace
Generates the current full namespace as a string. Mainly used internally.

=== "@capture-eval-context example"
    ```shards
    Take("ast")
    Shards.Distill(
      Context: @capture-eval-context
      Name: #(script-path | FS.Filename(true))
      Namespace: @namespace
    ) = wire
    ```


## @capture-eval-context
Captures the current evaluation environment (variables/defs/macros) into a value. Mainly internal use for the `Context` parameter of Shards.Distill.

=== "@capture-eval-context example"
    ```shards
    Take("ast")
    Shards.Distill(
      Context: @capture-eval-context
      Name: #(script-path | FS.Filename(true))
      Namespace: namespace
    ) = wire
    ```

--8<-- "includes/license.md"