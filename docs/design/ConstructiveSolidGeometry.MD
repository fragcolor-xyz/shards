# Constructive Solid Geometry

[Constructive Solid Geometry](https://en.wikipedia.org/wiki/Constructive_solid_geometry) (CSG) allows the modeler to create
complex shapes using boolean operators to combine simpler
solid objects.

![](https://upload.wikimedia.org/wikipedia/commons/thumb/8/8b/Csg_tree.png/450px-Csg_tree.png)

In Shards we employ a [Signed Distance Function](https://en.wikipedia.org/wiki/Signed_distance_function) (SDF) to generate implicit 3D shapes such as spheres, cubes, capsules, torii etc. which can be traced by ray-marching through the scene.

We'll explore in depth the binary tree representation, how to flatten it to a list, how the shader consumes it and what's the math behind it.

## The Tree

Each node in the tree is either a simple shape (a leaf node) or an operator, which takes the result of one or more child nodes (operands) and combines it. For simplicity we consider unary and binary operators only. Operations on three or more elements can be split into binary operations by nesting some of the elements deeper in the tree. In the example above the right node is an union of three cylinders, but it has been split into two binary union operators.

### Operators

`Union` - combines two shapes, A and B, so that each point in the resulting shapes belongs to either A or B, or both. The two shapes do not need to touch. The resulting shape will visually appear to be two shapes, A and B, even though mathematically we treat it as one. In the example above the three green cylinders form an union.

`Intersection` - visually the part of A which intersects (lies within) B and vice versa. The intersected shapes is contained in *both* A and B. If the two basic shapes do not intersect (do not touch each other), the result is an empty shape. In the example above the red and blue shapes are intersected.

`Subtraction` - visually subtracts (carves out) B from A. If the shapes do not touch, there is nothing to subtract so the result is the same as A. If B fully contains A, subtracting it will remove A entirely leaving just an empty shape. In the example above, the green part is subtracted from the red-blue intersection.

## The List

In order to traverse the tree easily we flatten it to a list and iterate over the list linearly.

1. Make sure we are working with a binary tree. If a node unifies or intersects three or more shapes, the node should be split to a binary union or intersection of the first shape and a new operator node (union or intersection) of the rest of the shapes. Repeat until all operators are binary.

2. Traverse the tree depth-first. Add each node on *leaving* the node. In the example above:
 - Enter the root (subtraction) node.
 - Enter the left (intersection) node.
 - Enter the red cube. It's a leaf node so we leave it immediately and add it to the list (1. Red cube)
 - Enter the blue sphere. It's a leaf node so we leave it immediately and add it to the list (2. Blue sphere)
 - Leave the intersection node and add it to the list (3. Intersect)
 - Enter the right (union) node.
 - Enter the left green cylinder. It's a leaf node so we leave it immediately and add it to the list (4. Green cylinder)
 - Enter the second right (union) node.
 - Enter the left green cylinder. It's a leaf node so we leave it immediately and add it to the list (5. Green cylinder)
 - Enter the right green cylinder. It's a leaf node so we leave it immediately and add it to the list (6. Green cylinder)
 - Leave the second right (union) node and add it to the list (7. Union)
 - Leave the first right (union) node and add it to the list (8. Union)
 - Leave the root (subtraction) node and add it to the list (9. Subtraction)

The list now looks like this:
```
1. Red cube
2. Blue sphere
3. Intersect
4. Green cylinder
5. Green cylinder
6. Green cylinder
7. Union
8. Union
9. Subtraction
```
If we treat it as a Reverse Polish Notation we can iterate it using a simple stack.

```
1. -> [1. RC]
2. -> [1. RC, 2. BS]
3. -> [1. RC * BS]
4. -> [1. RC * BS, 2. GC]
5. -> [1. RC * BS, 2. GC, 3. GC]
6. -> [1. RC * BS, 2. GC, 3. GC, 4. GC]
7. -> [1. RC * BS, 2. GC, 3. GC + GC]
8. -> [1. RC * BS, 2. GC + GC + GC]
9. -> [1. RC * BS - (GC + GC + GC)]
```

## The Shader

We will use the following structure to represent a flattened Node in a list:
```
struct NodeCommand
{
    float4  m_PosSize;    // Position in 3D (xyz) and a uniform scale
    float4  m_AuxData1;   // Auxiliary data
    float4  m_AuxData2;   // Auxiliary data
    uint4   m_Control;    // OpCode, AuxCode1, AuxCode2, ControlCode
}
```

To keep nodes uniform, they are all the same size (64 bytes = float4x4) where the first entry is always the node's position and size.

`m_PosSize.xyz` - position in 3D.

`m_PosSize.w` - uniform scale. Non-uniform scaling can be added either

`m_AuxData1` - auxiliary data for the command. What data is encoded here is specified by the `AuxCode1` code (`m_Control.y`)

`m_AuxData2` - auxiliary data for the command. What data is encoded here is specified by the `AuxCode2` code (`m_Control.z`). Both auxiliary codes use the same list of codes and it's up to the user to decide which slot to use, but if both specify the same data it's undefined which one should be used (it's up to the implementation).

`OpCode` (`m_Control.x`) - defines what the node does.

`ControlCode` (`m_Control.w`) - control flags used by the shader.

TODO Examples of some of the codes

## The Math

TODO

Location: `//.../Shaders/Math.h`


