# `shards check` eval harness

The measuring stick for diagnostic quality. For each script in `cases/`, the runner
invokes `shards check --json` and asserts that the diagnostics carry what an agent
repair loop needs: the right phase/kind, a precise location, structured
`actual`/`expected` types, and the repair aids (`candidates`, `did_you_mean`).

It also reports a **repairability** score — the fraction of broken cases whose
diagnostic shipped an actionable repair aid. This is the signal roadmap items 3.3
(canonical-form benchmark) and 3.4 (synthetic corpus) build on.

## Run

```sh
shards/tests/check/run.sh           # auto-detects build/Release or build/Debug
SHARDS=/path/to/shards shards/tests/check/run.sh
```

Requires `jq`. Exit code is non-zero if any assertion fails.

## Cases

| file | phase | kind | repair aid |
|---|---|---|---|
| `ok-basic.shs` | — | — | valid; composes cleanly (exit 0) |
| `type-mismatch.shs` | compose | input-type-mismatch | type-directed `candidates` |
| `unknown-shard.shs` | construct | unknown-shard | `did_you_mean` |
| `define.shs` | construct | — | parity: `check file.shs key:value` injects defines like `run` |
| `inc/sub/main.shs` | parse | — | parity: relative `-I` resolves against the invocation cwd like `run` |
| `syntax-error.shs` | parse | syntax | precise location |

All three phases (parse, construct, compose) carry precise `line`/`column`.

`check` accepts the same trailing `key:value` defines as `run` (e.g.
`shards check app.shs env:prod -I lib`), so a script that references command-line
defines — or whose composition branches on one — is checked in the configuration it
will actually run in. `define.shs` exercises both states (missing → construct error,
supplied → clean).

Include resolution also matches `run`: a relative `-I` (e.g. `-I.`) is canonicalized
against the invocation cwd, *before* the root path is set (setting the root path
changes the process cwd). `inc/sub/main.shs` includes a `lib.shs` reachable only via
`-I`, exercised from a different cwd to guard against the relative-`-I` regression.
Multiple `-I` flags all accumulate. A `-I` that does not resolve is a hard error
(exit 2, message on stderr, stdout left clean), never a silent skip — a swallowed
include dir would otherwise masquerade as "this `-I` wasn't honored".

## Candidate quality

The candidates engine matches on top-level `basicType` only, so an input-type
mismatch may include type-valid-but-semantically-odd bridges alongside the obvious
ones (e.g. `ParseInt`/`ParseFloat` for `String -> Int`). Inner seq/table element
matching could sharpen this later.
