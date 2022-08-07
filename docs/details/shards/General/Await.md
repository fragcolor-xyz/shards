`(Await)` triggers the execution of its shard (or sequence of shards) and passes the control back to the main program. Once the `(Await)` shard(s) have finished their execution the result of their computation is made available to the prgoram by `(Await)`.

This is called asynchronous computation and is used to prevent resource intensive processing (like downloading a large file data from an http server) from holding up the execution of the rest of the shards.

!!! note
    `(Await)` has an alias `(||)` which is more convenient to use. `||` also removes the need to use `(->)` as, unlike `(Hashed)`, it doesn't require the parameter shards to be grouped together.
