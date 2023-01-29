This shard defines the starting point for creating interactive world space UI elements.

Every instance of [Spatial.Panel](../Panel) should be placed within this shards's Contents parameter.

The draw commands for the UI elements are added to the Queue that is passed in. In contrast to the screen space [UI](../../General/UI) shard, they do not need to be rendered using a [GFX.UIPass](../../GFX/UIPass)

!!! note
    When rendering the draw commands, the sorting mode should be set to [SortMode.Queue](../../../enums/SortMode) so they are drawn in the correct order.
    The [BuiltinFeatureId.Transform](../../GFX/BuiltinFeature/#transform) feature should be used
