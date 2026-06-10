# Shards: AI-First Roadmap

**Status:** Proposed
**Audience:** Core team / contributors
**Scope:** Strategic repositioning and concrete engineering plan for making Shards the reference runtime for AI-generated programs.

---

## 1. Thesis

The era in which a programming language competed on *human* ergonomics is ending. When AI agents write most code, a language survives on properties that matter to machines and to the humans who must **trust** machine-written programs:

- Can a program be **verified before it runs**?
- Can the full API surface be **enumerated and held in a model's context**?
- Can generated code be **constrained and sandboxed by construction**?
- Can the runtime host **long-lived, concurrent, stateful agents**?
- Is it **fast and embeddable** enough for domains scripting languages cannot enter (real-time, mobile, web, embedded)?

Shards already has all five properties as engineering facts. None of them is currently packaged, announced, or productized as such. This document sets the plan to do so.

> **The moat, in one sentence:** *AI-written Shards can be verified, constrained, and trusted in ways AI-written Python never can — and it runs at 60fps on a phone.*

### What is explicitly no longer the pitch

1. **"Intuitive, human-friendly flow syntax"** — human authoring ergonomics is a depreciating asset.
2. **"Visual & textual harmony" as an authoring story** — the visual layer's value moves to *inspection and auditing* of AI-built programs by humans.
3. **"Fast interpreted language" as a headline** — speed is an enabler of the real moat (real-time domains + cheap verification loops), not the moat itself.

### Existing assets this plan builds on

| Asset | Where | Why it matters |
|---|---|---|
| Three-phase model: parse → compose → activate | `shards/lang/src/eval.rs`, `shards/core/runtime.cpp` (`validateConnection`) | Whole-program type validation **before any side effect** — a free, milliseconds-cheap verification oracle for agent repair loops |
| Full API introspection | `Shards.Enumerate`, `Shards.Help`, `Shards.EnumTypes` (`shards/modules/core/core.cpp`), `PARAM_IMPL` reflection (`shards/core/params.hpp`) | The entire stdlib signature surface is machine-readable ground truth |
| Closed-world stdlib (~800+ shards) + complete language spec in ~3.3k lines | `lib/shards-guide.md`, `lib/shards-reference.md` | The **whole language + stdlib fits in a model context window**; hallucinated APIs are detectable and preventable |
| Formal PEG grammar | `shards/lang/src/shards.pest` | Mechanical path to constrained decoding (GBNF) and tooling |
| AST as canonical artifact, text as a view | `shards ast` / `shards build -j` (JSON AST), `shards format`, 1:1 visual mapping | Surface syntax is a *rendering choice*; we can optimize it for models without redesigning the language |
| Wire/mesh concurrency | `shards/core/runtime.hpp` (SHMesh), `coro.hpp` fibers | Long-lived stateful coroutines with isolated state and channels = an agent-shaped runtime |
| Embedded inference | `AI.*` (candle/mistral.rs), `LLM.*` (llama.cpp), `Whisper.*` | Models run *inside* the runtime; no external service required |
| Agent tooling modules | `localshell`, `ssh`, `shell-common`, `http` | Agents hosted in Shards can already act on the world |
| Docs generation pipeline | `docs/generate.shs` | Already produces an "AI-friendly format"; needs to ship to the repo, not Notion |

---

## 2. The threat (stated plainly)

The default trajectory of every niche language in the AI era is death by **training-data gravity**: models write what they've seen, nobody chooses what the model can't write. Shards has near-zero presence in pretraining corpora — and worse, the **deprecated Lisp-flavored syntax is what little presence exists**, so models don't just know nothing; they know wrong things.

The moat does not exist passively. It must be built. Every item below is in service of one loop:

```
agent generates → shards verifies (compose) → structured errors → agent repairs → verified program runs sandboxed
```

---

## 3. Roadmap

Items ordered by leverage-per-effort. Items 1–2 are prerequisites for everything else.

### 3.1 `shards check` — the compiler as the agent's tool

**Effort: weeks. Priority: P0.**

A compose-only mode with **structured, machine-readable error output**. Today's errors are human prose; an agent repair loop needs data.

Deliverables:

- `shards check <file> [--json]` — parse + compose, never activate. Exit code semantics for CI/agents.
- JSON error schema per diagnostic:
  - `file`, `line`, `column` (already tracked via `LineInfo` in `shards/lang`)
  - `phase` (`parse` | `compose`)
  - `expected` / `actual` types (structured, not stringified)
  - `shard` and `parameter` context
  - **`candidates`**: type-directed suggestions — "output is `String`, parameter wants `Int`; candidate bridges: `ParseInt`, …" — computed from the existing registry (input/output type matching is already implemented for compose; reuse it in reverse)
  - "did you mean" suggestions for misspelled shard names (edit distance over `Shards.Enumerate`)
