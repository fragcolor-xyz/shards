# Missing Physics Features

## Issue 1: No Raycasting or Shape Query Support

**Severity**: High
**Component**: `shards/modules/physics/`

### Description
The physics module has no shards for spatial queries, which are essential for most game physics use cases.

### Missing Functionality
- `Physics.Raycast` - Cast a ray and get hit results
- `Physics.RaycastAll` - Cast a ray and get all hits
- `Physics.ShapeCast` - Sweep a shape along a path
- `Physics.CollideShape` - Check what a shape overlaps with
- `Physics.CollidePoint` - Check what bodies contain a point

### Use Cases Blocked
- Line-of-sight checks
- Bullet/projectile hit detection
- Ground detection for characters
- Sensor/trigger areas without collision response
- Mouse picking in 3D

### Reference
Jolt provides these via `PhysicsSystem::GetNarrowPhaseQuery()` with methods like `CastRay()`, `CastShape()`, `CollidePoint()`, etc.

---

## Issue 2: Missing Common Shape Types

**Severity**: Medium-High
**Component**: `shards/modules/physics/shapes.cpp`

### Current Shapes
- BoxShape
- SphereShape
- CapsuleShape
- HullShape (convex hull from mesh)

### Missing Shapes
- **CylinderShape** - Common for wheels, pillars, cans
- **MeshShape** - Concave static geometry (level collision)
- **HeightFieldShape** - Terrain collision
- **CompoundShape** - Multiple shapes per body
- **OffsetCenterOfMassShape** - Adjust center of mass
- **RotatedTranslatedShape** - Transform a shape
- **TaperedCapsuleShape** - Character controllers

### Impact
Users cannot create terrain, complex static level geometry, or compound physics objects.

---

## Issue 3: Missing Constraint Types

**Severity**: Medium
**Component**: `shards/modules/physics/constraints.cpp`

### Current Constraints
- FixedConstraint
- DistanceConstraint
- SliderConstraint

### Missing Constraints
- **HingeConstraint** - Doors, wheels, rotating platforms
- **ConeConstraint** - Ragdoll shoulder/hip joints
- **PointConstraint** - Ball-socket joints
- **SwingTwistConstraint** - Advanced ragdoll joints
- **SixDOFConstraint** - Full 6 degrees of freedom control
- **PathConstraint** - Constrain to a path
- **GearConstraint** - Gear ratios between bodies
- **RackAndPinionConstraint** - Linear/rotational conversion
- **PulleyConstraint** - Pulleys and ropes

### Impact
Cannot create proper ragdolls, vehicles with wheels, doors, or mechanical systems.

---

## Issue 4: No Body Sleeping Support

**Severity**: High (Performance)
**File**: `shards/modules/physics/core.hpp:338`

### Description
Body sleeping is explicitly disabled with a TODO comment. This means every body in the scene is simulated every frame, even if completely stationary.

### Current Code
```cpp
settings.mAllowSleeping = false;  // TODO: Add a parameter to control this
```

### Impact
A scene with 1000 bodies will simulate all 1000 every frame. With sleeping enabled, stationary bodies are skipped, potentially reducing simulation cost by 90%+ in typical scenes.

### Suggested Implementation
1. Add `AllowSleeping` parameter to `Physics.Body` (default: true)
2. Add `Physics.WakeUp` shard to manually wake bodies
3. Add `Physics.Sleep` shard to manually sleep bodies
4. Consider `Physics.SetSleepThreshold` for tuning

---

## Issue 5: No Character Controller Support

**Severity**: Medium
**Component**: `shards/modules/physics/`

### Description
Jolt has a full character controller system (`CharacterVirtual`, `Character`) that handles:
- Ground detection
- Slope limits
- Step climbing
- Moving platform support

This is not exposed in the Shards physics module.

### Suggested Shards
- `Physics.CharacterController` - Create a character controller
- `Physics.CharacterMove` - Move the character with collision
- `Physics.CharacterIsGrounded` - Check if on ground
- `Physics.CharacterGetGroundNormal` - Get ground surface normal
