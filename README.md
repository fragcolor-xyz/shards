<p align="center">
  <img width="450" src="assets/banner.png">
</p>

## A scripting tool to build tools.

```diff
- Warning: Work in progress
```

## Vision
Building a programming tool for the future. 

A future where the current *Screen* or *Desktop* is replaced by virtual interfaces within a *VR/MR/AR* environment. 

A future that is driven by Artificial Intelligence that wants to become general intelligence, but to make it happen the first step is to focus on Automation and Machine Learning.

To achieve that we build a scripting tool that can be both visual and textual at the same time and that represents the flow of data and logic as it is.

## Goals
* Automation scripting for everyone
* A universal interface to your code
* Visual scripting without wires (similar to Apple Shortcuts)
* Blocks modularity allows to reuse low level code with minimal efforts between projects
* Machine learning scripting; it's all about building graphs
* Video game engine scripting; even for non-technical artists
* Extremely quick prototyping, creativity unchained
  * From pseudo code to real app
  * Replace pseudo blocks with real native blocks as you go
* One click release of a fully bundled executable app or library
* Textual representation that can be used in genetic programming and neural networks

## Features
* Clear data flow
* Performance driven and easy to profile
* Automatically inferred strong types
* **SIMD** vectors as first class types
* Strong validation and optimization ahead of run-time
* Extremely easy to debug and dissect
* Deterministic execution and performance
* Built on top of **co-routines**, extremely easy to unleash parallelization
* Support **visual editing** without spaghetti and spider webs involvement
* Versatile textual representation (backed by **clojure/lisp** inspired [Mal](https://github.com/kanaka/mal))
* Fast execution inspired by threaded code interpreters

## TL;DR

### This *(textual; visual version coming soon (tm))*

```clojure
(def! Root (Node))
(schedule Root (Chain "MainLoop" :Looped
  (BGFX.MainWindow :Title "My Window" :Width 400 :Height 200)
  (ImGui.Window "My ImGui Window" :Width 400 :Height 200 
                :PosX 0 :PosY 0 :Contents (--> 
    "Hello world"   (ImGui.Text)
    "Hello world 2" (ImGui.Text) 
    "Hello world 3" (ImGui.Text)
    "Hello world 4" (ImGui.SameLine) (ImGui.Text)
    (ImGui.Button "Push me!" (-->
      (Msg "Action!")))
    (ImGui.CheckBox)
    (Cond [(--> (Is true)) (-->
        "Hello optional world" (ImGui.Text))])))
  (BGFX.Draw)))
(run Root 0.02)
```
### Becomes

![](assets/simple1.PNG)

Complete of a BGFX context, rendering on a DX11 (windows; in this case) surface.

## Motivation
In all those years of software engineering I've been touching many fields including, audio, VR, physics, game engines in general but also machine learning, NNs etc... everything interesting and all but all the software I wrote didn't exactly connect to each other and connecting it would have been a big effort. I always wanted to mix things and recombine, experiment, after all I started as a musician using max/msp on my black powerbook pismo...

Chainblocks is my answer to this call, a tool that let's me write specific native code and connect it to any other code I wrote.

Where **Chain** would be your procedure and **Blocks** the building blocks of your procedure. 

With a strong emphasis on automation and repeated tasks. Making each frame of execution of your script first class.

## Current state
Very much work in progress, this project started as a *nim* project, checking the history will show that, but slowly for different many reasons *nim* was fully replaced with *C++*.

The reason behind this switch requires a whole article on its own, but in a nutshell *nim* is cool but it does not interact very well with *C++* and it's very unstable.

Exactly because of this switch some horrors might still exist, please point them out if you are interested, I'm oriented to move to a more *C++17* rich codebase while keeping a *C* interface.
