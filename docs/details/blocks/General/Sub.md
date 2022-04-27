In Chainblocks a block receives an input, processes it, and produces an output. Usually this output is different from the input that the block processed. 

However, in some cases, we want a block (or a sequence of blocks) to return the same value as its output that it received as an input. For example, when we want a block to process the same input that its previous block (or sequence of blocks) processed, irrespective of the output created by the previous block(s).

There is a pass-through parameter available for some blocks (like `Match`) that can allows these blocks to pass-through their own input as their output. For other blocks, we can use the `Sub` block (also called the `Sub` flow) to acheive the same effect by wrapping the target block(s) within the `Sub` .
 
When a `Sub` block recieves an input, it passes that to the first block contained within it. This block processes the input and creates its own ouput that is then passed to the next block in the sequence inside the `Sub` flow. This continues till the last block in the flow is reached. The output of this last block is not used by the `Sub` flow. Instead the `Sub` flow outputs the same value that it had received as input. 

While `Sub` may seem to not use any of its internal blocks' inputs or outputs, every variable assigned inside the `Sub` will be available outside as well since the `Sub` will always be executed. Hence, the inputs and outputs of the blocks within the `Sub` flow can be made available outside the `Sub` by saving them into `Sub` variables.

As a result, this method gives a psuedo pass-through capabilty to any block (if you wrap just that one block with a `Sub` block) or to any sequence of blocks (if you wrap a whole sequence of blocks within a `Sub` block).

By nesting `Sub` blocks you can simulate even more flexible and powerful block execution paths.

!!! note
    `Sub` has an alias `|` which is more convenient to use. `|` also removes the need to use `->` as, unlike `Sub`, it doesn't require the parameter blocks to be grouped together.