- Warnings (e.g. the `If`/`Maybe` "branches return different types → Any" cases in `flow.hpp`) included with severity levels — agents should see type-widening, since it erodes downstream verification.
- Wall-clock budget: `check` on a typical script should stay in low milliseconds. **Compose speed is now an inner-loop metric**, same status as activation speed.

Acceptance: an LLM given a broken script + the JSON diagnostics (and nothing else) fixes typical type errors in one round-trip at a measurably higher rate than with prose errors.

### 3.2 MCP server — make Shards drivable by every agent

**Effort: weeks. Priority: P0.**

Expose the toolchain over Model Context Protocol so Claude Code, and every other MCP-speaking agent, can use Shards natively with zero prompt engineering.

Tools to expose:

- `check(source)` → structured diagnostics (3.1)
- `run(source, timeout, capabilities?)` → output + logs (sandboxed; see 3.6)
- `docs(shard_name)` → full signature: input/output types, parameters, help, example
- `enumerate(filter?)` → shard list with one-line summaries
- `search(query)` → semantic/keyword search over shard docs (can start as substring; later embed via `AI.Embed` — dogfooding)
- (later, with 3.5) `mesh_inspect` / `mesh_swap` for live-runtime work

Implementation note: this can largely be a Shards program itself (`Http.Server` + JSON shards already exist) — which is the best possible demo of the thesis.

Also in this item:

- **`shards-llm.txt` context pack**: a single canonical, versioned document — language guide + full shard reference — regenerated in CI from introspection ground truth (`docs/generate.shs` already builds most of this; redirect the output from Notion to the repo). It must be impossible for this file to drift from the binary.
- Ruthless consistency pass over all public docs: purge remaining old-syntax artifacts and Lisp-era leftovers (e.g. stale "comments begin with semicolon" phrasing). For a model, the docs *are* the language.

### 3.3 Surface syntax: keep the language, bless the word forms, measure the rest

**Effort: days for the decision + formatter work; the benchmark is a weekend of compute. Priority: P0 — because of sequencing (see 3.4).**

Findings:

- The paradigm (pipes, `@wire`, sub-blocks, `Name: value` params) has *neutral-to-good* LLM priors (shell pipes, Elixir `|>`, decorators, kwargs). **Keep it. Do not Pythonize** — a Python/JS skin imports Python/JS *semantic* priors (classes, exceptions, comprehensions, imports) that don't exist here; false familiarity is worse than no familiarity.
- The assignment operator family is the one **adversarial-to-prior** zone: `=` (Ref), `>=` (Set), `>` (Update), `>>` (Push) collide with the most semantically loaded operators in all of programming.
- Crucially, **the operators are pure eye candy**: `10 | Set(x)`, `| Ref(x)`, `| Update(x)`, `| Push(seq)` are the actual language; the operators are sugar over them (see `docs/sugar.md`). The "fix" therefore costs nothing at the language level — it is a *publishing policy*:

Decisions:

1. **Canonical AI-facing form = explicit word forms** (`Set`/`Ref`/`Update`/`Push`). All generated corpora (3.4), `shards-llm.txt`, MCP examples, and error-message suggestions use word forms exclusively.
2. Operators remain supported as human-eye-candy. `shards format` gains a `--canonical` mode (word forms) and a `--sugar` mode (operators) so the two renderings are mechanically interconvertible — syntax is a view over the AST, consistent with the visual-editor philosophy.
3. **Settle the remaining debate empirically, not by taste.** We own a verifier, so this is measurable: ~200 representative tasks × {operator syntax, word-form syntax} × current frontier models, same guide-in-context, measure first-pass `shards check` success rate and error histograms. If operator confusion doesn't dominate the histogram, this section's caution is refuted by data — and we'll know either way.

### 3.4 Synthetic corpus — attack the training-data gap

**Effort: months, parallelizable. Priority: P1. Hard sequencing constraint: starts only after 3.3 freezes the canonical surface.**

We own a generator-verifier pair: introspection enumerates valid constructions; `compose` verifies them. That is a synthetic-data factory.

- Generate large corpora of **verified** Shards programs across task families (data transforms, wires/channels, UI, gfx, net, AI shards).
- Generate **error→fix pairs** (mutate verified programs, capture the structured diagnostic, pair with the fix) — for repair-loop training these are more valuable than clean programs.
- Publish the corpus permissively; seed public GitHub with real, runnable Shards projects so future pretraining crawls pick the *new* syntax up.
- Fine-tune at least one open model as an existence proof, evaluated with the 3.3 benchmark harness.
- Actively mark the old Lisp-era syntax as deprecated in every crawlable surface.

