---
name: shards
description: Write, verify, and run Shards programs. Shards is a data-flow language that type-checks whole programs at compose time (before running), so the workflow is generate → verify → repair against the `shards` CLI. Use this skill whenever reading or writing .shs files, or driving the shards binary.
---

# Shards

Shards is a **data-flow** language: values flow left-to-right through *shards* joined by
`|`. The whole program is **type-checked at compose time, before anything runs** — so
never guess an API; ask the binary, and verify before running.

This skill is self-contained:
- **This file** — the mental model, a working cheat-sheet, and the CLI loop. Enough to
  read and write everyday Shards.
- **`GUIDE.md`** (next to this file) — the complete language guide. Read it before
  writing anything non-trivial.
- **`examples/`** — small, runnable, `check`-passing programs.
- The **shard catalog is live** (not a file): `shards enumerate` / `shards docs --json`.

## Mental model

Variables are **channels, not containers**. `|` passes the value along; a newline is an
implicit `|` (flow continues across lines until something stops it). Everything is a
shard — the assignment operators are just sugar over shards.

```shards
"world" | Set(name)            // name channels the string
["Hello " name "!"] | String.Format | Log   // -> "Hello world!"
```

## Cheat-sheet

### Variables — use the word forms

`=`/`>=`/`>`/`>>` are sugar and collide with other languages' priors. **Write the word
forms:**

| Word form   | Operator | Meaning                              |
|-------------|----------|--------------------------------------|
| `Set(x)`    | `>= x`   | create/assign a **mutable** variable |
| `Ref(x)`    | `= x`    | bind an **immutable** variable       |
| `Update(x)` | `> x`    | update an existing mutable variable  |
| `Push(s)`   | `>> s`   | append to a sequence                 |

```shards
0 | Set(counter)
counter | Add(1) | Update(counter)
item | Push(items)
```

### Literals & types

```shards
42  3.14  "text"  true  false  none      // int float string bool none
@f3(1 2 3)  @i2(4 5)  @color(0.8 0.2 0.2) // vectors
[1 2 3]                                   // sequence
{name: "Alice" age: 30}                   // table
```

### Calling shards

```shards
Add(5)                                    // positional
Http.Get(URL: "https://x" Timeout: 30)    // named (positional MUST come before named)
"a,b" | String.Split(",")                 // input flows in from the left
```

### Control flow

```shards
v | If({IsMore(10)} {"big" | Log} {"small" | Log})   // predicate is a {block}
v | When({IsMore(10)} {"big" | Log})                 // no else
v | Match(["a" {"A"} "b" {"B"} none {"?"}] Passthrough: false)
v | Cond([{Is("a")} {"A" | Log}  {true} {"default" | Log}])
Repeat({"tick" | Log} Times: 3)
[1 2 3] | ForEach({Mul(2) | Log})                    // $0 is the current element
Maybe({ risky } { "failed" | Log })                  // try / catch
```

### Wires & mesh (the runtime)

```shards
@wire(main {
  Once({ 0 | Set(counter) })   // runs once; allocations happen here
  counter | Add(1) | Update(counter) | Log
} Looped: true)

@mesh(root)
@schedule(root main)
@run(root FPS: 60)
```
`Do(wire)` runs inline sharing state; `Detach(wire)` schedules a copy (non-blocking);
`Step`, `Spawn`, `Branch` exist too — see GUIDE.md.

### Templates, defines, includes

```shards
@define(api "https://api.example.com")
@template(greet [name] { ["Hello " name] | String.Format })
@greet("world") | Log
@include("utils.shs" Once: true)
```

## Gotchas that bite generated code

- **`=` is `Ref` (immutable), not assignment.** Use `Set`/`Update`. This is the #1 error.
- **`And`/`Or` have flow-control side effects** — use them only inside predicate blocks
  (`If`/`When`/`Cond`), not in plain flow.
- **`Take` ≠ `Slice` ≠ `Limit`**: `Take(i)` = element at index; `Slice(a b)` = range;
  `Limit(n)` = first n.
- **Sub-blocks `{...}` preserve the input**: `1 | {Add(2)|Log} | Log` logs `3` then `1`.
- **Dynamic types need `Expect*`**: `ExpectString`, `ExpectTable`, … when the type isn't
  known at compose time.
- **Comments are `//`** (or `/* */`), never `;`.
- **Namespaces use `/`** (`fbl/set-tracked`); `::` is only the enum separator
  (`LogLevel::Info`).

## The loop — discover, verify, never guess

```sh
# 1. Discover a shard by capability
shards search http                 # matches name + summary
shards enumerate --filter CSV      # filter the full index
shards enumerate --json | jq ...   # composable

# 2. Read its exact signature (params, types, defaults) — ground truth, live
shards docs Http.Get --json
shards docs LogLevel --type enum --json

# 3. Write the .shs, then VERIFY before running (parse + compose; never executes)
shards check myscript.shs --json
#   exit 0 ok | 1 problems | 2 could-not-run
#   each diagnostic: phase (parse|construct|compose), line/column, message,
#   structured actual/expected types, and repair aids:
#     candidates    — bridge shards that fix a type mismatch
#     did_you_mean  — for an unknown shard name

# 4. Repair from the diagnostics, re-check until clean, then run
shards run myscript.shs
```

`check` resolves `@include`s relative to the file (`-I <dir>` adds roots). A fragment
that depends on definitions injected by an outer file should be checked via that
entry-point file, not in isolation.

## Quick reference

| Command | Purpose |
|---|---|
| `shards enumerate [--filter S] [--json]` | one-line index of all shards |
| `shards search <query> [--json]` | keyword search over name + summary |
| `shards docs <name> [--type shard\|enum] [--json]` | full signature (drill-down) |
| `shards check <file> [--json]` | type-check (parse + compose), never runs |
| `shards run <file>` | execute |
| `shards format <file> [-i]` | format source |
| `shards ast <file>` | dump the JSON AST |

When in doubt: `enumerate`/`search` to find it, `docs --json` to learn it, `check` to
prove it. The binary is the source of truth.
