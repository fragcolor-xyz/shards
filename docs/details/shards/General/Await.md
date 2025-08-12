`Await` runs its shards (or sequence of shards) as a separate task that is sent directly to the thread pool, while the rest of the program continues executing (via other scheduled threads). Once this `Await` task thread completes its execution the result of the execution of these inner shards is made available to the program.

This is called asynchronous computation and is used to prevent resource intensive processing (like downloading a large file data from an http server) from holding up the execution of the rest of the program.

!!! note
    Await is best used for shards that will take a long time to complete their tasks. Shards like `FS.Read` and `FS.Write` are good candidates to be wrapped in `Await`.