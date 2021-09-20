---
authors: Fragcolor & contributors
license: CC-BY-SA-4.0
---


# GLTF.Draw

```clojure
(GLTF.Draw
  :Model [(Object) (ContextVar [(Object)])]
  :Materials [(None) (Table [(Table {"Shader" (Object)})]) (ContextVar [(Table [(Table {"Shader" (Object)})])]) (Table [(Table {"Shader" (Object) "Textures" (Seq [(Object)])})]) (ContextVar [(Table [(Table {"Shader" (Object) "Textures" (Seq [(Object)])})])]) (Table [(Table {"Textures" (Seq [(Object)])})]) (ContextVar [(Table [(Table {"Textures" (Seq [(Object)])})])])]
  :Controller [(None) (Chain)]
)
```


## Definition




## Parameters

| Name | Type | Default | Description |
|------|------|---------|-------------|
| Model | `[(Object) (ContextVar [(Object)])]` | `None` | The GLTF model to render. |
| Materials | `[(None) (Table [(Table {"Shader" (Object)})]) (ContextVar [(Table [(Table {"Shader" (Object)})])]) (Table [(Table {"Shader" (Object) "Textures" (Seq [(Object)])})]) (ContextVar [(Table [(Table {"Shader" (Object) "Textures" (Seq [(Object)])})])]) (Table [(Table {"Textures" (Seq [(Object)])})]) (ContextVar [(Table [(Table {"Textures" (Seq [(Object)])})])])]` | `None` | The materials override table, to override the default PBR metallic-roughness by primitive material name. The table must be like {Material-Name <name> {Shader <shader> Textures [<texture>]}} - Textures can be omitted. |
| Controller | `[(None) (Chain)]` | `None` | The animation controller chain to use. Requires a skinned model. Will clone and run a copy of the chain. |


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


--8<-- "includes/license.md"
