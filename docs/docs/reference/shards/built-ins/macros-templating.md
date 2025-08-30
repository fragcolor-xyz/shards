---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

## @define
Creates a named definition in the current environment. These definitions can then be used inline in your script for substitution. By creating a named definition using a group of shards, you can reduce repeating huge chunks of code and improve readability.

=== "@define example"
    ```shards
    @define(hello-define {
      Msg("Hello")
      Msg("World")
    })

    @wire(wire1 {
      @hello-define
    } Looped: false)
    ```

=== "Outputs"
    ```
    [wire1] Hello
    [wire1] World
    ```

## @template
Similar to `@define`, creates a named definition that accepts arguments. This definition can then be used inline in your script for substitution any number of times.

=== "@define example"
    ```shards
    @template(hello-template [message] {
      Msg(message)
    })

    @wire(wire1 {
      @hello-template("Hello")
      @hello-template("Worldie")
    } Looped: false)
    ```

=== "Outputs"
    ```
    [wire1] Hello
    [wire1] Worldie
    ```

## @ast
Generates an AST JSON string from the provided shards code block. Usually used for `@macro`

=== "@ast example"
    ```shards
    @wire(wire1 {
      @ast({Msg("Hello")}) | Log
    } Looped: false)
    ```
=== "Outputs"
    ```
    {"shs":[[{"sh":{"name":"Msg","params":[{"str":"Hello"}]},"line_info":{"line":29,"column":9,"file":2809170343}}]]}
    ```


## @macro
Creates a reusable code block, with argument substitution

While very similar, there are subtle differences between `@macro` and `@template`. `@macro` produces an AST JSON, which is then parsed/evaluated and then injected into the programme. While `@template` only performs parameter substitution at runtime. Hence, only `@macro` can compute a variable-length program at compile time while `@template` is more geared towards dynamic substitution of arguments.

* Has the following parameters

| Parameter  | Type         | Description                                      |
| ---------- | ------------ | ------------------------------------------------ |
| **Name**   | Identifier   | The macro’s name.                                |
| **Args**   | Sequence     | A list of identifiers representing arguments.    |
| **Shards** | Shards block | The sequence of shards code form the macro body. Must return a sequence |

* The `Shards` parameter for `@macro` must output an AST JSON. `@macro` will throw and error otherwise.

* `@macro` can be used to generate an expression or generate a shards code pipeline.

=== "@macro as an expression"
    ```shards
    @macro(macro1 [v] {
      v >= hello
      " World!" | AppendTo(hello)
      [[{const: {str: hello}}]] | ToJson
    })

    @wire(wire1 {
      1
      Log(@macro1("Hello"))
    } Looped: false)
    ```
=== "Output"
    ```
    [wire1] Hello World!: 1
    ```

=== "@macro to generate pipeline"
    ```shards
    @macro(my-macro [n] {
      Sequence(pipelines)
      Repeat({
        {
          sh: {
            name: "Msg"
            params: [{str: "Hello"}]
          }
        } >> pipelines
      } Times: n)
      [pipelines] | ToJson | Log
    })

    @wire(wire1 {
      @my-macro(3)
    } Looped: false)
    ```
=== "Outputs"
    ```
    [wire1] Hello
    [wire1] Hello
    [wire1] Hello
    ```
* When using `@macro` to generate a pipeline of shards code, the value for the `Shards` parameter needs to be an AST JSON written in a specific format. You can either write it manually or use `@ast` to quickly convert a block of shards code into the required format.

=== "using @ast for @macro"
    ```shards

    @macro(my-macro-1 [] {
      Sequence(pipelines)
      Repeat({
        {
          sh: {
            name: "Msg"
            params: [{str: "Hello1"}]
          }
        } >> pipelines
      } Times: n)
      [pipelines] | ToJson | Log
    })

    @macro(my-macro2 [] {
      @ast({
        Msg("Hello2")
      }) | FromJson | ExpectTable | Take("shs") | ToJson
    })

    @wire(wire1 {
      @my-macro1()
      @my-macro2()
    } Looped: false)

    ```

=== "Outputs"
    ```
    [wire1] Hello1
    [wire1] Hello2
    ```
* `@macro` is also useful for generating and injecting a different code block into your program depending on a specified condition.

=== "conditional @macro example"
    ```shards
    @macro(macro-condition [cond] {
      Sequence(pipelines)
      cond | If(
        Predicate: Is(true) 
        Then: {
          @ast({Msg("Hello")})
        }
        Else: {
          @ast({
            1 | Math.Add(2) | Log("World")
          })
        }
      ) | FromJson | ExpectTable | Take("shs") | ToJson
    })

    @wire(wire1 {
      @macro-condition(true)
      @macro-condition(false)
    } Looped: false)
    ```

=== "Outputs"
    ```shards
    [wire1] Hello
    [wire1] World: 3
    ```

--8<-- "includes/license.md"