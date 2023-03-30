!!! note
    These styles are based on `egui` styling, which is the library used for the UI.

### Text

Built-in text styles: **Small**, **Body**, **Monospace**, **Button** and **Heading**. In general, **Body** is the one you want to tweak.

Custom text styles can be defined, in which case `override_text_style` must be set. For example:

=== "Code"
    ```clj linenums="1"
    (def my-style
    {:override_text_style "TextStyle1"
    :text_styles
    [{:name "TextStyle1"
        :size 24
        :family "Proportional"}
    {:name "TextStyle2"
        :size 16
        :family "Monospace"}]})
    ```

!!! note
    To change the color of the text, use `override_text_color` from the `visuals`section.

By default, the wrapping behavior for text depends on the widget, in which case the value of `wrap` is `nil`. To enable wrapping anywhere, change it to `true`.


### Spacing
<!-- TODO: add samples for each (code and reference image) -->

!!! info
    The `spacing` section defines default sizes as well as invisible space between widgets (margins).

`item_spacing`: extra space between widgets.

`window_margin`: horizontal and vertical margin within a window frame.

`button_padding`: space around the text of the button.

`menu_margin`: horizontal and vertical margin within a menu frame.

`indent`: indent collapsing headers by this amount.

`interact_size`: ??

`slider_width`: default width of a slider.

`combo_width`: default width of a combo box.

`text_edit_width`: default width of a text field.

`icon_width`: width/height of the outer part of the icon for collapsing headers, radio buttons and checkboxes.

`icon_width_inner`: width/height of the inner part of the icon for collapsing headers, radio buttons and checkboxes.

`icon_spacing`: space between that icon and the text (if any)

`tooltip_width`: width of a tooltip.

`indent_ends_with_horizontal`: ??

`combo_height`: height of a combo before displaying scroll bars.

`scroll_bar_width`: ??

`scroll_handle_min_length`: ??

`scroll_bar_inner_margin`: ??

`scroll_bar_outer_margin`: ??


### Visuals
<!-- TODO: add samples for each (code and reference image) -->

To change the text color, use `override_text_color`:

=== "Code"
    ```clj linenums="1"
    (def my-style
    {:visuals
     {:override_text_color (color 64 250 0)}})
    ```


#### Widgets
<!-- TODO: add samples for each (code and reference image) -->

There are 5 states/kinds for widgets: `noninterative`, `inactive`, `hovered`, `active` and `open`. Each have the same properties:

`bg_fill`: background color of widgets that must have a background fill.

`weak_bg_fill`: background color of widgets that can *optionally* have a background fill (e.g. buttons).

`bg_stroke`: surrounding rectangle color.

`rounding`: radius of angles of that rectangle.

`fg_stroke`: stroke and text color inside the widget.

`expansion`: ??


#### Selection
<!-- TODO: add samples for each (code and reference image) -->


### Interactions
<!-- TODO: add samples for each (code and reference image) -->


### Debug
<!-- TODO: add samples for each (code and reference image) -->

??? note "All styles"
    ```clj linenums="1"
    --8<-- "details/shards/UI/all_styles.edn"
    ```
