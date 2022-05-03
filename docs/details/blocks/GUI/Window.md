A `GUI.Window` is a graphical user-interface window that can be used to display contents to, as well as to recieve inputs from, the user. Multiple `GUI.Window`s can be rendered within the `GFX.MainWindow` container.

### `:Title` Text ###

The `:Title` parameter value displays as text on the title-bar of the rendered window and hence can identify the window. However, for this to work, the title-bar display must not be suppressed (i.e. the flag `GuiWindowFlags.NoTitleBar` should not be passed to the `:Flag` parameter).

### `:Pos`, `:Height` and `:Width` ###
The `:Pos` parameter defines the 2D coordinates of the top-left corner of the rendered window, whereas the `:Width` and `:Height` parameters define the rendered window's dimensions. 

The area available to the rendered window is bounded by the container window. Hence if `w` and `h` are the width and height of the container window (in pixels), the rendered window should lie within these four vertices: `(0 0)`, `(0 h)`, `(w 0)`, and `(h w)`.

If any part of the rendered window is outside this area, that part will not be visible.

Integer inputs to these parameters are interpreted directly as pixel value coordinates (for position), or as pixel value of dimensions (for height and width), of the rendered window. 

Float inputs, however, must lie between values 0 and 1 (inclusive) and are multiplied with the container window's height and width pixel values to get the rendered window's height. width and position.

For example, let a container window have a `height` of 800 pixels and a `width` of 200 pixels.

#### `:Pos` ####
Now `:Pos (int2 200 100)` and `:Pos (float2 0.25 0.5)` will be equivalent as they both evaluate to `(10 10)` for the pixel coordinates of the top left corner of the rendered window (0.25 * 800 = 200; 0.5 * 200 = 100).

Also, `:Pos (int2 400 100)` and `:Pos (float2 0.5 0.5)`, will both compute the rendered window's top left corner to be in the center of the container window i.e. `(400 100)` (0.5 * 800 = 400, 0.5 * 200 = 100).

#### `:Height` and `:Width` ####
Similarly `:Height (int 160)` and `:Height (float 0.2)` will compute a height of 160 pixels for the rendered window (0.2 * 800 = 160). In the same way `:Width (int 150)` and `:Width (float 0.75)` will both compute the width of the rendered window as 150 pixels (0.75 * 200 = 150).

### Displayable `:Contents` ###
The input to the `:Contents` parameter is code (in form of blocks) that generates and controls UI elements (like text string, buttons, checkboxes etc.) that will get displayed in the rendered window. So while other parameters mostly define the attributes of the rendered window, `:Contents` controls the core user interaction.

### Attribute `:Flags` ###
The `:Flags` parameter defines some attributes and behavior of the rendered window. Passing a flag indicator to the `:Flags` parameter will enable that behavior or attribute, whereas not passing it will result in the default behavior. 

This is a list of valid flag indicators and the behaviors they control.

- `GuiWindowFlags.Menubar`: show menu-bar (default is hide)
- `GuiWindowFlags.NoTitleBar`: hide title-bar (default is show)
- `GuiWindowFlags.NoResize`: prevent window resizing (default is allow)
- `GuiWindowFlags.NoMove`: prevent window movement (default is allow)
- `GuiWindowFlags.NoCollapse`: prevent window collapse (default is allow)

### `:OnClose` Flag ###
The `:OnClose` parameter is a nifty way to control the visibility and state of the close [x] button (displayed on the title-bar of the rendered window). This parameter requires a boolean variable as input. The close [x] button is displayed If the variable is `true` and hidden if the variable is `false`. If this close [x] is clicked then window is closed and the state of the `:OnClose` parameter's variable is changed from `true` to `false`. Hence this variable acts as a state indicator for the rendered window (i.e. whether a given window is currently open or closed).