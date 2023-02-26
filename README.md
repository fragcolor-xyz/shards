# Shards

<p align="center">
  <img width="450" src="assets/ShardsLogo.png">
</p>

## Modular, performant scripting for efficient app development.

[![license](https://img.shields.io/github/license/fragcolor-xyz/shards)](./LICENSE)
![CI](https://github.com/fragcolor-xyz/shards/workflows/CI/badge.svg)
[![codecov](https://codecov.io/gh/fragcolor-xyz/shards/branch/devel/graph/badge.svg?token=4PMT2FQFDS)](https://codecov.io/gh/fragcolor-xyz/shards)
[![docs](https://img.shields.io/badge/docs-API-blueviolet)](https://docs.fragcolor.xyz/)
[![examples](https://img.shields.io/badge/learn-examples-blue)](https://learn.fragcolor.xyz/)

## Vision

We believe that technology is on the cusp of a major revolution. As virtual and mixed reality continue to evolve, we see a future where traditional interfaces are replaced by more flexible and intuitive virtual interfaces that are seamlessly integrated into the user's VR/MR/AR environment. We are committed to creating programming tools that will enable developers to build this future and shape the way we interact with technology in the years to come.

With Shards we aim to create a low-code tool for producing high-performance, multi-platform applications. Our approach is to create a scripting tool that is both visual and textual. This tool will represent the flow of data and logic as it is, in a "what you see is how it works" manner.

## Introduction

Shards is a concatenative programming language that emphasizes modularity and composition. This is achieved through the use of shards, which represent individual functional units that can easily be connected with other shards to form complex computational workflows. Each shard can have parameters, which function like properties that manipulate how the shard itself works. This means that developers can create a diverse range of workflows that can be easily customized to meet specific requirements, enabling faster and more efficient deployment of applications.

Shards shares some similarities with stack-based languages such as Forth, as both focus on functional composition and modularity. However, Shards has a unique way of executing code that sets it apart from other languages.

By using Shards, developers can create a diverse range of computational workflows that can be easily customized to meet specific requirements, enabling faster and more efficient deployment of applications.

*Please note that the textual representation of shards may change, and it adopts a Lisp/Clojure syntax mainly for convenience.*

The expression `2 (Math.Add 3) (Math.Add 4)` in the Shards language represents the same computation as the Forth expression `2 3 + 4 +`. However, the difference lies in the way that the computation is represented and executed.

In the Shards language, the expression `2 (Math.Add 3) (Math.Add 4)` would be evaluated as follows:

1. The `Const` shard with an output value of `2` produces its output.
2. The output of the `Const` shard is connected to the input of the first `Math.Add` shard. This shard has a parameter operand of `3`, which results in an output of `5`.
3. The output of the first `Math.Add` shard is connected to the input of the second `Math.Add` shard. The second shard has a parameter operand of `4`, which produces an output of `9`.
4. The output of the second `Math.Add` shard is the final result of the computation, which in this case is `9`.

To summarize, Shards use a concatenative syntax where each shard is represented by its name followed by its parameters (if any) without any additional separators. For example, the expression `2 (Math.Add 3) (Math.Add 4)` is evaluated as you described, with the `Const` (inferred in this case) shard producing an output of `2` that is connected to the first `Math.Add` shard, and so on.


## Goals

This project has two main goals:

### Unleashing creativity

- Prototype quickly and unleash your imagination
- Quickly move from pseudo code to a real app
- Replace pseudo blocks with native blocks in real time, without pausing program execution
- An easy-to-use universal interface for your mixed/native code
- Maximum modularity and inter-project code reuse
- Release a fully bundled executable app or library with just one click

### Accessibility

- Automation scripting for everyone
- Video game engine scripting, even for non-technical artists
- Simplified machine learning scripting with just graph building
- Powerful textual representation (scripting language) for experienced programmers

## Features

Shards has numerous features that make it ideal for live on-the-fly game development, using both visual scripting and traditional game scripting.

### Intuitive

- Supports **visual editing** without the associated spaghetti and spider webs
- Offers a versatile textual representation (currently backed by a derived **Clojure/Lisp**). More information on this can be found [here](https://docs.fragcolor.xyz/docs/shards/).

### Developer-friendly

- Clear data flow
- Extremely easy to debug and dissect
- Automatically inferred strong types
- **SIMD** vectors as first-class types
- Hot code reloading, without any serialization due to completely decoupled data

### Performant

- Performance-driven and easy to profile
- Deterministic execution and performance
- Strong validation, composition, and optimization ahead of run-time
- Built on top of **co-routines**; extremely easy to unleash parallelism and maintain a low syscall count
- Fast execution inspired by threaded code interpreters
- Support for WASM

### Built for game development

- Batteries included: 600+ shards and functions allowing a high level of abstraction
- Game loops: Trivially easy implementation and control of game loop code
- Graphics rendering: A `wgpu` based composable/hackable graphics rendering pipeline implementation

## TL;DR

<details><summary>This code</summary>

```clojure
(defwire action
  (Pause 2.0)
  (Msg "This happened 2 seconds later"))

(defmesh main)

(defloop main-loop
  (GFX.MainWindow
   :Title "My Window"
   :Width 400 :Height 200
   :Contents
   (->
    (Setup
     (GFX.DrawQueue) >= .ui-draw-queue
     (GFX.UIPass .ui-draw-queue) >> .render-steps)
    .ui-draw-queue (GFX.ClearQueue)

    (UI
     .ui-draw-queue
     (UI.Window
      :Title "My UI Window"
      :Contents
      (->
       "Hello world"   (UI.Label)
       "Hello world 2" (UI.Label)
       "Hello world 3" (UI.Label)
       (UI.Button
        "Push me!"
        (-> (Msg "Action!")
            (Detach action)))
       (UI.Checkbox "" .checked)
       .checked
       (When (Is true)
             (-> "Hello optional world" (UI.Label))))))

    (GFX.Render :Steps .render-steps))))

(schedule main main-loop)
(run main 0.02)
```
</details>

<details><summary>Becomes this app (cross-platform)</summary>

  ![](assets/simple1.PNG)

</details>

## Installation

To start developing with Shards, you'll need to [set up your environment](https://docs.fragcolor.xyz/contribute/getting-started/) and then [build Shards](https://docs.fragcolor.xyz/contribute/code/build-shards/).

## Usage

Shards files end with extension `.edn` and can be directly run from the console using the following script (from the `/build` folder):

```
./shards <filename.edn>
```

Shards language API and features are documented [here](https://docs.fragcolor.xyz/docs/) while example codes and tutorials can be found [here](https://learn.fragcolor.xyz/).

## License

Shards source code is licensed under the [BSD 3-Clause license](./LICENSE).
