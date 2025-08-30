---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Built-in functions

Built-in functions are special keywords in Shards, prefixed with `@`, that provide core language features and utility operations. They are directly integrated into the compiler and runtime, making them faster and more predictable than user-defined functions or macros.

They are used for:

* [Data construction](data-construction.md) – e.g., `@i2`, `@f3`, `@color` or `@type` for creating vectors, floats, color values or type descriptions.

* [Introspection and Environment](introspection-environment.md) – e.g., `@namespace`, `@platform`, `@capture-eval`, for querying state and capturing the evaluation context.

* [Execution](execution.md) - `@wire`, `@mesh`, `@schedule`, `@run` to execute shards code.

* [Macros and Templating](macros-templating.md) - `@define`, `@template`, `@macro` and `@ast` to group shards together for the purpose of reusing blocks of code.

--8<-- "includes/license.md"
