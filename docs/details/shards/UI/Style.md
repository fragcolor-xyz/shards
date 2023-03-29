### Text

Built-in text styles: **Small**, **Body**, **Monospace**, **Button** and **Heading**.

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

### Spacing

### Visuals

To change the text color, use `override_text_color`:

=== "Code"
    ```clj linenums="1"
    (def my-style
    {:visuals
     {:override_text_color (color 64 250 0)}})
    ```

#### Widgets

### Debug

??? note "All styles"
    ```clj linenums="1"
    --8<-- "details/shards/UI/all_styles.edn"
    ```
