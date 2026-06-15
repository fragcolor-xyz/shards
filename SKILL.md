---
name: shards
description: Write, verify, and run Shards programs. Use the `shards` CLI to discover shards, read their exact signatures, and type-check before running — Shards verifies whole programs at compose time, so most errors are caught without ever executing.
---

# Writing Shards

Shards is a data-flow language: data flows left-to-right through shards joined by `|`.
The whole program is **type-checked at compose time, before anything runs**, so the
fastest way to write correct Shards is a tight generate → verify → repair loop against
the CLI — never guess an API, ask the binary.

## The loop

1. **Discover** a shard by capability
   ```sh
   shards search http              # matches name + summary
   shards enumerate --filter CSV   # filter the full index by name
   ```
2. **Read its exact signature** (parameters, types, defaults) — ground truth, live from the binary
   ```sh
   shards docs Http.Get --json
   shards docs LogLevel --type enum --json
   ```
3. **Write** the `.shs` file.
4. **Verify before running** — parse + compose only, never executes
   ```sh
   shards check myscript.shs --json
   ```
   Exit code: `0` ok · `1` problems found · `2` could not run. Each diagnostic has
   `phase` (`parse`/`construct`/`compose`), `line`/`column`, `message`, structured
   `actual`/`expected` types, and repair aids: `candidates` (bridge shards for a type
   mismatch) and `did_you_mean` (for an unknown shard name).
5. **Repair** using the diagnostics, then re-run `check` until it's clean.
6. **Run** it
   ```sh
   shards run myscript.shs
   ```

## Context pack

`docs/shards-llm.txt` is the always-loaded pack: the full language guide plus a
one-line index of every shard. It is a *snapshot* — for the authoritative signature of
any specific shard, always use `shards docs <name> --json`. Regenerate the pack with
`just gen-llm-context`.

## Conventions that matter for generated code

- **Use the explicit word forms, not the operator sugar.** Write `Set`/`Ref`/`Update`/`Push`,
  not `>=`/`=`/`>`/`>>`. They are identical at the AST level, but the operators collide
  with priors from other languages (`=` is not assignment here). Word forms are the
  canonical AI-facing surface.
- **Comments are `//` (C-style)**, never `;`. (`;` was an old, removed Lisp-flavored syntax.)
- **Namespaces use `/`** for variables/shards (e.g. `fbl/set-tracked`); `::` is only the
  *enum* separator (e.g. `LogLevel::Info`).
- Newlines are implicit pipes — flow continues across lines until something stops it.
- `check` resolves `@include`s relative to the file; pass extra roots with `-I <dir>`.
  A file that depends on definitions injected by an outer file should be checked via that
  entry-point file, not in isolation.

## Quick reference

| Command | Purpose |
|---|---|
| `shards enumerate [--filter S] [--json]` | one-line index of all shards |
| `shards search <query> [--json]` | keyword search over name + summary |
| `shards docs <name> [--type shard\|enum] [--json]` | full signature (the drill-down tool) |
| `shards check <file> [--json]` | type-check (parse + compose), never runs |
| `shards run <file>` | execute |
| `shards format <file> [-i]` | format source |
| `shards ast <file>` | dump the JSON AST |

Everything composes with normal shell tools and emits machine-readable JSON with
`--json`, e.g. `shards enumerate --json | jq '.[] | select(.input|test("Image"))'`.
