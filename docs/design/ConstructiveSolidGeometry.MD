# Constructive Solid Geometry

[Constructive Solid Geometry](https://en.wikipedia.org/wiki/Constructive_solid_geometry) (CSG) allows the modeler to create
complex shapes using boolean operators to combine simpler
solid objects.

![](https://upload.wikimedia.org/wikipedia/commons/thumb/8/8b/Csg_tree.png/450px-Csg_tree.png)

In Shards we employ a [Signed Distance Function](https://en.wikipedia.org/wiki/Signed_distance_function) (SDF) to generate implicit 3D shapes such as spheres, cubes, capsules, torii etc. which can be traced by ray-marching through the scene.

We'll explore in depth the binary tree representation, how to flatten it to a list, how the shader consumes it and what's the math behind it.

## The Tree

Each node in the tree is either a simple shape (a leaf node) or an operator that takes the result of one or more child nodes (operands) and combines them. For simplicity we consider unary and binary operators only. Operations on three or more elements can be split into binary operations by nesting some of the elements deeper in the tree. In the example above the right node is an union of three cylinders, but it has been split into two binary union operators.

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

### Examples of some of the codes

`OpCodeSphere` - the current node should trace a sphere using `sdSphere` and put the distance on the stack.

`AuxCodeOpData` - the auxiliary data is operator data. Operator data is different per OpCode. For example, the `OpCodeSphere` requires a radius for the sphere it traces. If OpData is available, it gets it from GetOpData().x;

`AuxCodeRotation` - the auxiliary data is rotation data for the node, given as a quaternion. See the Math section for details.

## The Math

Location: `//.../Shaders/Math.h`

There are two important concepts in the math library - the use of quaternions for rotation, and the order of operations when traversing from parent node to child node.

### Quaternions

[Quaternions and spatial rotation](https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation) - A spatial rotation of θ radians about a unit axis `(ux, uy, uz)` is mathematically given by `q = (cos(θ/2), ux * sin(θ/2), uy * sin(θ/2), uz * sin(θ/2))`. In the shader we choose to set the real component to `.w` and the imaginary components i, j and k to `.x`, `.y` and `.z` respectively . This means normalizing the  `.xyz` tuple gives you the rotation axis `(ux, uy, uz)`.

To rotate a vector p (or a fixed point p) around the rotation axis, we need to evaluate the conjugation of p by q:
```
p(rotated) = q p q^(-1)
```

### Node inheritance

Nodes define a position, an uniform scale and optionally a rotation quaternion. If they don't define a rotation, we use `(0, 0, 0, 1)` instead which is a 0 radian rotation.

When we need to modify a point or a child node local to the current node, we 1. Scale, 2. Rotate and 3. Translate it. We can swap the scale and rotate operations because we use uniform scaling. Non-uniform scaling requires scale to be applied first.

```
new Pos = Rotate(Pos(child), Rot(parent)) * Scl(parent) + Pos(parent)
new Rot = Rot(parent) * Rot(child)
new Scl = Scl(parent) * Scl(child)
```

In the above assignment the order of multiplication for the rotation matters as it involves multiplication of their real and three imaginary components. Refer to the Wikipedia article for details.

It's an arbitrary decision if we should resolve the child node's position before mapping the data and feeding it to the shader or if should handle the math in the shader. For performance it's better to resolve the location before mapping it, so we can reduce the workload on the shader. This way the shader can assume every node's location is given with the base node as a reference, not its immediate parent as a reference.
