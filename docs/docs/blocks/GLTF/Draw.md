# GLTF.Draw

```clojure
(GLTF.Draw
  :Model [(Object) (ContextVar [(Object)])]
  :Materials [(None) (Table [(Table {"Shader" (Object)})]) (ContextVar [(Table [(Table {"Shader" (Object)})])]) (Table [(Table {"Shader" (Object) "Textures" (Seq [(Object)])})]) (ContextVar [(Table [(Table {"Shader" (Object) "Textures" (Seq [(Object)])})])]) (Table [(Table {"Textures" (Seq [(Object)])})]) (ContextVar [(Table [(Table {"Textures" (Seq [(Object)])})])])]
)
```

## Definition


## Parameters
| Name | Type | Default | Description |
|------|------|---------|-------------|
| Model | `[(Object) (ContextVar [(Object)])]` |  | The GLTF model to render. |
| Materials | `[(None) (Table [(Table {"Shader" (Object)})]) (ContextVar [(Table [(Table {"Shader" (Object)})])]) (Table [(Table {"Shader" (Object) "Textures" (Seq [(Object)])})]) (ContextVar [(Table [(Table {"Shader" (Object) "Textures" (Seq [(Object)])})])]) (Table [(Table {"Textures" (Seq [(Object)])})]) (ContextVar [(Table [(Table {"Textures" (Seq [(Object)])})])])]` |  | The materials override table, to override the default PBR metallic-roughness by primitive material name. The table must be like {Material-Name <name> {Shader <shader> Textures [<texture>]}} - Textures can be omitted. |


## Input
| Type | Description |
|------|-------------|
| `[(Seq [(Float4)]) (Seq [(Seq [(Float4)])])]` |  |


## Output
| Type | Description |
|------|-------------|
| `[(Seq [(Float4)]) (Seq [(Seq [(Float4)])])]` |  |


## Examples

```clojure
(GLTF.Draw

)
```
