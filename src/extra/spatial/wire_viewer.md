Ideal code

```clj
(Spatial.UI
 :Queue .queue :View .view :Scale 100.0
 :Contents
 (->
  (WireViewer my-wire .base-transform)
  
  ;; which could be combined with other Spatial panels
  (Spatial.Panel
   :Transform .panel-t-0 :Size (Float2 640 360)
   :Contents (-> nil))))
```

Basic implementation

1. `WireViewer` gets a reference to the wire and a base transform
  - consider having the wire as input instead
2. `WireViewer` internally creates and manages `Spatial.Panel` (one for each shard in the wire)
3. `WireViewer` needs an API to be able to tell `Spatial.UI` about each panel (and also add/remove some)
4. the `base-transform` is the `(0,0,0)` coordinate from which all other panel coordinates are derived.
5. we need to have an API to update that coordinate
   - basically we need to be able to update `:Transform` and `:Size` for `Spatial.Panel` that are managed by `WireViewer`

Considerations
 
 1. `WireViewer` is implemented in rust
 2. `Spatial.UI` and `Spatial.Panel` are implemented in C++
 3. We need some interop for the two parts to communicate
 
 Specifically

 1. A struct that can represent a panel on C++ side, where the actual implementation is on rust side (through some C-api external calls).
 2. A pointer type for that struct.
 3. An implementation of the "geometry" that can cross interop boundaries.

 More details

 ```c++
 typedef void *ExtPanelPtr;
 
extern "C" {
const egui::FullOutput &spatial_render_external_panel(ExtPanelPtr, const egui::Input &inputs);
shards::spatial::PanelGeometry spatial_get_geometry(const ExtPanelPtr);
}

 struct ExternalPanel {
    ExtPanelPtr impl;

  ExternalPanel(ExtPanelPtr ptr) : impl(ptr) {}
  const egui::FullOutput &render(const egui::Input &inputs) { return spatial_render_external_panel(this->impl, inputs); }
  spatial::PanelGeometry getGeometry() const{ return spatial_get_geometry(this->impl); }
 }
 ```
 