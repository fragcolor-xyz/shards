The `:Port` is only set once, even if given a variable and that variable is changed during the execution of the loop.
Having a variable is useful if the port to use is the result of a previous computation, otherwise a constant `int` can directly be used.
