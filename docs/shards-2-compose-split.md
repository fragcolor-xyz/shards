# Shards 2.0: Compose Once, Instantiate Many

**Status:** Proposed (design only, nothing implemented). Top engineering priority as of 2026-10-04.
**Audience:** Core team / contributors
**Scope:** The core design for Shards 2.0, a new Rust runtime in a new repo. Compose output becomes a shared, read-only artifact, separate from the small runtime state each instance owns. Also covers what carries over from 1.x (§3.6) and how to validate the design before porting (§5).

Related: [`ai-first-roadmap.md`](ai-first-roadmap.md). Surface syntax (§3.3) and compose-time optimization (§3.7) are covered there. This doc is the runtime model those build on.

---

## 1. Problem

In Shards 1.x, a shard is one object that holds its parameters, its compose output, its warmup resources and its activate scratch state together. Running the same wire N times gives N copies of all of it, even though everything except the runtime state is identical.

Formable hit this first. Shards was its game-engine scripting language, and spawning 100 identical entities meant 100 copies of identical compose output. The problem is general, though: the same machinery serves:

- HTTP request handlers (`shards/modules/http/http.cpp`, `WireDoppelgangerPool<Peer>`)
- WebSocket and KCP connections (`network_ws.cpp`, `network_kcp.cpp`)
- `Expand` / `TryMany` / `DoMany` items

So any Shards server with 100 connections pays the same cost.

**Goal:** N instances of a wire should cost one compiled artifact plus N small states.

## 2. How 1.x works today (audit, 2026-10-04)

Findings come from reading the code; nothing was measured. Line numbers were correct when this doc was written. Re-check them before relying on them.

### 2.1 Instances are rebuilt, not shared

Every multi-instance path (`Spawn`, `Expand`, `TryMany`, `DoMany`, network servers) goes through `WireDoppelgangerPool` (`shards/core/wire_doppelganger_pool.hpp`). When the pool is created it serializes the template wire once. Each new instance then (`acquireFromBatch`, ~L115-143):

1. Deserializes the string. Every shard is recreated with `createShard`, then `setup`, then `setParam` for each parameter, and literals are deep-copied. Sub-wires are deep-cloned too.
2. Is renamed `"{name}-{idx}"` and given a new id.
3. Runs a full `compose`.

A recycled pool item skips steps 1-3, but every schedule still pays `prepare` (a coroutine plus stack allocation) and the warmup of every shard.

Nothing dedupes compose results across identical wires. The only existing reuse is "the same `SHWire` object is not recomposed" (the `if (!wire->composeResult)` guard in `shards/modules/core/wires.cpp`, used by `Do` / `Detach` / `Step`). It does not help, because each instance is a different object. Identical instances also hash differently: the wire hash mixes in `wire->name` (`shards/core/hash.inl` ~L344), and instances are renamed.

### 2.2 Compose output lives inside instances

This is what makes sharing structurally impossible today, not just unimplemented:

- `wire->outputType` is a shallow copy that points into a shard of that wire (`shards/core/runtime.cpp` ~L1526, see the comment there).
- Exposed and required variable arrays are owned by each shard (`PARAM_REQUIRED_VARIABLES`, `ExposedInfo` members). Sub-flow parameters store their own compose result (`ShardsVar` / `TShardsVar`).
- Compose writes into the per-instance shard header (`inlineShardId`; in Rust, `decl_override_activate!` replaces `activate`).
- The C ABI `struct Shard` (`include/shards/shards.h`) holds about 25 function pointers per instance, with no shared vtable.
- In both C++ (`ShardWrapper<T>`) and Rust (`trait Shard`: `compose`, `warmup` and `activate` all take `&mut self`), parameters, compose output and runtime state live in one struct.
- `SHMesh::schedule` throws "Multiple wire schedule" if the same `SHWire` is scheduled twice (`shards/core/runtime.hpp` ~L592). So copying instances is currently the only way to run a wire concurrently.

### 2.3 Resources keyed by object identity, not content

Composing once is not enough on its own. Several subsystems recognize resources by object identity, so even a shared compose would still duplicate them unless the keys change too.

