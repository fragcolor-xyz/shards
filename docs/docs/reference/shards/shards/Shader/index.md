# Shaders

Shaders are written using a subset of the regular shards language, in addition to some of the shader specific shards listed here.

## Supported operations

### Variables

```shards
0.0 >= float-var // Set
3.14 > float-var // Update
1.0 = const-var // Constant
```

### Vector types

```shards
@f3(0.0) >= v3-zero
@f3(1.0 0.0 0.0) >= v3-forward
```

### Vector swizzles

```shards
v3-forward Take(0) >= x
v3-forward Take([2 1 0]) >= v3-reversed
v3-forward Take([2 1]) >= v2-zy
```

### Sub flow

```shards
v3-forward
{Take(0) >= x}
{Take(1) >= y}
{Take(2) >= z}
```

### Push (for matrix types only)

Push works for building matrix types out of vectors, but with the constraint that you can't push from within branching control flows.

```shards
@f4(1.0 0.0 0.0 0.0) >> mat
@f4(0.0 1.0 0.0 0.0) >> mat
@f4(0.0 0.0 1.0 0.0) >> mat
@f4(0.0 0.0 0.0 1.0) >> mat
mat // this evaluates to a float4x4 when used in expressions
```

### Scalar an vector maths

```shards
@f3(...) >= v3

4.0 | Math.Cos >= cos-4
cos-4 | Math.Sin >= cos-sin-4
Math.Floor >= floored-result

v3 | Math.Multiply(2.0) >= v3-multiplied
v3 | Math.Add(1.0) >= v3-plus-one
v3 | Math.Add(@f3(1.0 -1.0 0.0)) >= v3-adjusted

v3 | Math.Normalize >= v3-normalized
v3 | Mathlength >= length
v3 | Math.Dot(v3) >= dot-product
v3 | Math.Cross(v3) >= v3-cross
```

### Number conversions

```shards
3.3 | ToInt >= int-var // (3)
3.3 | ToFloat4 >= v4-var // @f4(3.3 0.0 0.0 0.0)

@f3(1.0 2.0 3.0) | ToFloat >= float-var // (1.0)

@f3(0.0 float-var float-var) >= v3 // (0.0 1.0 1.0)
```

### Vector contructors

```shards
1.0 = float-var
@f3(0.0 float-var float-var) >= v3 // (0.0 1.0 1.0)
@f4(float-var 0.0 4.0 float-var) >= v4 // (1.0 0.0 4.0 1.0)
```

### Matrix multiplication

```shards
@f3(...) >> mat-0
@f3(...) >> mat-0
...
@f3(...) >> mat-1
@f3(...) >> mat-1
...
@f3(...) >= v3

// Matrix * Matrix = Matrix
mat-0 | (MathmatMulmat-1) >= mat-2

// Matrix * Vector = Vector
mat-2 | (MathmatMulv3) >= v3-transformed
```

### Comparisons

```shards
3.4 | IsMoreEqual(3.0) >= bool // (true)
3.4 | IsLess(3.0) > bool // (false)
```

### Branches

```shards
3.4 = v
v | If( Predicate: IsLess(3.0)
      Then: {...}
      Else: {...})

v | When(
      Predicate:IsLess(3.0)
      Action: {...})

v | When( 
      Predicate: {IsLess(3.0) (And) (IsGreater 1.0)}
      Action: {...})
```

### Combinational logic

```shards
3.4 = v
v When(
      Predicate: {IsLess(3.0) (And) (IsGreater 1.0)}
      Action: {...})
v When(
      Predicate: {IsLess(3.0) (Or) (IsGreater 1.0)}
      Action: {...})
```

### For range loop

```shards
ForRange(
  From: 0 
  To: 32 
  Action: {
  = i
  ...
  })
```

### Function-like wire evaluation

Pure Wires are supported. They can be run using `Do` and passing an input as a scalar/vector/matrix type. Their output is returned.

```shards
@wire( rotation-matrix {
  >= input
  input | Math.Cos >= c
  input | Math.Sin
  {>= s}
  {Math.Negate >= neg-s}
  @f2(c s) >> result
  @f2(neg-s c) >> result
  result // Output a 2x2 matrix
} Looped: false Pure: true)

0.45 | Do(rotation-matrix) >= mat2-2 // This will contain the result from the wire

```

Non-pure Wires are also supported. They will accept an input value and return their output.
In addition they will also capture any variables referenced that are defined in parent Wires.

```shards
@wire(sub-logic{
  global | Math.Cos >= v
  2.0 | Math.Add(v)
} Looped: false)

3.0 | Set(Name: global Global: true)
Do(sub-logic) >= v1
2.0 > global
Do(sub-logic) >= v2
```

!!! info "Partial support"

    Currently, only values that are read from parent Wires are supported. Trying to update a variable defined in a parent Wire will have no effect on that variable.

--8<-- "includes/license.md"
