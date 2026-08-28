# ShardsBench

ShardsBench is a deterministic benchmark for code-producing language models and
agents working in Shards. It grades generated programs through the same stages a
developer sees:

1. candidate extraction;
2. parsing and wire construction;
3. whole-program composition with `shards check --json`;
4. semantic assertions with `shards run`.

The initial `dev` suite is deliberately small. It is a vertical slice for validating
the task format, runner, metrics, and candidate contract before building the larger
held-out benchmark.

To evaluate a model rather than the reference answers, follow the complete
[code-generation run protocol](RUN.md). In particular, `validate` is maintainer
tooling; it does not invoke or benchmark a model.

## Candidate contract

Every current task asks for a file defining this wire:

```shards
@wire(solution {
  // The task input flows in here. Leave the result in the output flow.
})
```

The grader copies the candidate into an isolated temporary working directory,
includes it from `tests.shs`, calls `Do(solution)`, and checks the result with Shards
assertions. Candidates may be raw Shards or a Markdown fenced `shards`/`shs` block.

Because the candidate is included into the grading wire, its top-level statements
would execute before the assertions. The scorer therefore rejects any candidate whose
top level contains anything other than `@wire`, `@define`, and `@template` definitions
(status `contract_violation`).

## Quick start

From the repository root, benchmark maintainers can inspect/export tasks and verify
the graders against their reference answers:

```sh
python3 benchmarks/shardsbench/run.py list
python3 benchmarks/shardsbench/run.py export --output /tmp/shardsbench-prompts.jsonl
python3 benchmarks/shardsbench/run.py validate
```

This is a benchmark smoke test, not a model run. An actual model run exports prompts,
generates answers in an isolated environment, and then invokes `score` as specified in
`RUN.md`.

The opt-in extended suite contains larger application tasks and may open windows or
require graphics hardware:

```sh
python3 benchmarks/shardsbench/run.py list --suite extended
python3 benchmarks/shardsbench/run.py validate --suite extended
python3 benchmarks/shardsbench/run.py export \
  --suite extended \
  --output /tmp/shardsbench-extended-prompts.jsonl
```

`validate` grades each task's reference answer. To grade model outputs, place one
answer per task under a directory using the task ID as its relative path:

```text
answers/
  core/double-int.shs
  core/sum-int-sequence.shs
  ...
```

Then run:

```sh
python3 benchmarks/shardsbench/run.py score answers \
  --model example-model \
  --output /tmp/shardsbench-results.json
```

Use `--shards /path/to/shards` to override binary discovery. By default the runner
checks `SHARDS`, `build/Release/shards`, `build/Debug/shards`, and `PATH`, in that
order.

## Metrics

The JSON report includes strict `pass_at_1` plus diagnostic rates:

- `candidate_at_1`: an answer file existed and yielded source;
- `parse_at_1`: no parse diagnostic was emitted;
- `construct_at_1`: parsing and construction succeeded;
- `compose_at_1`: `shards check --json` accepted the complete grader;
- `contract_at_1`: the candidate's top level contains only allowed definitions;
- `requirements_at_1`: task-required shards occur inside the `solution` wire's AST;
- `pass_at_1`: all runtime assertions passed and the grader ran to completion.

Per-task records retain structured diagnostics, exit codes, durations, and bounded
stdout/stderr. The summary also breaks scores down by track and difficulty and counts
diagnostics by phase and kind. Runtime correctness is the headline metric; composing
is useful partial credit, not proof that the task was solved.

## Task layout

```text
tasks/dev/<task-id>/
  task.json       # metadata
  prompt.md       # user-facing problem
  starter.shs     # optional broken/starter program
  tests*.shs      # one or more dev graders; private in a held-out suite
  reference.shs   # maintainer reference answer
  assets/         # optional files copied into the grading workspace
```

Task manifests conform to `schema/task.schema.json`. A manifest's `tests` field may
name one grader or a list of graders; separate graders allow a wire to be composed
independently against otherwise-incompatible concrete input types. The runner also
performs dependency-free validation so it does not require a JSON Schema package.

App tasks can declare `requirements.shards`. These are checked against function calls
inside the `solution` wire in the canonical JSON AST. This prevents an empty/no-op wire
from receiving full credit merely because its outer grader starts successfully. AST
requirements complement runtime assertions; they are not a substitute for them.

## Security and benchmark integrity

Two gates keep a candidate from short-circuiting its grader:

1. **Top-level contract.** The candidate's JSON AST may contain only `@wire`,
   `@define`, and `@template` definitions at the top level. Anything else — a stray
   `Stop`, an `@include`, a `@schedule`/`@run`, any effectful statement — would run
   inside the grading wire before the assertions and is rejected outright.
2. **Grader-completion sentinel.** The scorer appends a final statement to each
   grader that writes a secret token to a sentinel file. The token is passed as a
   command-line define whose *name* is also randomized per run, so a frozen candidate
   can neither read the token from any file nor reference or shadow the define. A
   clean exit without a valid sentinel (for example a `Stop` issued inside
   `Do(solution)` that ends the grading wire) is reported as `grader_short_circuit`,
   not a pass.

These gates defeat accidental early exits and simple cheating. They are **not a
security sandbox**: `shards run` has no capability manifest, and a deliberately
malicious candidate still executes with the grader's full OS privileges (it could,
for instance, inspect its own process arguments through OS facilities). Only grade
trusted model output locally. Official or third-party submissions should be evaluated
inside a locked-down container or VM, and top-scoring candidates should be read by a
human before publication.

The checked-in `dev` graders are public smoke tests. A leaderboard-quality suite should
keep its grading pack private, publish only prompts and public examples, pin the Shards
commit/catalog fingerprint, and periodically add newly authored tasks.

## Suites

- `dev`: deterministic, headless core-language generation and repair tasks suitable
  for ordinary CI.
- `extended`: opt-in application tasks with platform requirements. The first task
  builds a bounded eight-frame dashboard containing a rotating GFX cube and composed
  UI panels/widgets. It checks the complete render/UI pipeline, required AST surface,
  and a live mesh observer that verifies the frame counter reaches exactly eight.

Keep platform-dependent task results separate from the portable headline score. A
machine that cannot initialize the required graphics/UI subsystem should report the
suite as unsupported, not count its tasks as model failures.

## Tests

```sh
python3 -m unittest discover \
  -s benchmarks/shardsbench \
  -t benchmarks/shardsbench \
  -v
python3 benchmarks/shardsbench/run.py validate
```
