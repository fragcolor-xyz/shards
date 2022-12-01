---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Shards

This section contains a reference of all the shards available within the shards language.

## How to read

To see how to read the shard description, let's look at [`String.Join`](./String/Join) as an example:

<div class="sh-parameters" markdown="1">
| Name | - {: #sh-flags-row} | Description | Default | Type |
|------|---------------------|-------------|---------|------|
| `<input>` ||A sequence of string values that will be joined together. | | `[ String ]` |
| `<output>` ||A string consisting of all the elements of the sequence delimited by the separator. | | `String` |
| `Separator` |  | The string to use as a separator. | `""` | `String` |
</div>

The first thing you will see on every shard page is this table.

The first two entries in the table describe what inputs a shard will accept and what kind of values it will output.

### <input>

In this case the `String.Join` shard accepts a sequence of `Strings` as an input, written as `[ String ]`, the square brackets meaning sequence.

### <output>

The output will be a single `String`

### Parameters

After the input and output of the shard, the parameters are listed. The `String.Join` shard requires a `Separator` parameter and it has to be a `String`, the default value when not specified will be `""`


## Optional parameters

<div class="sh-parameters" markdown="1">
| Name | - {: #sh-flags-row} | Description | Default | Type |
|------|---------------------|-------------|---------|------|
| `Window` | :fontawesome-solid-circle-plus:{title="Optional"}  | None or a window variable we wish to use as relative origin. | `None` | `Object ` |
</div>

Occasionally you will also see a :fontawesome-solid-circle-plus: icon next to a parameter like this, this means that the parameter is optional and it's value can be left unset.

When a variable is not optional and it's default value is not `None` you should set the parameter on the shard.

## Type descriptions

The type column shows the expected types for a input/output/parameter

<div class="sh-parameters" markdown="1">
| Name | Type |
|------|----- |
| `Values` | `String Float` |
| `Sequence` | `[ String Float ]` |
| `Table` | `{ String Float }` |
| `Variables` | `&String &Float` |
| `SequenceVariable` | `&[ Float4 ]` |
</div>

Multiple different types in a row indicate that any of the types listed are accepted.

When placed within `[ square brackets ]` this means that a sequence of those types is accepted

When placed within `{ curly brackets }` this means that a table of those types as values accepted

When a value is prefixed with an `&ampersand` this means that a variable of that type is accepted

### Special cases

When the type `Any` is specified this means that any type is accepted, or the shard has more complex rules about the valid types. Check the relevant shard documentation for more info.

When the type `Shard [ Shards ]` is specified this means that the shard accepts other shards for that parameter. More specifically it accepts a single shard `:ParameterName (Shard ...)` or sequence of shards `:ParameterName (-> (Shard1 ...) (Shard2 ...))`

--8<-- "includes/license.md"