| Resource | Where | What happens with N identical instances | Weight |
|---|---|---|---|
| **GPU pipelines** | `Feature::pipelineHashCollect` hashes only `id` (`shards/gfx/pipeline_hashes.cpp` L29). `GFX.Feature` warmup does `std::make_shared<Feature>()` (`shards/modules/gfx/feature.cpp` ~L303). `UniqueVariables` gives each instance different global names. | Up to N pipelines. Each one re-composes shader shards, generates WGSL, runs naga and creates a wgpu pipeline. Also splits instanced batches, since draws are grouped by pipeline hash. | Heaviest |
| Feature generator wires | `GFX.Feature` with `DrawableGenerators` / `ViewGenerators` deep-clones them and composes a fresh `Brancher` per instance | N wire composes plus warmups | Medium-heavy |
| glTF by `Path` | `GFX.glTF` warmup or first activate → full parse, image decode, GPU upload | N loads. No cross-instance cache. | Very heavy |
| Textures / meshes | `GFX.Texture`, `GFX.Mesh`, `GFX.BuiltinMesh` create a GPU object per instance | N uploads of identical data | Medium |
| Physics shapes | `Physics.HullShape` (`ConvexHullShapeSettings`, `shards/modules/physics/shapes.cpp`). `Physics.SoftBodyShape` (`CreateConstraints` + `Optimize`, `soft_body.cpp` ~L428-438). Box/Sphere/Capsule. | N hull and constraint builds, although Jolt's `Ref<>` types are designed to be shared | Heavy (hull, soft body) |
| Model loads | `LLM.Model`, `Whisper.Load`, `ML.Model`, candle GGUF | N loads if placed in an instanced wire | Very heavy |
| Compiled artifacts | Regex (`std::regex::assign` in `setParam`, `shards/modules/core/strings.cpp`), Jinja environment and template, SQLite prepared statements, kissfft configs | N compilations | Light-medium |
| egui | `UI` creates its own `egui::Context` and font atlas. `UI.Render` uploads its own font texture and creates its own UI feature. | N atlases and N pipelines | Medium-heavy |

### 2.4 What already works and should carry over

- **`GFX.glTF Copy:`** clones only the node tree and shares `MeshPtr` / `MaterialPtr`. This is the 2.0 model on a small scale.
- Already shared: HTTP clients (per mesh, keyed by proxy/cert config), SQLite connections (per mesh by db name), channels, and the `Expect` input-type cache.
- `TypeCache` (`shards/core/type_cache.hpp`) is a global hash-interned `TypeInfo` store. It has one call site today and is the natural building block for shared types.
- Clone composes commonly receive the same input type and shared-variable types. Cache soundness still requires auditing all other compose dependencies and making them explicit (§3.2).

## 3. Design

### 3.1 Three-layer shard model

The core idea, by analogy: compose output is the program, and instances are processes.

```rust
trait Shard {
    /// Parameters, validated. Built by the parser/loader.
    type Params;
    /// Compose output: resolved types, exposed/required variables, constant
    /// tables, compiled regexes, shader descriptors... Immutable and shareable.
    type Compiled: Send + Sync + 'static;
    /// Per-instance runtime state: counters, buffers, cursors, device handles.
    type State;

    fn compose(params: &Self::Params, ctx: &ComposeCtx) -> Result<Self::Compiled>;
    fn instantiate(compiled: &Self::Compiled, ctx: &InstanceCtx) -> Result<Self::State>; // replaces warmup
    fn activate(compiled: &Self::Compiled, state: &mut Self::State, input: &Var) -> Result<Var>;
    fn cleanup(compiled: &Self::Compiled, state: &mut Self::State);
}
```

The API makes the split explicit and gives the compiler useful ownership checks:

- `activate` receives `&Compiled`, preventing ordinary mutable access. Interior mutability is still possible: the contract must forbid instance-dependent semantic changes through shared compiled data.
- `Compiled: Send + Sync` permits safe cross-thread sharing in safe Rust; it does not prove immutability or make a device-bound resource valid in another context.
- Each shard type gets one static vtable, replacing per-instance function pointers.

