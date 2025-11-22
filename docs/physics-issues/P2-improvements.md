# Physics Module Improvements

## Issue 1: Soft Body Parameter Change Detection Is Broken

**Severity**: Medium
**Files**: `shards/modules/physics/physics.hpp:163-165`, `shards/modules/physics/soft_body.cpp:197-198`

### Description
Soft body parameter hashing always returns 0, so parameter changes are never detected after initial creation.

### Current Code in physics.hpp
```cpp
inline void BodyNode::updateParamHash0() {
  if (shape.index() == 0) {
    // ... regular body hashing
  } else {
    paramHash0 = 0;  // Soft body: always 0!
  }
}

inline void BodyNode::updateParamHash1() {
  if (shape.index() == 0) {
    // ... regular body hashing
  } else {
    paramHash1 = 0;  // Soft body: always 0!
  }
}
```

### Impact
Changing friction, pressure, damping, etc. on soft bodies at runtime has no effect.

### Suggested Fix
Implement proper hashing for soft body parameters similar to regular bodies.

---

## Issue 2: Hardcoded Physics System Limits

**Severity**: Low-Medium
**File**: `shards/modules/physics/core.hpp:499, 523`

### Description
Physics system limits are hardcoded with no way to configure them.

### Current Code
```cpp
JPH::TempAllocatorImpl tempAllocator{1024 * 1024 * 4};  // 4MB fixed

physicsSystem.Init(
  1024 * 8,  // 8192 max bodies
  0,         // num body mutexes (0 = auto)
  1024,      // max body pairs
  1024,      // max contact constraints
  ...
);
```

### Impact
- Large scenes may silently fail when exceeding 8K bodies
- Small scenes waste memory with 4MB temp allocator
- No way to tune for specific use cases

### Suggested Fix
Add parameters to `Physics.Context`:
- `MaxBodies` (default: 8192)
- `MaxBodyPairs` (default: 1024)
- `MaxContactConstraints` (default: 1024)
- `TempAllocatorSize` (default: 4MB)

---

## Issue 3: No Collision Event Filtering by Type

**Severity**: Low
**File**: `shards/modules/physics/physics.cpp:168-293`

### Description
`Physics.Collisions` returns all collision events but there's no way to filter by event type (ContactAdded vs ContactPersisted).

### Suggested Enhancement
Add optional `EventType` parameter:
- `All` (default) - Both added and persisted
- `Enter` - Only ContactAdded (new collisions)
- `Stay` - Only ContactPersisted (ongoing collisions)

---

## Issue 4: Missing Physics.IsValid or Physics.Exists Check

**Severity**: Low
**Component**: `shards/modules/physics/`

### Description
No way to check if a physics body reference is still valid before using it.

### Use Case
```shards
; Store body reference
@body = (Physics.Body ...)

; Later, check if still valid before use
@body | Physics.IsValid | If({
  @body | Physics.ApplyForce ...
})
```

---

## Issue 5: Factory Instance Memory Leak

**Severity**: Very Low
**File**: `shards/modules/physics/physics.cpp:745`

### Description
The Jolt Factory singleton is allocated but never freed.

### Current Code
```cpp
SHARDS_REGISTER_FN(physics) {
  JPH::Factory::sInstance = new JPH::Factory();  // Never deleted
  // ...
}
```

### Suggested Fix
Register cleanup in module unload, or use a static instance instead of heap allocation.

---

## Issue 6: Debug Draw Help Text Is Empty

**Severity**: Very Low
**File**: `shards/modules/physics/debug.cpp:135`

### Current Code
```cpp
static SHOptionalString help() { return SHCCSTR(""); }  // Empty!
```

### Suggested Fix
Add proper help text describing the debug visualization capabilities.

---

## Issue 7: SoftBodyShape Help Text Is Empty

**Severity**: Very Low
**File**: `shards/modules/physics/soft_body.cpp:397`

### Current Code
```cpp
static SHOptionalString help() { return SHCCSTR(""); }
```

### Suggested Fix
Add description: "Creates a soft body collision shape from a mesh. Used with Physics.SoftBody for cloth, rubber, and deformable objects."
