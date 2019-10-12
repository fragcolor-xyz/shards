### <span style="color:red">Warning: Work in progress</span>

# Chainblocks
## A scripting tool to build tools.

## Features:
* Clear data flow
* Performance driven
* Extremely easy to debug and dissect
* Deterministic execution and performance
* Built on top of **co-routines**, extremely easy to unleash parallelization
* Support **visual editing** without spaghetti and spider webs involvement
* Versatile textual representation (backed by **clojure/lisp** inspired [Mal](https://github.com/kanaka/mal))

## TL;DR

### This

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