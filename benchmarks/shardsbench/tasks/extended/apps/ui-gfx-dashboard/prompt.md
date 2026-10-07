Build a small but complete dashboard application as a looped wire named `solution`.

The app must:

- open a 640×480 `GFX.MainWindow` titled `ShardsBench Dashboard`;
- create and render a green built-in cube using a draw queue, transform/base-color
  features, a drawable pass, a camera view, and a rotation that changes each frame;
- create a UI draw queue and UI pass after the drawable pass;
- render a top-panel title and a central panel containing the current frame label, a
  progress bar, and a `Reset` button;
- initialize a mutable global integer named `app-frame-count` to zero exactly once;
- increment `app-frame-count` once per rendered frame in the outer looped `solution`
  wire, immediately before `GFX.MainWindow`;
- make the Reset button set `app-frame-count` back to zero when clicked;
- render both the scene and UI every frame.

Use canonical `Set`, `Update`, and `Push` word forms. Define the application only; the
grader supplies the mesh, schedules `solution`, and runs exactly eight frames.
