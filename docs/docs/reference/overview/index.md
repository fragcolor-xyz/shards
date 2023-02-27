---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---

# Overview

An overview of the different areas covered by Shards.

## Math

Various shards exist to allow for the execution of Mathematical calculations.

Basic mathematical operations can be done with the following shards:

- [Math.Add](../../../reference/shards/Math/Add/) (Addition)

- [Math.Subtract](../../../reference/shards/Math/Subtract/) (Subtraction)

- [Math.Multiply](../../../reference/shards/Math/Multiply/) (Multiplication)

- [Math.Divide](../../../reference/shards/Math/Divide/) (Division)

Some shards encapsulate a few operations to help reduce code verbosity and promote simpler code. For example, [Math.Inc](../../../reference/shards/Math/Inc/) can be used to simply increase a variable's value by 1, removing the need to specify the operand and reassigning the new value back to the variable.

- [Math.Inc](../../../reference/shards/Math/Inc/) (Increase value by 1)

- [Math.Dec](../../../reference/shards/Math/Dec/) (Decrease value by 1)

- [Math.Lerp](../../../reference/shards/Math/Lerp/) (Linearly interpolate between two values)

More complex operations can also be executed with shards such as:

- [Math.Exp](../../../reference/shards/Math/Exp/) (Exponential)

- [Math.Mod](../../../reference/shards/Math/Mod/) (Modulus)

- [Math.Pow](../../../reference/shards/Math/Pow/) (Power)

- [Math.Sqrt](../../../reference/shards/Math/Sqrt/) (Square root)

## Strings

These shards allow you to work with strings and text in Shards.

- [String.Contains](../../../reference/shards/String/Contains/) (Searches for a string)

- [String.Join](../../../reference/shards/String/Join/) (Combines strings)

- [String.ToLower](../../../reference/shards/String/ToLower/) (Converts to lower case)

- [String.ToUpper](../../../reference/shards/String/ToUpper/) (Converts to upper case)

- [String.Trim](../../../reference/shards/String/Trim/) (Trims a string)

## Collections

Collections are used to group multiple items into a single element. There exists various shards that can be used to facilitate working with collections.

Shards to create collections:

- [Sequence](../../../reference/shards/General/Sequence/)

- [Table](../../../reference/shards/General/Table/)

Shards to work with collections:

- [AppendTo](../../../reference/shards/General/AppendTo/) (Append to a collection)

- [PrependTo](../../../reference/shards/General/PrependTo/) (Prepend to a collection)

- [Sort](../../../reference/shards/General/Sort/) (Sort a collection)

- [Take](../../../reference/shards/General/Take/) (Extract elements from a collection)

## Flow

Flow shards allow you to manipulate the flow of your Shards program.

Shards to run another Wire in-line:

- [Do](../../../reference/shards/General/Do/) (Flow stays on the new Wire if paused)

- [Step](../../../reference/shards/General/Step/) (Flow returns to original Wire if paused)

Shards to schedule another Wire:

- [Detach](../../../reference/shards/General/Detach/) (Schedules original Wire)

- [Spawn](../../../reference/shards/General/Spawn/) (Schedules a clone)

- [Branch](../../../reference/shards/General/Branch/) (Schedules Wires on a Submesh)

Shards to control the flow of Wires:

- [Start](../../../reference/shards/General/Start/)

- [Resume](../../../reference/shards/General/Resume/)

- [Stop](../../../reference/shards/General/Stop/)

Shards to run code in bulk:

- [Expand](../../../reference/shards/General/Expand/) (Creates copies of a Wire)

- [TryMany](../../../reference/shards/General/TryMany/) (Creates clones for each input to try)

For more information on Shards Flow, check out the primer guide [here](../../../learn/shards/the-flow/).

## Shaders

Shader shards allow you to write shader code and create material effects in your program.

Shards to aid shader writing: 

- [Shader.Literal](../../../reference/shards/Shader/Literal/)

- [Shader.ReadBuffer](../../../reference/shards/Shader/ReadBuffer/)

- [Shader.SampleTexture](../../../reference/shards/Shader/SampleTexture/)

- [Shader.WriteOutput](../../../reference/shards/Shader/WriteOutput/)

## Graphics

Graphics shards fall under the GFX shards family and are used to create and render objects to your program.

The base of the graphic shards:

- [GFX.Drawable](../../../reference/shards/GFX/Drawable/) (An object to be drawn)

- [GFX.View](../../../reference/shards/GFX/View/) (The camera view)

- [GFX.gITF](../../../reference/shards/GFX/gITF/) (Loads a model from a file)

Shards to set parameters to your drawables:

- [GFX.Texture](../../../reference/shards/GFX/Texture/) (Converts an image)

- [GFX.Material](../../../reference/shards/GFX/Material/) (Creates a reusable setting)

Shards to control what is or will be drawn:

