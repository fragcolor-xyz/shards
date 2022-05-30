The `:Label` parameter is useful for conveying more information to the user about a displayed text string. For example, you may want to give labels to input fields of a UI form. This parameter takes the intended label-name as a string value argument and prints this label to the right of the displayed text string. 

The `:Color` parameter can be used to control the color of the displayed text. The argument to this parameter is the function `color` which represents the required color in terms of its RGBA values. If this parameter is not used, the default text color is white on a black GUI background.

The `:Format` parameter is useful when you need to display the input string as part of a larger text string. The argument to this parameter is the larger text string which should contain a placeholder i.e. `{}` where you want the input string to display. This parameter then replaces the `{}` with the input string and returns the composite string as output to display.
