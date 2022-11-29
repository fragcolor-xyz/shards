# Shaders

Shaders are written using a subset of the regular shards language, in addition to some of the shader specific shards listed here.

## Supported operations

### Variables

```clojure
0.0 >= .float-var ; Set
3.14 > .float-var ; Update
1.0 = .const-var ; Constant
```

### Vector types

```clojure
(Float3 0.0) >= .v3-zero
(Float3 1.0 0.0 0.0) >= .v3-forward
```

### Vector swizzles

```clojure
.v3-forward (Take 0) >= .x
.v3-forward (Take [2 1 0]) >= .v3-reversed
.v3-forward (Take [2 1]) >= .v2-zy
```

### Sub flow

```clojure
.v3-forward
(| (Take 0) >= .x)
(| (Take 1) >= .y)
(| (Take 2) >= .z)
```

### Push (for matrix types only)

Push works for building matrix types out of vectors, but with the constraint that you can't push from within branching control flows.

```clojure
(Float4 1.0 0.0 0.0 0.0) >> .mat
(Float4 0.0 1.0 0.0 0.0) >> .mat
(Float4 0.0 0.0 1.0 0.0) >> .mat
(Float4 0.0 0.0 0.0 1.0) >> .mat
.mat ; this evaluates to a float4x4 when used in expressions
```

### Scalar an vector maths

```clojure
(Float3 ...) >= .v3

4.0 (Math.Cos) >= .cos-4
.cos-4 (Math.Sin) >= .cos-sin-4
(Math.Floor) >= .floored-result

.v3 (Math.Multiply 2.0) >=. v3-multiplied
.v3 (Math.Add 1.0) >=. v3-plus-one
.v3 (Math.Add (Float3 1.0 -1.0 0.0)) >=. v3-adjusted

.v3 (Math.Normalize) >= .v3-normalized
.v3 (Math.Length) >= .length
.v3 (Math.Dot .v3) >= .dot-product
.v3 (Math.Cross .v3) >= .v3-cross
```

### Number conversions

```clojure
3.3 (ToInt) >= .int-var ; (3)
3.3 (ToFloat4) >= .v4-var ; (Float4 3.3 0.0 0.0 0.0)

(Float3 1.0 2.0 3.0) (ToFloat) >= .float-var ; (1.0)

(Float3 0.0 .float-var .float-var) >= .v3 ; (0.0 1.0 1.0)
```

### Vector contructors

```clojure
1.0 = .float-var
(Float3 0.0 .float-var .float-var) >= .v3 ; (0.0 1.0 1.0)
(Float4 .float-var 0.0 4.0 .float-var) >= .v4 ; (1.0 0.0 4.0 1.0)
```

### Matrix multiplication

```clojure
(Float3 ...) >> .mat-0
(Float3 ...) >> .mat-0
...
(Float3 ...) >> .mat-1
(Float3 ...) >> .mat-1
...
(Float3 ...) >= .v3

; Matrix * Matrix = Matrix
.mat-0 (Math.MatMul .mat-1) >= .mat-2

; Matrix * Vector = Vector
.mat-2 (Math.MatMul .v3) >= .v3-transformed
```

### Comparisons

```clojure
3.4 (IsMoreEqual 3.0) >= .bool ; (true)
3.4 (IsLess 3.0) > .bool ; (false)
```

### Branches

```clojure
3.4 = .v
.v (If (IsLess 3.0)
      :Then (-> ...)
      :Else (-> ...))

.v (When (IsLess 3.0)
      (-> ...))

.v (When (-> (IsLess 3.0) (And) (IsGreater 1.0))
      (-> ...))
```

### Combinational logic

```clojure
3.4 = .v
.v (When (-> (IsLess 3.0) (And) (IsGreater 1.0))
      (-> ...))
.v (When (-> (IsLess 3.0) (Or) (IsGreater 1.0))
      (-> ...))
```

### For range loop

```clojure
(ForRange 0 32 (->
  = .i
  ...
  ))
```

### Function-like wire evaluation

Pure Wires are supported. They can be run using `Do` and passing an input as a scalar/vector/matrix type. Their output is returned.

```clojure
(defpure rotation-matrix
  >= .input
  .input (Math.Cos) >= .c
  .input (Math.Sin)
  (| >= .s)
  (| (Math.Negate) >= .neg-s)
  (Float2 .c .s) >> .result
  (Float2 .neg-s .c) >> .result
  .result ; Output a 2x2 matrix
)

0.45 (Do rotation-matrix) >= .mat2-2 ; This will contain the result from the wire

```

Non-pure Wires are also supported. They will accept an input value and return their output.
In addition they will also capture any variables referenced that are defined in parent Wires.

```clojure
(defwire sub-logic
  .global (Math.Cos) >= .v
  2.0 (Math.Add .v)
)

3.0 >== .global
(Do sub-logic) >= .v1
2.0 > .global
(Do sub-logic) >= .v2
```

!!! info "Partial support"

    Currently, only values that are read from parent Wires are supported. Trying to update a variable defined in a parent Wire will have no effect on that variable.