> **Why sequencing is non-negotiable:** a language with a corpus can never cheaply change its surface again — every future model's prior of Shards will be whatever this corpus says. 3.3 decides; 3.4 sets it in stone.

### 3.5 Agent residency — the live runtime story

**Effort: months. Priority: P1. The flagship demo.**

Let an agent attach to a **running** mesh:

- Inspect scheduled wires, their states, and live variable values (the reflection machinery exists; it needs a protocol surface — natural fit as MCP tools on 3.2).
- Compose a replacement wire **against the live environment's actual types** — compose-time checking applied to hot code.
- Hot-swap wires in a running mesh.

This is the Smalltalk-image story reborn for agents: the agent doesn't edit dead text and restart a process — it operates on a living program. No mainstream language can offer this without heroics; the wire/mesh architecture is most of the way there. Combined with 3.2, this is a demo no other language can replicate.

### 3.6 Capability manifests — the safety story

**Effort: months (design-heavy, mechanically cheap). Priority: P1.**

Because all effects flow through a finite, enumerable shard set, Shards can offer what no ambient-authority language can: **compose-time capability enforcement**.

- A wire (or schedule, or embedding host) declares an allowed shard set / namespace whitelist (e.g. `deny: [FS.* Process.* Network.*]` or allow-list style).
- `compose` rejects violations — *before execution*, with structured diagnostics (3.1).
- This is the answer to "run AI-generated code on an end user's device": fast enough for real-time, sandboxable by construction, verified before execution, embeddable everywhere (desktop, iOS/visionOS, wasm, RISC-V).

Target scenario: AI-generated interactive content (UGC) running on someone's phone at 60fps. That intersection — real-time + sandboxable + verifiable + embeddable — is empty of competitors.

### 3.7 Performance — keep the lead, spend it wisely

**Effort: ongoing. Priority: P2 (the lead is already real; these extend it).**

Current speed mechanisms (for the record): `InlineShard` enum-switch dispatch eliminating indirect calls for hot shards (`shards/modules/core/inlined.cpp`), compose-time type erasure of all runtime checks, warmup/activate memory recycling, fcontext fiber scheduling (`shards/core/coro.hpp`, `shards/core/asm/`).

Remaining overhead and the order to attack it:

1. **Compose-time fusion pass** — the wire graph is fully typed and static after compose; fusing common chains (`Get | Math.Add | Update`, dead store elimination, const-folding) into single activations is mechanical. Add an `optimizeWire()` pass in `composeWire()`. Benchmark: `shards/tests/cbperf.shs` is exactly this shape.
2. **Monomorphic specialization** — when a pipeline composes to a single concrete type, emit a no-box activation path on raw `int64_t`/`double` instead of tagged `SHVar` flow.
3. **Copy-on-write sequences/tables** — kill clone costs on immutable flow.
4. **JIT for `Looped` wires** — last, and only if 1–2 leave money on the table; high reward but a permanent maintenance tax.

New metric: **`shards check` latency is a first-class performance target** — it is the frequency of the agent's inner loop.

### 3.8 Interop pressure valve

**Effort: months. Priority: P2.**

A closed world is only a virtue with a pressure valve. ~800 shards is not an ecosystem; agents will hit walls (the payment API, the niche driver). Without a sanctioned escape hatch, users fall back to Python and never return.

- First-class, **sandbox-preserving** extension story. The wasm component model is the natural fit: extensions are capability-scoped by construction, keeping the 3.6 guarantees intact.
- Extensions must register full type signatures so they participate in compose-time verification and introspection like native shards — no opaque escape hatches.

---

## 4. Repositioning (README / site)

Rewrite the public pitch around **verification and trust**, not flow and intuition:

- Headline: the one-sentence moat (§1).
- Lead with: complete spec in one context window; verify-before-run; capability sandboxing; agent-shaped concurrency; embedded inference; real-time everywhere.
- The visual layer is presented as the *human audit surface* for AI-built programs.
- "AI-Ready" stops being aspirational the day 3.1 + 3.2 ship; until then, don't claim it louder than the code supports.

---

## 5. Sequencing summary

```
P0 (now, weeks):    3.1 shards check (JSON diagnostics)
                    3.2 MCP server + shards-llm.txt + docs consistency pass
                    3.3 canonical-form decision + generation benchmark
P1 (next, months):  3.4 synthetic corpus + open-model fine-tune   [after 3.3 freeze]
                    3.5 agent residency (live mesh attach/swap)   [flagship demo]
                    3.6 capability manifests
P2 (ongoing):       3.7 fusion → specialization → COW → (maybe) JIT
                    3.8 wasm-component extension story
                    §4 public repositioning                        [after 3.1+3.2]
```

The pieces are roughly 70% built. What was missing is the assembly and the thesis — this document is the thesis; the P0 items are the assembly.
