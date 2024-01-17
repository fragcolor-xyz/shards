This function should be used only once per rendered frame.

It takes a [View](../View) or sequence of [Views](../View) that represent the main camera as a parameter.

??? info Default view
    If neither `View:` nor `Views:` is specified, the renderer will use a default view with both identity view and projection matrices.

The `Steps:` parameter contains the list of rendering operations to perform. This can be a sequence of objects created by one of the following:

- [DrawablePass](../DrawablePass)
- [EffectPass](../EffectPass)

## Default queue

If no render queue is specified, this command reads from a global default queue.
