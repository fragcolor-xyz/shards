### <span style="color:red">Warning: Work in progress</span>

# Chainblocks
## A scripting tool to build tools.

## Goals:
* Blocks modularity allows to reuse low level code with minimal efforts between projects
* Machine learning scripting; it's all about building graphs
* Video game engine scripting; even for non-technical artists
* Extremely quick prototyping, creativity unchained
  * From pseudo code to real app
  * Replace pseudo blocks with real native blocks as you go

## Features:
* Clear data flow
* Performance driven
* Extremely easy to debug and dissect
* Deterministic execution and performance
* Built on top of **co-routines**, extremely easy to unleash parallelization
* Support **visual editing** without spaghetti and spider webs involvement
* Versatile textual representation (backed by **clojure/lisp** inspired [Mal](https://github.com/kanaka/mal))
* Fast execution inspired by threaded code interpreters

## TL;DR

### This *(textual; visual GUI version coming soon (tm))*

```clojure
(def! Root (Node))
(schedule Root (Chain "MainLoop" :Looped
  (BGFX.MainWindow :Title "My Window" :Width 400 :Height 400)
  (ImGui.Window "My ImGui Window" :Width 400 :Height 400 
                :PosX 0 :PosY 0 :Contents (--> 
    "Hello world"   (ImGui.Text)
    "Hello world 2" (ImGui.Text) 
    "Hello world 3" (ImGui.SameLine) (ImGui.Text)
    "Hello world 4" (ImGui.SameLine) (ImGui.Text)
    (ImGui.Button "Push me!" (-->
      (Msg "Action!")
    ))))
  (BGFX.Draw)))
(run Root 0.02)
```
### Becomes

![](assets/simple1.PNG)

Complete of a BGFX context, rendering on a DX11 (windows; in this case) surface.

## Current state
Very much work in progress, this project started as a *nim* project, checking the history will show that, but slowly for different many reasons *nim* was fully replaced with *C++*.

The reason behind this switch requires a whole article on it's own, but in a nutshell *nim* is cool but it does not interact very well with *C++* and it's very unstable.

Exactly because of this switch some horrors might still exist, please point them out if you are interested, I'm oriented to move to a more *C++17* rich codebase while keeping a *C* interface.