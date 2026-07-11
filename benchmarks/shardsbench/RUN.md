# Running an honest ShardsBench code-generation evaluation

This document defines a **model run**. A run is a batch of independent code-generation
requests followed by one grading pass. It is not `shardsbench validate`: `validate`
only proves that maintainers' `reference.shs` files pass their graders.

The current runner intentionally separates generation from scoring. This keeps the
task/grader format provider-neutral and makes the isolation boundary explicit.

## 1. Choose and record one evaluation policy

Results are comparable only when they use the same suite, context policy, tool policy,
sampling settings, and number of samples.

Recommended policy names:

| Policy | Context given to the model | Tools during generation |
|---|---|---|
| `direct-none` | Task prompt only | None |
| `direct-skill` | `skills/shards/SKILL.md` plus task prompt | None |
| `direct-guide` | `SKILL.md`, `GUIDE.md`, plus task prompt | None |
| `agent-cli` | Shards skill plus task prompt | Clean `shards` CLI: `search`, `enumerate`, `docs`, `check`, and optionally `run` |

Do not mix these policies into one score. In particular, `agent-cli` measures an agent
and its repair loop, while the direct policies measure one-shot model generation.

Before generation, create a run manifest outside the model's workspace. At minimum,
record:

```json
{
  "run_id": "2026-07-11-example-model-dev-direct-skill-s01",
  "benchmark_commit": "full git commit SHA",
  "suite": "dev",
  "model": "provider/model-version",
  "model_revision": "exact revision or deployment ID when available",
  "context_policy": "direct-skill",
  "tool_policy": "none",
  "temperature": 0.0,
  "top_p": 1.0,
  "max_output_tokens": 4096,
  "samples_per_task": 1,
  "fresh_context_per_task": true,
  "system_prompt_sha256": "hash of the exact system/context text",
  "notes": "Any provider options or deviations"
}
```

Also retain the exact system prompt/context text. Model names such as “latest” are not
reproducible; resolve them to a dated or immutable model version where possible.

## 2. Freeze and export the prompt set

Run this from the Shards repository, before creating the model environment:

```sh
git rev-parse HEAD

python3 benchmarks/shardsbench/run.py export \
  --suite dev \
  --output /tmp/shardsbench-run/prompts.jsonl
```

For the opt-in UI/GFX application suite:

```sh
python3 benchmarks/shardsbench/run.py export \
  --suite extended \
  --output /tmp/shardsbench-run/extended-prompts.jsonl
```

Do not edit individual prompts after export. Hash and archive the exported JSONL with
the run manifest. Each line contains a stable task `id`, metadata, and the complete
user-facing `prompt`; repair tasks already include their starter source.

The checked-in `dev` and `extended` suites are public development fixtures. They can
support honest local experiments only if the evaluated model/agent is denied access to
this repository. They are not contamination-resistant leaderboard sets. An official
run requires a private task/grader pack.

## 3. Create the generation boundary

Generation must happen in a clean directory or container that does **not** contain:

- the Shards repository;
- `benchmarks/shardsbench/tasks`;
- any `reference.shs`;
- any `tests.shs` or private assets;
- earlier ShardsBench results or diagnostics.

Copy only these declared inputs into that environment:

1. the exported prompt JSONL;
2. the exact context files selected by the policy;
3. for `agent-cli`, a clean Shards binary and only the allowed CLI/tool wrapper.

Do not give a repository-aware coding agent the prompt while its working directory is
this checkout. It could read the public answers and graders even without being
explicitly instructed to do so.

For `agent-cli`, start every task in a fresh empty workspace. The model may create its
candidate and use the declared CLI commands, but the CLI must not be able to resolve
includes into the benchmark repository. Record every tool call and its output.

## 4. Generate answers

Process every JSONL record using the same fixed procedure:

1. Start a fresh conversation with no memory of previous tasks.
2. Install the selected system/context text.
3. Send exactly the record's `prompt` as the user message.
4. Apply the same model and sampling settings.
5. Capture the complete raw response and provider metadata.
6. Do not inspect grader results or retry based on hidden-test feedback.

The output contract asks for one complete Shards file defining:

```shards
@wire(solution {
  // generated implementation
})
```

Save one raw response per task using the task ID as its relative path:

