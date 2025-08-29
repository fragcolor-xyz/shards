# Shards Lifecycle
When you run a Shards script, it goes through specific stages before the program is executed. You don’t need to know these stages to write Shards scripts, but this page explains each stage to help you understand what happens under the hood of Shards.

## The Lifecycle

<div style="border:1px solid #ddd;border-radius:12px;padding:16px;max-width:440px;">
  <div style="display:flex;flex-direction:column;align-items:center;gap:6px;line-height:1.6;">
    <div><strong>Read</strong></div>
    <div style="opacity:.6" aria-hidden="true">↓</div>
    <div><strong>Parse</strong></div>
    <div style="opacity:.6" aria-hidden="true">↓</div>
    <div><strong>Construct</strong></div>
    <div style="opacity:.6" aria-hidden="true">↓</div>
    <div><strong>Compose</strong></div>
    <div style="opacity:.6" aria-hidden="true">↓</div>
    <div><strong>Warmup</strong></div>
    <div style="opacity:.6" aria-hidden="true">↓</div>
    <div><strong>Activation</strong></div>
    <div style="opacity:.6" aria-hidden="true">↓</div>
    <div><strong>Cleanup</strong></div>
  </div>
</div>

## Read
The Shards text `.shs` file is read.

## Parse
The read `.shs` file is converted to AST format.

!!! note ".sho file"
    `shards build` can be run to convert your `.shs` file into an AST `.sho` file.

## Construct
The Shards computational graph is constructed from the parsed AST file.

!!! note "What is a Shards computational graph?"
    A computational graph is an intermediate representation of your programme meant for execution. While a Shards text `.shs` and even an AST `.sho` file is formatted for human reading and understanding, a computational graph is specifically meant for computers to understand and process quickly.

!!! note "`#`"
    When you prefix a parenthesized expression with `#`, eg. `#(3 | Math.Add(2))`, it is evaluated at this stage and the resulting value is embedded.

## Compose

The constructed computational graph is composed. Here, each output, input and parameter are validated (checked if they are the correct type for example). Optimizations and the fixing of output types also happens at this stage.

## Warmup
At this stage, objects are created, memory allocated or pre-allocated and data is prepared. 

!!! note "Shards efficiency"
    The Warmup stage is unique to Shards as a coding language and is one main reason why shards is so efficient at running programs. The Warmup stage happens only **once** per wire lifetime. So when a looped wire is scheduled, warmup happens once, then at the activation stage, the looped wire then only has to reuse all the resources that has been prepared at the warmup stage each time it is activated. Consequently, when a non-looped wire is activated multiple times, warmup → activation → cleanup happens at every activation.

## Activation
The actual running of our program happens at this stage. Ideally, only minimal work happens here because of Warmup.

## Cleanup
Memory is freed and deallocated, and resources cleaned up.