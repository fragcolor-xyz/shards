---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Overview

An overview of the different areas covered by Shards.

## Math

Various shards exist to allow for the execution of Mathematical calculations.

Basic mathematical operations can be done with the following shards:

- [Math.Add](../../../reference/shards/shards/Math/Add/) (Addition)

- [Math.Subtract](../../../reference/shards/shards/Math/Subtract/) (Subtraction)

- [Math.Multiply](../../../reference/shards/shards/Math/Multiply/) (Multiplication)

- [Math.Divide](../../../reference/shards/shards/Math/Divide/) (Division)

Some shards encapsulate a few operations to help reduce code verbosity and promote simpler code. For example, [Math.Inc](../../../reference/shards/shards/Math/Inc/) can be used to simply increase a variable's value by 1, removing the need to specify the operand and reassigning the new value back to the variable.

- [Math.Inc](../../../reference/shards/shards/Math/Inc/) (Increase value by 1)

- [Math.Dec](../../../reference/shards/shards/Math/Dec/) (Decrease value by 1)

- [Math.Lerp](../../../reference/shards/shards/Math/Lerp/) (Linearly interpolate between two values)

More complex operations can also be executed with shards such as:

- [Math.Exp](../../../reference/shards/shards/Math/Exp/) (Exponential)

- [Math.Mod](../../../reference/shards/shards/Math/Mod/) (Modulus)

- [Math.Pow](../../../reference/shards/shards/Math/Pow/) (Power)

- [Math.Sqrt](../../../reference/shards/shards/Math/Sqrt/) (Square root)

## Strings

These shards allow you to work with strings and text in Shards.

- [AppendTo](../../../reference/shards/shards/General/AppendTo/) (Append to a string)

- [PrependTo](../../../reference/shards/shards/General/PrependTo/) (Prepend to a string)

- [String.Contains](../../../reference/shards/shards/String/Contains/) (Searches for a string)

- [String.Join](../../../reference/shards/shards/String/Join/) (Combines strings)

- [String.ToLower](../../../reference/shards/shards/String/ToLower/) (Converts to lower case)

- [String.ToUpper](../../../reference/shards/shards/String/ToUpper/) (Converts to upper case)

- [String.Trim](../../../reference/shards/shards/String/Trim/) (Trims a string)

## Collections

Collections are used to group multiple items into a single element. There exists various shards that can be used to facilitate working with collections.

Shards to create collections:

- [Sequence](../../../reference/shards/shards/General/Sequence/)

- [Table](../../../reference/shards/shards/General/Table/)

Shards to work with collections:

- [AppendTo](../../../reference/shards/shards/General/AppendTo/) (Append to a collection)

- [PrependTo](../../../reference/shards/shards/General/PrependTo/) (Prepend to a collection)

- [Sort](../../../reference/shards/shards/General/Sort/) (Sort a collection)

- [Take](../../../reference/shards/shards/General/Take/) (Extract elements from a collection)

- [Push](../../../reference/shards/shards/General/Push/) (Adds new elements to the back of a collection)

## Flow

Flow shards allow you to manipulate the flow of your Shards program.

Shards to run another Wire in-line:

- [Do](../../../reference/shards/shards/General/Do/) (Flow stays on the new Wire if paused)

- [Step](../../../reference/shards/shards/General/Step/) (Flow returns to original Wire if paused)

Shards to schedule another Wire:

- [Detach](../../../reference/shards/shards/General/Detach/) (Schedules original Wire)

- [Spawn](../../../reference/shards/shards/General/Spawn/) (Schedules a clone)

- [Branch](../../../reference/shards/shards/General/Branch/) (Schedules Wires on a Submesh)

Shards to control the flow of Wires:

- [Start](../../../reference/shards/shards/General/Start/)

- [Resume](../../../reference/shards/shards/General/Resume/)

- [Stop](../../../reference/shards/shards/General/Stop/)

Shards to run code in bulk:

- [Expand](../../../reference/shards/shards/General/Expand/) (Creates copies of a Wire)

- [TryMany](../../../reference/shards/shards/General/TryMany/) (Creates clones for each input to try)

For more information on Shards Flow, check out the primer guide [here](../../../learn/shards/primer/the-flow/).

## Shaders

Shader shards allow you to write shader code and create material effects in your program.

Shards to aid shader writing: 

- [Shader.Literal](../../../reference/shards/shards/Shader/Literal/)

- [Shader.ReadBuffer](../../../reference/shards/shards/Shader/ReadBuffer/)

- [Shader.SampleTexture](../../../reference/shards/shards/Shader/SampleTexture/)

- [Shader.WriteOutput](../../../reference/shards/shards/Shader/WriteOutput/)

## Graphics

Graphics shards fall under the GFX shards family and are used to create and render objects to your program.

The base of the graphic shards:

- [GFX.Drawable](../../../reference/shards/shards/GFX/Drawable/) (An object to be drawn)

- [GFX.View](../../../reference/shards/shards/GFX/View/) (The camera view)

- [GFX.gITF](../../../reference/shards/shards/GFX/gITF/) (Loads a model from a file)

Shards to set parameters to your drawables:

- [GFX.Texture](../../../reference/shards/shards/GFX/Texture/) (Converts an image)

- [GFX.Material](../../../reference/shards/shards/GFX/Material/) (Creates a reusable setting)

Shards to control what is or will be drawn:

- [GFX.DrawQueue](../../../reference/shards/shards/GFX/DrawQueue/) (Records `Draw` commands)

- [GFX.ClearQueue](../../../reference/shards/shards/GFX/ClearQueue/) (Clears the `Draw` queue)

- [GFX.Draw](../../../reference/shards/shards/GFX/Draw/) (Adds `Drawable`s into the RenderQueue)

