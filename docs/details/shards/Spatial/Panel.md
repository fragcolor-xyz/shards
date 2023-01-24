This shards defines a single world space UI panel. This shard needs to be placed within a [Spatial.UI](../UI)'s Contents block.

This size of the panels defined using virtual UI points/units/pixels. How these virtual points map to world coordinates is controlled by the UI's Scale parameter. For example if the UI's scale is set to 200; 200 virtual points will be equal to 1 unit in world space.

A panel's position and orientation in the world is controlled by the `Transform` parameter, which is a standard 4x4 transformation matrix.

!!! danger
    Applying scaling on this transform is not recommended and may result in undefined behavior.

The Panels' contents should be placed in the `Contents` parameter, similar to the screen space [UI](../../General/UI)
