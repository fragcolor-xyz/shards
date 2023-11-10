# UI.MarkdownViewer

| Name | -  | Description | Default | Type |
|------|---------------------|-------------|---------|------|
| `<input>` ||The markdown text to render. | | `String` |
| `<output>` ||The output of this shard will be its input. | | `String` |

**Code**

```clj
@wire(main-wire {
  GFX.MainWindow(
    Contents: {
      Once({
        GFX.DrawQueue >= ui-draw-queue
        GFX.UIPass(ui-draw-queue) >> render-steps
      })
      UI(
        ui-draw-queue
        UI.TopPanel(
          Contents: {
            "# Title

## Sub-title
**list:**
- item 1
- item 2" | UI.MarkdownViewer
          }
        )
      )

      GFX.Render(Steps: render-steps)
    }
  )
} Looped: true)

@mesh(root)
@schedule(root main-wire)
@run(root 0.1 10)
```