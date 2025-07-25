Loads a glTF model from a file. Outputs a `GFX.DrawableHierarchy` that can be passed to [Draw](../Draw).

--8<-- "includes/drawable_refactor.md"

## Static file

Loaded when the Wire containing it is warmed up.

```clojure
{transform: ...} | GFX.glTF(Path: "pathToModel.glb") >= drawable
```

## Dynamic file

Loaded when activated.
You should cache the result inside a [Setup](../../General/Once) shard.

```clojure
{transform ... path: path} | GFX.glTF >= drawable
```

## Raw bytes

Loaded when activated.
You should cache the result inside a [Setup](../../General/Once) shard.

```clojure
{transform: ... bytes: gltf-data} | GFX.glTF >= drawable
```

## Duplicate existing model

--8<-- "includes/experimental.md"

Generate another `GFX.DrawableHierarchy` from an existing `GFX.DrawableHierarchy`

```clojure
... GFX.glTF(...) >= other-gltf-drawable
{transform:... copy: other-gltf-drawable} | GFX.glTF >= drawable
```

