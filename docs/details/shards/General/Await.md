`Await` runs its shards (or sequence of shards) as a separate task that is sent directly to the thread pool, while the rest of the program continues executing (via other scheduled threads). Once this `Await` task thread completes its execution the result of the execution of these inner shards is made available to the program.

This is called asynchronous computation and is used to prevent resource intensive processing (like downloading a large file data from an http server) from holding up the execution of the rest of the program.

!!! note
    `Await` has an alias `||` which is more convenient to use. `||` also removes the need to use `{}` as, unlike `Await`, it doesn't require the parameter shards to be grouped together.
