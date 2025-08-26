---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Built-in functions

Built-in functions are special keywords in Shards, prefixed with `@`, that provide core language features and utility operations. They are directly integrated into the compiler and runtime, making them faster and more predictable than user-defined functions or macros.

They are used for:

* [Data construction](data-contruction.md) – e.g., `@i2`, `@f3`, and `@color` for creating vectors, floats, or color values.

* [Introspection and meta-programming](introspection-meta.md) – e.g., `@type` to generate type descriptors, or `@namespace` to query the current namespace.

* [Platform or context utilities](utilities.md) – e.g., `@platform` to detect the current platform or `@capture-eval-context` to snapshot the current evaluation state.

* [Execution](execution.md) - `@wire`, `@mesh`, `@schedule`, `@run` to execute shards code.

--8<-- "includes/license.md"
