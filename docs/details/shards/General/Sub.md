In Shards, a shard receives an input, processes it, and produces an output. Usually, this output is different from the input that the shard processed.

However, in some cases, we want a shard (or a sequence of shards) to return the same value as its output that it received as an input. For example, when we want a shard to process the same input that its previous shard (or sequence of shards) processed, irrespective of the output created by the previous shard(s).

There is a pass-through parameter available for some shards (like [`Match`](../../../shards/General/Match/)) that can allow these shards to pass through their input to their output. For other shards, we can use the `Sub` shard (also called the `Sub` flow) to achieve the same effect by wrapping the target shard(s) within the `Sub`.

`Sub` passes its input to the first shard contained within. The shard contained within `Sub` then processes the input, creating an output which is then passed to the next shard in the sequence inside the `Sub` flow. This continues till the last shard in the flow is reached. The output of the last shard is not used by the `Sub` flow. Instead, the `Sub` flow outputs the same value that it had received as input.

While `Sub` may seem to not use any of its internal shards' inputs or outputs, every variable assigned inside the `Sub` will be available outside as well since `Sub` will always be executed. Hence, the inputs and outputs of the shards within the `Sub` flow can be made available outside the `Sub` by saving them into `Sub` variables.

As a result, the aforementioned method gives a pseudo-pass-through capability to any shard (if you wrap a shard with a `Sub` shard) or to any sequence of shards (if you wrap a whole sequence of shards within a `Sub` shard).

By nesting `Sub` shards you can simulate even more flexible and powerful shard execution paths.

!!! note
    `Sub` has an alias `|` which is more convenient to use. `|` also removes the need to use `->` as, unlike `Sub`, it doesn't require the parameter shards to be grouped.