- [GFX.Feature](../../../reference/shards/shards/GFX/Feature/) (Allows for customizable shaders)

## UI

UI shards allow you to customize and render User Interfaces in Shards.

Shards to define the segments in your interface:

- [UI.CentralPanel](../../../reference/shards/shards/UI/CentralPanel/)

- [UI.TopPanel](../../../reference/shards/shards/UI/TopPanel/)

- [UI.LeftPanel](../../../reference/shards/shards/UI/LeftPanel/)

- [UI.RightPanel](../../../reference/shards/shards/UI/RightPanel/)

- [UI.BottomPanel](../../../reference/shards/shards/UI/BottomPanel/)

Shards to organize UI elements on your interface:

- [UI.Horizontal](../../../reference/shards/shards/UI/Horizontal/) (Horizontal layout)

- [UI.Group](../../../reference/shards/shards/UI/Group/) (Groups content together)

- [UI.Grid](../../../reference/shards/shards/UI/Grid/) (Organizes content in a grid)

- [UI.Frame](../../../reference/shards/shards/UI/Frame/) (Groups content in a frame)

- [UI.Indent](../../../reference/shards/shards/UI/Indent/)

- [UI.Space](../../../reference/shards/shards/UI/Space/) 

- [UI.Separator](../../../reference/shards/shards/UI/Separator/) 

- [UI.ScrollArea](../../../reference/shards/shards/UI/ScrollArea/)

- [UI.Scope](../../../reference/shards/shards/UI/Scope/)

Shards to add UI elements to your interface:

- [UI.CodeEditor](../../../reference/shards/shards/UI/CodeEditor/) (Code segment)

- [UI.Image](../../../reference/shards/shards/UI/Image/)

- [UI.Hyperlink](../../../reference/shards/shards/UI/Hyperlink/)

- [UI.Label](../../../reference/shards/shards/UI/Label/) (Text)

- [UI.Link](../../../reference/shards/shards/UI/Link/)

- [UI.MenuBar](../../../reference/shards/shards/UI/MenuBar/)

- [UI.ProgressBar](../../../reference/shards/shards/UI/ProgressBar/)

- [UI.Spinner](../../../reference/shards/shards/UI/Spinner/) (Spinning loading widget)

Shards to capture User Input:

- [UI.Button](../../../reference/shards/shards/UI/Button/)

- [UI.Checkbox](../../../reference/shards/shards/UI/Checkbox/)

- [UI.ColorInput](../../../reference/shards/shards/UI/ColorInput/) (Color picker)

- [UI.Combo](../../../reference/shards/shards/UI/Combo/) (Drop-down menu)

- [UI.FloatSlider](../../../reference/shards/shards/UI/FloatSlider/) (Numeric slider)

- [UI.FloatInput](../../../reference/shards/shards/UI/FloatInput/) (Numeric input)

- [UI.RadioButton](../../../reference/shards/shards/UI/RadioButton/)

Shards to draw graphs:

- [UI.Plot](../../../reference/shards/shards/UI/Plot/)

- [UI.PlotBar](../../../reference/shards/shards/UI/PlotBar/)

- [UI.PlotPoints](../../../reference/shards/shards/UI/PlotPoints/)

To customize the User Interface:

- [UI.Style](../../../reference/shards/shards/UI/Style/)

Additional UI controls:

- [UI.Reset](../../../reference/shards/shards/UI/Reset/)

## Inputs

Inputs shards allow you to capture mouse and keyboard inputs and bind actions to them.

Keyboard Captures:

- [Inputs.KeyDown](../../../reference/shards/shards/Inputs/KeyDown/)

- [Inputs.KeyUp](../../../reference/shards/shards/Inputs/KeyUp/)

Mouse Captures:

- [Inputs.MouseDown](../../../reference/shards/shards/Inputs/MouseDown/)

- [Inputs.MousePos](../../../reference/shards/shards/Inputs/MousePos/)

- [Inputs.MouseUp](../../../reference/shards/shards/Inputs/MouseUp/)

## Physics

Physics shards allow you to realize physics concepts in your Shards program.

Shards to simulate objects:

- [Physics.Ball](../../../reference/shards/shards/Physics/Ball/)

- [Physics.Cuboid](../../../reference/shards/shards/Physics/Cuboid/)

- [Physics.DynamicBody](../../../reference/shards/shards/Physics/DynamicBody/)

- [Physics.KinematicBody](../../../reference/shards/shards/Physics/KinematicBody/)

- [Physics.StaticBody](../../../reference/shards/shards/Physics/StaticBody/)

Shards to simulate force:

- [Physics.Impulse](../../../reference/shards/shards/Physics/Impulse/)

- [Physics.Simulation](../../../reference/shards/shards/Physics/Simulation/)

Shards to aid Physics calculations:

- [Physics.CastRay](../../../reference/shards/shards/Physics/CastRay/)

## Conversion

These shards are used for converting between data types.

For type conversions:

- [ToBytes](../../../reference/shards/General/ToBytes/)

- [ToInt](../../../reference/shards/General/ToInt/)

- [ToFloat](../../../reference/shards/General/ToFloat/)

- [ToString](../../../reference/shards/General/ToString/)

For converting between vector types:

- [ToInt2](../../../reference/shards/General/ToInt2/)

- [ToInt3](../../../reference/shards/General/ToInt3/)

- [ToFloat2](../../../reference/shards/General/ToFloat2/)

- [ToFloat3](../../../reference/shards/General/ToFloat3/)

!!! note
    To construct vectors out of components, you can employ the [type's keyword](../../../reference/shards/types).
    
    For example, you can use `(float2 x y)` to create a vector with the values `x` and `y`.

--8<-- "includes/license.md"