A **compiled wire** is an immutable graph of `Arc<Compiled>` nodes plus wire-level types (input, output, requirements, stack size). It owns every type it exposes, so nothing points into instance memory (this fixes §2.2). An **instance** is a flat state block, ideally one allocation sized at compile time, plus a reference to its compiled wire.

### 3.2 Content-keyed compose cache

Cache compiled wires and shards by a **structural key**:

- shard identity and implementation version (or an equivalent cache scope tied to an immutable registry)
- parameter values (hashed)
- input type
- the resolved bindings and types of the variables the shard reads from its environment
- any compose-visible host configuration, target features, and policy assumptions

Incidental instance names and ids are excluded (this fixes the §2.1 hashing issue). Variable names may be normalized only when the binding representation preserves their meaning. Runtime values are excluded only if compose cannot observe them.

**Compose contract:** output is a deterministic function of declared inputs. `ComposeCtx` must expose dependencies explicitly; ambient filesystem, clock, registry, or host-state reads cannot silently affect a cached result. A shard whose dependencies cannot yet be represented must bypass caching. Capability approval must not be inherited from an artifact compiled under another policy: either key the relevant policy or repeat admission checks for the receiving host.

Hashes index cache entries; structural equality must resolve collisions before reuse. Invalidation must follow dependency changes, including nested compiled artifacts. Under this contract:

- 100 `Spawn`s of one template → one compose, 100 `instantiate` calls.
- Identical sub-wires used in different places share one compiled artifact.
- Hot reload recompiles only shards whose keys changed.
- Loading the same script twice can reuse compose output when dependencies match; parsing, admission checks, and instantiation may still cost work.

The 1.x XXH3/XXH128 hashing and `TypeCache` interning carry over as the implementation.

### 3.3 Content-keyed shared resources

For §2.3, a `Compiled` value may hold handles to **shared resources**, which are keyed by content and refcounted:

- **GPU features and pipelines:** keyed by shader content (`EntryPoint::getPipelineHash` already exists) instead of object id. `UniqueVariables` becomes opt-in for features that really need per-instance globals. The pipeline key additionally includes the GPU device and context.
- **Assets** (glTF, textures, meshes, images): keyed by path plus modification time, or by a content hash. Per-instance state is a lightweight node tree or transform, which generalizes `GFX.glTF Copy:`.
- **Physics shapes:** keyed by shape type plus parameters, plus a mesh content hash for hull and soft-body shapes. Shared as Jolt `Ref<>`.
- **Models:** keyed by path plus load parameters. Per-instance state is the context or KV cache.
- **Compiled regexes, templates and prepared statements:** keyed by source text (statements per connection).

Keep portable compiled descriptions separate from device- or connection-bound realizations. Resolve the latter through services scoped to the owning device/context/connection, with explicit teardown and invalidation. A reusable compiled wire must not retain a handle that only works in the host that first composed it.

Resources that actually need to change and be shared (e.g. a texture cache that streams in) become explicit **services** that shards call. Their synchronization and lifetime rules are separate from the immutable compiled artifact contract.

### 3.4 Scheduling (hypothesis)

Once each instance's state is an explicit `State` value rather than implicit fiber-stack contents, a wire may be expressible as a resumable state machine. That could reduce or remove the need for stackful coroutines, both natively and on wasm, where Asyncify is used today and the JSPI attempt failed for architectural reasons (see CLAUDE.md, "JSPI Fiber Implementation").

This is **unproven**. The hard cases are shards that call other shards (`Do`, `Branch`, sub-flows) and shards that suspend deep inside nested calls. Prototype before committing. If it fails, a Rust stackful-coroutine library (e.g. `corosensei`) is the fallback, and §3.1-3.3 do not depend on the outcome.

### 3.5 Rust hosts, C++ is consumed

The 2.0 core is Rust: runtime, scheduler, type system, composer. C++ is no longer the host. It remains as libraries called from Rust: llama.cpp, whisper.cpp, JoltPhysics, miniaudio, tracy and others, behind thin binding crates. This reverses 1.x's arrangement, where C++ hosts and calls Rust modules. Benefits:

- One build system and one toolchain for the core.
- More ownership and lifetime checks within the Rust core; C++ bindings and unsafe code still require explicit lifetime contracts and auditing.
- The `Compiled`/`State` separation is expressed in the type system, with additional contracts for interior mutability and resource scope.

