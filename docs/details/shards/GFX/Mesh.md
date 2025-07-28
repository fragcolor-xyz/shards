The sequence of strings given to the `Layout` parameter will be used as the pattern to interpret the sequence value in the Vertices key of the input table.

Consider the following example:
```shards
{
    "Vertices": [
      @f3(-1.0 1.0 1.0) @color(0x000000)
      @f3(1.0 1.0 1.0) @color(0x0000ff)
      @f3(-1.0 -1.0 1.0) @color(0x00ff00)
      @f3(1.0 -1.0 1.0) @color(0x00ffff)
      @f3(-1.0 1.0 -1.0) @color(0xff0000)
      @f3(1.0 1.0 -1.0) @color(0xff00ff)
      @f3(-1.0 -1.0 -1.0) @color(0xffff00)
      @f3(1.0 -1.0 -1.0) @color(0xffffff)]
    "Indices": [
      0 1 2
      1 3 2
      4 6 5
      5 6 7
      0 2 4
      4 2 6
      1 5 3
      5 7 3
      0 4 1
      4 5 1
      2 3 6
      6 3 7
    ]
  }
  GFX.Mesh(Layout: ["position" "color"] WindingOrder: WindingOrder::CW) = mesh
```
The sequence in the `Layout` parameter will make it such that each pair of elements in the sequence represent a vertex, with the first value in the pair representing the vertex position and the second value represents the vertex colour. Thus element 1 and element 2 in the sequence repesent the first vertex, element 3 and element 4 represent the second vertex and so on and so forth.

Any string can be added to the sequence of string in the `Layout` parameter, however the following strings already have a set definition and will be interpreted accordingly:
  - "position" : Vertex position (usually 2D or 3D)
  - "color" : Vertex color
  - "normal" : Vertex normal (for lighting calculations)
  - "texCoord0" , "texCoord1" , "texCoord2" etc. : Texture coordinates
  - "tangent" : Tangent vector (for advanced lighting techniques)

The `Shader.ReadInput` shard can be used to read the values assigned to the strings specified in the `Layout` parameter. `Shader.ReadInput(Name: "texCoord0")` for example will read the texture coordinates of the current vertex set to texCoord0.

The sequence of indices in the `Indices` key of the input table will be used to draw the triangles that will make up the mesh, with each set of three indices representing a single triangle.

Consider the following example:
```shards
{
    "Vertices": [
      @f3(-1.0 1.0 1.0) @color(0x000000)
      @f3(1.0 1.0 1.0) @color(0x0000ff)
      @f3(-1.0 -1.0 1.0) @color(0x00ff00)
      @f3(1.0 -1.0 1.0) @color(0x00ffff)
      @f3(-1.0 1.0 -1.0) @color(0xff0000)
      @f3(1.0 1.0 -1.0) @color(0xff00ff)
      @f3(-1.0 -1.0 -1.0) @color(0xffff00)
      @f3(1.0 -1.0 -1.0) @color(0xffffff)]
    "Indices": [
      0 1 2
      1 3 2
      4 6 5
      5 6 7
      0 2 4
      4 2 6
      1 5 3
      5 7 3
      0 4 1
      4 5 1
      2 3 6
      6 3 7
    ]
  }
  GFX.Mesh(Layout: ["position" "color"] WindingOrder: WindingOrder::CW) = mesh
```
In this example, the vertex with index 0, 1 and 2 will form the first triangle, the vertex with index 1, 3 and 2 will form the second triangle and so on and so forth.

The `WindingOrder` can either be `WindingOrder::CW` or `WindingOrder::CCW`. This will determine the orientation of the triangles that will be created from the indices.

Consider the following example:
```shards
{
    "Vertices": [
      @f3(-1.0 1.0 1.0) @color(0x000000)
      @f3(1.0 1.0 1.0) @color(0x0000ff)
      @f3(-1.0 -1.0 1.0) @color(0x00ff00)
      @f3(1.0 -1.0 1.0) @color(0x00ffff)
      @f3(-1.0 1.0 -1.0) @color(0xff0000)
      @f3(1.0 1.0 -1.0) @color(0xff00ff)
      @f3(-1.0 -1.0 -1.0) @color(0xffff00)
      @f3(1.0 -1.0 -1.0) @color(0xffffff)]
    "Indices": [
      0 1 2
      1 3 2
      4 6 5
      5 6 7
      0 2 4
      4 2 6
      1 5 3
      5 7 3
      0 4 1
      4 5 1
      2 3 6
      6 3 7
    ]
  }
  GFX.Mesh(Layout: ["position" "color"] WindingOrder: WindingOrder::CW) = mesh
```
In this example, since `WindingOrder` is set to `WindingOrder::CW`, the vertices are interpreted in clockwise order when viewed from the front of the triangle.
- Vertex 0: (-1.0, 1.0, 1.0)
- Vertex 1: ( 1.0, 1.0, 1.0)
- Vertex 2: (-1.0, -1.0, 1.0)
In this case, if you were looking at the front face of the triangle, you would see the vertices arranged clockwise: 0 -> 1 -> 2.
The normal of this triangle would point towards the viewer (out of the screen).

If `WindingOrder` is set to `WindingOrder::CCW`, the vertices are interpreted in counter-clockwise order when viewed from the front of the triangle.
- Vertex 0: (-1.0, 1.0, 1.0)
- Vertex 1: ( 1.0, 1.0, 1.0)
- Vertex 2: (-1.0, -1.0, 1.0)
However, now this order (0 -> 1 -> 2) is considered to be the back face of the triangle. The front face would be the reverse order: 2 -> 1 -> 0.
The normal of this triangle would point away from the viewer (into the screen).

The mesh object output created by this shard can be passed to the `Mesh` parameter of the `GFX.DrawablePass` shard to be added to the render pipeline and subsequently rendered.