```text
/tmp/shardsbench-run/answers/
  core/classify-sign.shs
  core/double-int.shs
  core/running-total.shs
  core/sum-int-sequence.shs
  core/uppercase-name.shs
  repair/parse-and-increment.shs
  repair/uppercase-shard.shs
```

For the extended task:

```text
/tmp/shardsbench-run/extended-answers/
  apps/ui-gfx-dashboard.shs
```

The files may contain raw Shards or the model's complete Markdown response. The scorer
extracts a labelled `shards`/`shs` fence, a single unlabelled fence, or otherwise treats
the entire file as source. Preserve the unmodified API response separately if the
provider returns reasoning, token counts, finish reasons, or other metadata that would
be lost in the answer file.

A missing answer, refusal, truncation, or empty response is a benchmark failure. Do not
repair it manually. If the protocol permits automatic retries for infrastructure
errors, define the retry rule before the run and apply it uniformly to every task.

## 5. End generation, then score once

The model or agent must no longer have access when graders are introduced. Copy the
answer directory to the scoring machine/environment and run:

```sh
python3 benchmarks/shardsbench/run.py score \
  /tmp/shardsbench-run/answers \
  --suite dev \
  --model provider/model-version \
  --run-name 2026-07-11-example-model-dev-direct-skill-s01 \
  --output /tmp/shardsbench-run/results.json
```

Score the UI/GFX suite separately because it has platform requirements and opens a
bounded test window:

```sh
python3 benchmarks/shardsbench/run.py score \
  /tmp/shardsbench-run/extended-answers \
  --suite extended \
  --model provider/model-version \
  --run-name 2026-07-11-example-model-extended-direct-skill-s01 \
  --output /tmp/shardsbench-run/extended-results.json
```

Scoring performs candidate extraction, `shards check --json`, the top-level candidate
contract (definitions only — see the README's integrity section), AST requirements,
runtime assertions, and a grader-completion sentinel that detects candidates ending
the grading wire before its assertions run. It records the benchmark commit, Shards
binary metadata, live shard catalog count/hash, platform, diagnostics, durations, and
per-task outcomes.

Do not send scoring diagnostics back to the model unless the declared track is an
interactive repair benchmark. One-shot code generation ends at the first response.

## 6. Multiple samples and `pass@k`

The current scorer accepts one candidate per task. For multiple samples, generate each
sample independently into a separate answer directory and score it as a separate run:

```text
runs/<run-id>/sample-001/answers/...
runs/<run-id>/sample-002/answers/...
runs/<run-id>/sample-003/answers/...
```

Do not score several candidates and report only the best one as `pass@1`. Report each
sample's `pass@1`; a future aggregation command will compute formal `pass@k` across the
saved runs. Use deterministic sample IDs and retain all unsuccessful generations.

## 7. Archive and report

An auditable run archive contains:

```text
run-directory/
  manifest.json
  system-prompt.txt
  prompts.jsonl
  answers/                 # exact responses consumed by the scorer
  raw-responses/           # provider-native response records
  transcripts/             # required for tool-assisted agents
  results.json
  stdout.log
  stderr.log
```

Publish or compare at least:

- strict `pass_at_1`;
- `parse_at_1`, `construct_at_1`, `compose_at_1`, and `requirements_at_1`;
- per-track and per-difficulty results;
- diagnostic histogram;
- model/context/tool policy;
- task and catalog fingerprints;
- token usage, latency, and cost when available;
- every deviation from this protocol.

Keep `dev` and `extended` results separate. A graphics initialization failure caused by
an unsupported evaluation host is an infrastructure result, not evidence about model
quality.

## Disallowed shortcuts

An honest code-generation run does not allow:

- model access to task references, graders, or the benchmark checkout;
- human edits to generated code;
- task-specific prompt changes;
- hidden-test feedback during one-shot generation;
- selecting among candidates using private grader results;
- carrying conversation state or learned solutions between tasks;
- undeclared tools, web searches, retrieval sources, or repair attempts;
- reporting maintainer `validate` results as model results.

For public fixtures, disclose possible training contamination. For official comparison,
use a private, versioned task pack and reveal its graders only after submissions are
frozen.

## What is still manual

ShardsBench currently exports prompts and deterministically scores answers, but it does
not call model-provider APIs. Provider adapters, automatic transcript capture, run
manifest generation, formal `pass@k` aggregation, and a private held-out task service
are the next orchestration layer. Until then, the procedure above is the normative way
to produce model outputs without exposing the graders.
