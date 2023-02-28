---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Shards

This section contains a reference of all the shards available within the Shards language.

## How to read

Let's look at the description for [`String.Join`](./String/Join) as an example:

<div class="sh-parameters" markdown="1">
| Name | - {: #sh-flags-row} | Description | Default | Type |
|------|---------------------|-------------|---------|------|
| `<input>` ||A sequence of string values that will be joined together. | | `[ String ]` |
| `<output>` ||A string consisting of all the elements of the sequence delimited by the separator. | | `String` |
| `Separator` |  | The string to use as a separator. | `""` | `String` |
</div>

The first thing you will see on every shard page is this table.

The first two entries in the table describe what inputs a shard will accept and what kind of values it will output.

### &lt;input&gt;

In this case, the `String.Join` shard accepts a sequence of `Strings` as an input - written as `[String]`. A sequence is identified by its enclosing square brackets.

### &lt;output&gt;

The output will be a single `String`

### Parameters

After the input and output of the shard, the parameters are listed. The `String.Join` shard requires a `Separator` parameter that is a `String`. When the parameter is not specified, the default value will be `""`.

## Optional parameters

<div class="sh-parameters" markdown="1">
| Name | - {: #sh-flags-row} | Description | Default | Type |
|------|---------------------|-------------|---------|------|
| `Window` | :fontawesome-solid-circle-plus:{title="Optional"}  | None or a window variable we wish to use as relative origin. | `None` | `Object ` |
</div>

Occasionally you will see a :fontawesome-solid-circle-plus: icon next to a parameter. This means that the parameter is optional and its value can be left unset.

When a variable is not optional and its default value is not `None`, you should set the parameter on the shard.

## Type descriptions

The type column shows the expected types for an input, output or parameter:

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

When placed within `[ square brackets ]`, this means that a [Sequence](./types/#sequence) of those types is accepted.

When placed within `{ curly brackets }`, this means that a [Table](./types/#table) of those types as values is accepted.

When a value is prefixed with an `&ampersand`, this means that a variable / [ContextVar](./types/#contextvar) of that type is accepted.

## Further reading

See the [Types](./types) page for more information about types.

When placed within `{ curly brackets }`, this means that a [Table](./types/#table) of those types as values is accepted.

When a value is prefixed with an `&ampersand`, this means that a variable / [ContextVar](./types/#contextvar) of that type is accepted.

--8<-- "includes/license.md"
