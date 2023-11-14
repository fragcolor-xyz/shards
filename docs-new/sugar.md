# Shards Syntax Sugar Documentation

## Introduction

This document provides an overview of the syntax sugar in the Shards programming language, particularly focusing on `Get`, `Set`, `Ref`, `Update`, and `Push`, used in conjunction with `Const`. These constructs simplify and enhance the readability of code.

## Syntax Sugar in Shards

### `Set` Sugar

**Explicit Shard:**
```shards
Const(10) | Set(x)
```

**Syntax Sugar Equivalent:**
```shards
10 >= x
```

**Description:**
- **Purpose:** Sets a mutable variable.
- **Usage:** Simplifies setting `x` to a constant value (10).
- **Mutability:** Mutable operation.

---

### `Update` Sugar

**Explicit Shard:**
```shards
Const(11) | Update(x)
```

**Syntax Sugar Equivalent:**
```shards
11 > x
```

**Description:**
- **Purpose:** Updates a variable's value.
- **Usage:** Changes the value of `x` to 11.
- **Mutability:** Mutable operation with an existing value update.

---

### `Ref` Sugar

**Explicit Shard:**
```shards
Const(12) | Ref(y)
```

**Syntax Sugar Equivalent:**
```shards
12 = y
```

**Description:**
- **Purpose:** Creates an immutable reference.
- **Usage:** Assigns an immutable reference `y` to the constant value 12.
- **Mutability:** Immutable operation.

---

### `Get` and `Push` Sugar

**Explicit Shard:**
```shards
Get(y) | Push(seq)
```

**Syntax Sugar Equivalent:**
```shards
y >> seq
```

**Description:**
- **Purpose:** Retrieves and appends a value to a sequence.
- **Usage:** Appends the value of `y` to the sequence `seq`.
- **Mutability:** Mutable operation on the sequence.

### `Sub` Sugar

**Fully Explicit Shard:**
```shards
Const("Hello World!") | Sub({StringToBytes | ToHex | Log("Hex Hello World!")}) | Log("Normal Hello World!")
```

**Syntax Sugar Equivalent:**
```shards
"Hello World!" | {StringToBytes | ToHex | Log("Hex Hello World!")} | Log("Normal Hello World!")
```

**Description:**
- **Purpose:** Streamlines data processing through a sequence of operations.
- **Usage:** Removes the need for the explicit `Sub` shard, allowing for more concise expression.

---

### Special Evaluation Constructs

#### Explicit Evaluation

**Fully Explicit Shard:**
```shards
Const(1) | Math.Add(1) | Set(x)
Const(1) | Math.Add(x) | Assert.Is(3)
```

**Syntax Sugar Equivalent:**
```shards
1 | Math.Add((1 | Math.Add(1))) | Assert.Is(3)
```

**Description:**
- **Purpose:** Facilitates inline and nested calculations.
- **Usage:** Condenses complex mathematical expressions into a more streamlined form.

#### Forced Evaluation at Script Evaluation

**Fully Explicit Shard:**
```shards
Const(1) | Math.Add(2) | Assert.Is(3)
```

**Syntax Sugar Equivalent:**
```shards
1 | Math.Add(#(1 | Math.Add(1))) | Assert.Is(3)
```

**Description:**
- **Purpose:** Pre-evaluates expressions at script evaluation time.
- **Usage:** Performs advance computation of certain code segments before execution sequence begins.
