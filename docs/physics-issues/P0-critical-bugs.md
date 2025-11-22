# Critical Physics Module Bugs

## Issue 1: Memory Leak - Bodies Never Removed From Mirror Map

**Severity**: Critical
**File**: `shards/modules/physics/body.cpp:159-169`

### Description
When a `Physics.Body` shard is cleaned up, the body is disabled but never actually removed from the `PhysicsMirror` map. Over time in long-running applications, this causes unbounded memory growth.

### Current Code
```cpp
void cleanup(SHContext *context) {
  if (_instance.node && _instance.node->node) {
    auto &bodyNode = _instance->node;
    bodyNode->enabled = false;
    bodyNode->persistence = false;
    bodyNode->selfVar.reset();
  }
  // Body remains in bodyMirror.map forever!
}
```

### Expected Behavior
The body should be removed from the mirror map when the shard is cleaned up.

### Suggested Fix
Add a `removeNode()` call or equivalent cleanup mechanism that removes the body from `bodyMirror.map`.

---

## Issue 2: Memory Leak - Constraints Never Removed From Mirror Map

**Severity**: Critical
**File**: `shards/modules/physics/constraints.cpp:50-57`

### Description
Same issue as bodies - constraints are disabled but never removed from the constraint mirror map.

### Current Code
```cpp
void baseCleanup(SHContext *context) {
  if (_constraint) {
    _constraint->enabled = false;
    _constraint->persistence = false;
    _constraint.reset();  // shared_ptr released, but entry stays in map
  }
}
```

---

## Issue 3: ApplyForceAt Has Incorrect Logic Operator

**Severity**: High
**File**: `shards/modules/physics/physics.cpp:703-709`

### Description
The `ApplyForceAt` shard uses `&&` instead of `||` when checking if force should be applied. This means a force like `(100, 0, 0)` is completely ignored because not ALL components are non-zero.

### Current Code
```cpp
if (fpl[0] != 0.0 && fpl[1] != 0.0 && fpl[2] != 0.0) {  // BUG: requires ALL non-zero
  bodyInterface.AddForce(body->GetID(), fpl, at);
}
```

### Expected Behavior
Force should be applied if ANY component is non-zero.

### Suggested Fix
```cpp
if (fpl[0] != 0.0 || fpl[1] != 0.0 || fpl[2] != 0.0) {
  bodyInterface.AddForce(body->GetID(), fpl, at);
}
```

Or simply remove the check entirely since adding a zero force is harmless.

---

## Issue 4: Typo `shasert` Instead of `shassert`

**Severity**: Medium (potential compilation error)
**File**: `shards/modules/physics/core.hpp:198`

### Description
Typo in assertion macro name will cause compilation failure if that code path is exercised.

### Current Code
```cpp
shasert(node->data);  // Should be shassert
```

### Fix
```cpp
shassert(node->data);
```

---

## Issue 5: Thread Buffer "Lazy GC" Can Cause Data Loss

**Severity**: Medium-High
**File**: `shards/modules/physics/core.hpp:81-85`

### Description
When more than 64 unique thread IDs have written to the event collector, ALL thread buffers are cleared. This could cause event loss if the clear happens mid-frame.

### Current Code
```cpp
// TODO: Fix lazy GC
//  technically okay since we schedule on the same thread pool anyways
if (threadBuffers.size() > 64)
  threadBuffers.clear();
```

### Suggested Fix
Implement proper cleanup of stale thread buffers rather than clearing all of them, or increase the limit and add monitoring.
