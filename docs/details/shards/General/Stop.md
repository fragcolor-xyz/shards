!!! help
    ```clj
    (defwire my-wire
      (If
        (-> )
        :Then (->)
        :Else (Stop)
    )
    ```
    The above code will produce an error:
    > Stop input and wire output type mismatch, Stop input must be the same type of the wire's output (regular flow)
    