- [GFX.DrawQueue](../../../reference/shards/GFX/DrawQueue/) (Records `Draw` commands)

- [GFX.ClearQueue](../../../reference/shards/GFX/ClearQueue/) (Clears the `Draw` queue)

- [GFX.Draw](../../../reference/shards/GFX/Draw/) (Adds `Drawable`s into the RenderQueue)

- [GFX.Feature](../../../reference/shards/GFX/Feature/) (Allows for customizable shaders)

## UI

UI shards allow you to customize and render User Interfaces in Shards.

Shards to define the segments in your interface:

- [UI.CentralPanel](../../../reference/shards/UI/CentralPanel/)

- [UI.TopPanel](../../../reference/shards/UI/TopPanel/)

- [UI.LeftPanel](../../../reference/shards/UI/LeftPanel/)

- [UI.RightPanel](../../../reference/shards/UI/RightPanel/)

- [UI.BottomPanel](../../../reference/shards/UI/BottomPanel/)

Shards to organize UI elements on your interface:

- [UI.Horizontal](../../../reference/shards/UI/Horizontal/) (Horizontal layout)

- [UI.Group](../../../reference/shards/UI/Group/) (Groups content together)

- [UI.Grid](../../../reference/shards/UI/Grid/) (Organizes content in a grid)

- [UI.Frame](../../../reference/shards/UI/Frame/) (Groups content in a frame)

- [UI.Indent](../../../reference/shards/UI/Indent/)

- [UI.Space](../../../reference/shards/UI/Space/) 

- [UI.Separator](../../../reference/shards/UI/Separator/) 

- [UI.ScrollArea](../../../reference/shards/UI/ScrollArea/)

- [UI.Scope](../../../reference/shards/UI/Scope/)

Shards to add UI elements to your interface:

- [UI.CodeEditor](../../../reference/shards/UI/CodeEditor/) (Code segment)

- [UI.Image](../../../reference/shards/UI/Image/)

- [UI.Hyperlink](../../../reference/shards/UI/Hyperlink/)

- [UI.Label](../../../reference/shards/UI/Label/) (Text)

- [UI.Link](../../../reference/shards/UI/Link/)

- [UI.MenuBar](../../../reference/shards/UI/MenuBar/)

- [UI.ProgressBar](../../../reference/shards/UI/ProgressBar/)

- [UI.Spinner](../../../reference/shards/UI/Spinner/) (Spinning loading widget)

Shards to capture User Input:

- [UI.Button](../../../reference/shards/UI/Button/)

- [UI.Checkbox](../../../reference/shards/UI/Checkbox/)

- [UI.ColorInput](../../../reference/shards/UI/ColorInput/) (Color picker)

- [UI.Combo](../../../reference/shards/UI/Combo/) (Drop-down menu)

- [UI.FloatSlider](../../../reference/shards/UI/FloatSlider/) (Numeric slider)

- [UI.FloatInput](../../../reference/shards/UI/FloatInput/) (Numeric input)

- [UI.RadioButton](../../../reference/shards/UI/RadioButton/)

Shards to draw graphs:

- [UI.Plot](../../../reference/shards/UI/Plot/)

- [UI.PlotBar](../../../reference/shards/UI/PlotBar/)

- [UI.PlotPoints](../../../reference/shards/UI/PlotPoints/)

To customize the User Interface:

- [UI.Style](../../../reference/shards/UI/Style/)

Additional UI controls:

- [UI.Reset](../../../reference/shards/UI/Reset/)

## Inputs

Inputs shards allow you to capture mouse and keyboard inputs and bind actions to them.

Keyboard Captures:

- [Inputs.KeyDown](../../../reference/shards/Inputs/KeyDown/)

- [Inputs.KeyUp](../../../reference/shards/Inputs/KeyUp/)

Mouse Captures:

- [Inputs.MouseDown](../../../reference/shards/Inputs/MouseDown/)

- [Inputs.MousePos](../../../reference/shards/Inputs/MousePos/)

- [Inputs.MouseUp](../../../reference/shards/Inputs/MouseUp/)

## Physics

Physics shards allow you to realize physics concepts in your Shards program.

Shards to simulate objects:

- [Physics.Ball](../../../reference/shards/Physics/Ball/)

- [Physics.Cuboid](../../../reference/shards/Physics/Cuboid/)

- [Physics.DynamicBody](../../../reference/shards/Physics/DynamicBody/)

- [Physics.KinematicBody](../../../reference/shards/Physics/KinematicBody/)

- [Physics.StaticBody](../../../reference/shards/Physics/StaticBody/)

Shards to simulate force:

- [Physics.Impulse](../../../reference/shards/Physics/Impulse/)

- [Physics.Simulation](../../../reference/shards/Physics/Simulation/)

Shards to aid Physics calculations:

- [Physics.CastRay](../../../reference/shards/Physics/CastRay/)

--8<-- "includes/license.md"
