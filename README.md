# Shards

> Shards is a high-performance, multi-platform, type-safe programming language designed for visual development.

<div align="center">

  <a href="">[![license](https://img.shields.io/github/license/fragcolor-xyz/shards)](./LICENSE)</a>
  <a href="">[![CI](https://github.com/fragcolor-xyz/shards/actions/workflows/main.yml/badge.svg)](https://github.com/fragcolor-xyz/shards/actions/workflows/main.yml)</a>
  <a href="">[![codecov](https://codecov.io/gh/fragcolor-xyz/shards/branch/devel/graph/badge.svg?token=4PMT2FQFDS)](https://codecov.io/gh/fragcolor-xyz/shards)</a>
  <a href="">[![docs](https://img.shields.io/badge/docs-8A2BE2)](https://docs.fragcolor.com/)</a>

</div>

<img src="assets/wide-logo.png">

Shards is a dataflow visual programming language that makes app development accessible to all. It uses a straightforward system of connecting visual blocks, called shards, to build full-fledged apps and games without coding. Wires powered by a lightweight cooperative concurrency model, glue sequences of shards together, preventing the complicated spider webs usually associated with visual development. Automatic type checking provides reliability, while optimized shard implementations ensure high performance. The intuitive visual workflow allows beginners to quickly build powerful, multi-platform apps.

In Shards, the code's syntax and its computational graph of interconnected shards and wires semantically represent the same visual model. This synergy allows for seamless round-trip engineering, with visual development applications capable of visually loading the computational graph from the code and subsequently generating its code representation.

Shards powers an upcoming AI-powered game creation system where communities can play and collaborate in real time. While Shards is often not explicitly seen there, it is the language behind the AI-assisted visual interactions that will allow users to create games and experiences in a low to no-code environment.

In Shards, every primitive is a shard, flowing from shard to shard, to build a computational graph that represents a visual model.

## For example

```dart
["Hello " @name] | String.Join | Log >= message
"!" | AppendTo(message)
message | Log("new message")
```

Run with: `./shards new hello.shs name:Astarion`

Will print:

```
[info] [2023-09-23 22:21:34.528] [T-28912] [logging.cpp::68] [root] Hello Astarion
[info] [2023-09-23 22:21:34.532] [T-28912] [logging.cpp::62] [root] new message: Hello Astarion!
```

## Getting started

To start developing with Shards, you'll need to [set up your environment](https://docs.fragcolor.com/contribute/getting-started/) and then [build Shards](https://docs.fragcolor.com/contribute/code/build-shards/).
Shards scripts end with the `.shs` extension and can be directly run from the console using:
```
./build/debug/shards new <filename>.shs
```

Shards language features are documented [here](https://docs.fragcolor.com/learn/shards/primer/), while the API can be found [here](https://docs.fragcolor.com/reference/shards/).

> [!NOTE]
> The code samples in this readme are programmed using the new Shards syntax. The documentation for the new syntax will be released very soon. Stay tuned!

## Goals

Shards achieves zero-compromise on four goals: Reach, Correctness, Performance, and Simplicity.

### Reach

#### Reach people

Using visual development engines like Rare, anyone can build complex applications like games, without prior coding skills.

#### Reach platforms

Shards is available in all major platforms: OS X, Windows, Mac, browser, Android, and iOS.

#### Reach problems

Visual development powered by shards is suitable for solving a wide range of problems, from performance demanding games, to utility shell scripts, with batteries included: 550+ shards for all kinds of purposes.

### Correctness

Shards automatically infer types and validate the compatibility of these types based on the data flowing in and out from shards. This helps catching type errors and ensures that the data passed between shards is of the correct type.

### Performance

#### Zero waste visual development

Zero waste round-trip code generation during visual development due to direct mapping between generated Shards code and loaded computational graph, both representing a visual model.

#### Performant composition

The straightforward architecture flow of shards allows for fast composition of the computational graph, enabling quick loading of programs.

#### Performant runtime

Inferring and validating types at compile time allows programs to run faster, as we no longer need to worry about types during runtime.

#### Primitive performance

Each primitive (shard) in the computational graph was programmed to the highest standards of performance, using C++ and Rust.

#### Parallel performance

Built on top of co-routines. It’s extremely easy to unleash parallelism while maintaining a low syscall count.

### Simplicity

Shards is just shards flowing into shards, thus:

- supporting visual editing without the associated spaghetti and spider webs.
- Trivial implementation and control of game loop code.
- Extremely easy to debug and dissect due to the straight forward flow of shards.
- Low learning curve to make changes to the code directly.

## TL;DR

<details>
<summary>This code:</summary>

```dart
@wire(action {
  Pause(2)
  Msg("This happened 2 seconds later")
})

@wire(main-loop {
  GFX.MainWindow(
    Title: "My Window"
    Width: 400
    Height: 200
    Contents: {
      Once({
        GFX.DrawQueue >= ui-draw-queue
        GFX.UIPass(ui-draw-queue) >> render-steps
      })
      UI(
        ui-draw-queue
        UI.Window(
          Title: "My UI Window"
          Contents: {
            "Hello world" | UI.Label
            "Hello world 2" | UI.Label
            "Hello world 3" | UI.Label
            UI.Button(
              "Push me!"
              {
                Msg("Action!")
                Detach(action)
              })
            UI.Checkbox("" checked)
            checked
            When(Is(true) {
              "Hello optional world" | UI.Label
            })
          }
        )
      )
      GFX.Render(Steps: render-steps)
    }
  )
} Looped: true
)

@mesh(main)
@schedule(main main-loop)
@run(main FPS: 60)
```
</details>

<details>
<summary>Becomes this app:</summary>

![](assets/simple1.PNG)

**Notice that in the meantime shards GUI framework changed to use egui instead of ImGui**

</details>

## License

Shards source code is licensed under the [BSD 3-Clause license](https://github.com/fragcolor-xyz/shards/blob/devel/LICENSE).
