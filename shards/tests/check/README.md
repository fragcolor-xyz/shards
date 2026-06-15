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
| `syntax-error.shs` | parse | syntax | precise location |

All three phases (parse, construct, compose) carry precise `line`/`column`.

## Candidate quality

The candidates engine matches on top-level `basicType` only, so an input-type
mismatch may include type-valid-but-semantically-odd bridges alongside the obvious
ones (e.g. `ParseInt`/`ParseFloat` for `String -> Int`). Inner seq/table element
matching could sharpen this later.