### 3.6 New repo, new core, carried-over parts

2.0 is a new implementation in a **new repository**, not an in-place migration. The compiled/state split changes the shard contract, the ABI, the clone path and possibly the scheduler. Migrating 1.x step by step would mean keeping both models working at every step, which costs more than a new core. A new repo also starts as a plain Cargo workspace, without 1.x's CMake setup, submodules and C++ build workarounds. 1.x stays maintained in this repo for existing users.

New implementation:

- runtime, scheduler, type system and composer
- the shard trait and registry (§3.1), the compose cache (§3.2) and resource services (§3.3)

Carried over and adapted, not rewritten:

- **Language front end:** `shards/lang` (pest grammar, parser, AST, formatter, `shards check` with its did-you-mean and candidate engines) is already Rust. Changes follow 2.0's syntax decisions ([`ai-first-roadmap.md`](ai-first-roadmap.md) §3.3).
- **Agent tooling:** the `check` / `docs` / `enumerate` / `search` CLI surface and the `skills/shards/` skill (roadmap §3.1-3.2).
- **Rust modules** (`http`, egui, the gfx Rust backend, candle and others): the domain logic carries over, and the shard binding layer is ported to the new trait.
- **C++ libraries** (§3.5): consumed through binding crates.
- **Tests and samples:** the conformance suite (§5), converted mechanically if the syntax changes.

**Scope guard.** A clean rewrite invites every feature ever wanted, which is how second systems fail to ship. The first milestone in the new repo is the narrow prototype in §5, before any gfx, physics or AI porting.

## 4. Open questions

1. **Compose that depends on runtime values.** Shards that resolve variables at warmup, or `Once`-style setup. Rule: anything compose needs must be known at compose time; everything else is state. Every shard that currently blurs this line needs an audit.
2. **Instance-specific compose.** Can two instances of one template ever compose differently (e.g. different captured-variable types in `Spawn`)? If so, the captured types must be part of the cache key, not a reason to recompose everything.
3. **Running one wire concurrently.** With instances separated from compiled wires, the 1.x restriction that one `SHWire` cannot be scheduled twice disappears: instances are scheduled, compiled wires are not. Confirm nothing else relies on it.
4. **Cache eviction.** Unbounded compose and resource caches in long-running servers need refcount-based or LRU eviction. The 1.x pipeline cache's frame-based eviction is a starting point.
5. **Debugging and introspection.** Line info, `shards check` diagnostics and profiling (tracy zones) must work against compiled artifacts, with instance identity shown only where it matters.

## 5. Validation

In order:

1. **1.x baseline benchmark.** Before any 2.0 work, add a 1.x benchmark that spawns 100 identical entity-like wires (gfx feature + mesh + physics body + some logic) and records compose time, memory and pipeline count. Include a CPU-only variant (logic and core shards only), so the narrow prototype in step 2 has a directly comparable baseline. In 2.0, the full benchmark is the acceptance test: adding instances 2-100 should cost roughly `sizeof(State)` each and create zero additional pipelines.
2. **Narrow prototype before a full port** (the first milestone in the new repo). Implement a small shard subset with shared compiled artifacts and independent state, using the simplest scheduler that works. For 100 instances, measure compose count, creation latency, live memory, and state isolation against the CPU-only baseline. Include cache misses for changed bindings/types, host configuration, and policy assumptions, plus equivalent instances that should hit. A forced hash collision must not reuse the wrong artifact. This validates the split without requiring a scheduler rewrite or graphics port.

Throughout:

- **Conformance.** The existing suite (`./run-tests.sh --with-cpu`: ~110 test scripts plus ~260 doc samples, 370/370 passing on Linux Debug as of 2026-10-04) becomes the 2.0 conformance suite, ported mechanically if the syntax changes.
- **Possible 1.x step.** Content-keyed feature hashing (§3.3, first bullet) can land in 1.x on its own. It collapses N pipelines to one for identical entities and restores instanced batching. It is optional now that Formable is no longer a driver, but it is a cheap way to validate the keying scheme.
