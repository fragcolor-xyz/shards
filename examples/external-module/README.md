# External Shards Module Example

A template for writing Shards modules that live **outside** the shards source tree,
in Rust and/or C++, and can be consumed two ways:

1. **Statically linked** into the shards runtime — required on iOS (and the right
   choice whenever you ship a single binary, e.g. a Swift app embedding
   Shards.framework).
2. **Dynamically loaded** as a plugin from `<script dir>/externals/` (desktop only,
   Rust path; see also [shards-external-base](https://github.com/fragcolor-xyz/shards-external-base)
   for the C++ dynamic variant).

## Layout

```
external-module/
├── CMakeLists.txt   # declares the module: C++ sources + Rust crate + register fns
├── cpp/example.cpp  # C++ shard  (Example.Square)  + SHARDS_REGISTER_FN(cpp)
├── rust/            # Rust crate (Example.Reverse) + shardsRegister_example_rust
└── test.shs         # exercises both shards
```

A single `add_shards_module()` can carry both languages. Registration functions
follow the convention `shardsRegister_<module>_<id>`:

- C++: `SHARDS_REGISTER_FN(cpp)` expands to `shardsRegister_example_cpp` (the
  module id comes from `SHARDS_THIS_MODULE_ID`, defined by the build).
- Rust: export `#[no_mangle] pub extern "C" fn shardsRegister_example_rust(core)`.

## Static consumption

Configure shards with the module directory:

```sh
cmake -Bbuild -DSHARDS_EXTERNAL_MODULE_DIRS=/path/to/your-module ...
```

`SHARDS_EXTERNAL_MODULE_DIRS` accepts a `;`-separated list. Each directory is added
like an in-tree module **before** the union libraries are generated, so:

- the Rust crate becomes a member of the generated rust union (one staticlib, one
  libstd — no duplicate-runtime link errors),
- the C++ sources compile into the C++ union,
- the generated registry calls your `shardsRegister_*` functions at runtime init —
  no app-side registration code needed, on any platform including iOS.

Verify with:

```sh
build/<config>/shards run examples/external-module/test.shs
```

## Dynamic consumption (desktop plugin, Rust)

```sh
cd rust
cargo rustc --release --features dylib --crate-type cdylib
```

This produces a `cdylib` that self-registers at `dlopen` time (a `ctor` resolves the
core vtable via the host's exported `shardsInterface` symbol, ABI-checked). Drop it
into `externals/` next to your script — the runtime scans that folder and loads any
`.dylib`/`.so`/`.dll` whose name does not start with `lib`.

## Pitfalls

- **Do not enable `shards/dllshard` unconditionally.** Keep it behind an opt-in
  feature (here: `dylib`). Cargo unifies features across a workspace: if this crate
  is built into the static union with `dllshard` on, *every* module switches to
  dlopen-based core resolution and the static build breaks.
- **Do not link an independently built Rust staticlib next to shards.** Two Rust
  staticlibs each carry a full libstd — duplicate symbols. Always join the union via
  `SHARDS_EXTERNAL_MODULE_DIRS` instead.
- **Module ids and `REGISTER_SHARDS` ids must form valid C identifiers**
  (`<module>_<id>`) — no dashes.
- When copying this template out of the tree, adjust the `shards` path dependency in
  `rust/Cargo.toml` (or use a git dependency).
