---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Introspection & Meta programming

## @type
produces a Type descriptor object in Shards — basically a value that describes a type.

`@type` has the following parameters

| Parameter                                    | Type                        | Description                                                                                                                                                            |
| -------------------------------------------- | --------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Type** (required)                          | `Type` (enum or descriptor) | The base type to describe. This can be a an enum like `Type::Int`, a container type like `[Type::Int]`, or a composite like a table `{ fieldName: Type::X }`, or a shard like `GFX.BuiltinMesh`.     |
| **Variable** (optional)                      | `bool`                      | Marks the type as **variable**. Mainly used internally. |
| **InputType** (optional)                     | `bool`                      | If a shard was provided for the `Type` parameter, `@type ` will generate the output of the provided shard. If `InputType` was set to `true` it will generate the type of the input of the provided shard instead.                    |
| **ObjectVendor / ObjectTypeId / ObjectName** | `string`                    | These fields are used when describing a **custom object type**, where the runtime finds the specified custom object.

=== "@type example"
    ```shards
      Sequence(Name: int-seq Type: @type([Type::Int])) ;; Creates a sequence with the specified type

      {name: "hello" id: @i16(0)} >= temp-table

      temp-table
      Expect(Type: @type({name: Type::String id: Type::Int16})) ;; Verifies the type of the table

      @mesh(foo)
      Const(foo) | Expect(@type(Type::Object ObjectName: "Mesh")) | Log ;; Verifies the type of the Object

      @type(GFX.BuiltinMesh InputType: true) ;; Generates the type of the input of GFX.BuiltinMesh

    ```

## @ast
generates a JSON string of the provided AST fragment

=== "@ast example"
    ```shards
    @ast(
      @wire(wire-name {
        Pass
      })
    ) | FromJson | Expect(@type({func: {params: [{none: Type::Any}]}})) >= wire-ast
    ```

## @platform
returns a string for the current system ("android", "ios", "visionos", "emscripten")

--8<-- "includes/license.md"