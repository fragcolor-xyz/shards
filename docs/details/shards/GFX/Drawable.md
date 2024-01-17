The Drawable shard defines an instance of a drawn object. It usually has a transform (in world space) and a mesh to draw.

--8<-- "includes/drawable_refactor.md"

## Params

The `Params` parameter contains a table of values that will be passed to the shader when this drawable is rendered.

!!! info Defining shader parameters
    See [GFX.Feature](../Feature) about where shader parameters are defined and used.

## Constant/Dynamic

Shader parameters and the drawable transform can be set as either constant parameters or dynamic ones.

### Constant parameters

Constant parameters are passed through the input table. They are read and stay the same until this function is called again.

```clojure
{Mesh: mesh Transform: transform} | GFX.Drawable >= my-drawable
```

### Dynamic parameters

Dynamic parameters are passed as parameters to the shard. You can set the same fields as you can when setting constant parameters, with the exception of the mesh. The variables are referenced by the `Drawable` so that changes in their value will be reflect in the rendered result.

```clojure
.. >= dynamic-color
.. >= dynamic-transform
{Mesh: mesh} | Drawable(Transform: dynamic-transform Params: {baseColor: dynamic-color}) >= my-drawable
```

## Intended usage

To avoid re-creating Drawables for objects with minor or no changes, you should set up Drawables from within a [Setup](../../General/Once) block and pass their dynamic parameters to the shard.

```clojure
; Only done once
Setup(
  ... = const-transform
  @f4(1.0) >= dynamic-color
  {Mesh: mesh Transform: const-transform} | Drawable(Params: {baseColor: dynamic-color}) >= my-drawable)

; Update color every time this wire runs
... > dynamic-color
```
