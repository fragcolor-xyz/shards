# Shards 2.0: Compose Once, Instantiate Many

**This document moved** to the Shards 2.0 repository: `sinkingsugar/shards2`, at [`docs/shards-2-compose-split.md`](https://github.com/sinkingsugar/shards2/blob/main/docs/shards-2-compose-split.md) (private while the core design is being prototyped). Its last version in this repo is in git history at `2127b074e`.

Summary: Shards 2.0 is a new Rust runtime in which compose output becomes a shared, immutable artifact, separate from the small runtime state each instance owns. In 1.x, every copy of a wire (`Spawn`, `Expand`, server connections) is rebuilt and recomposed.

Status (2026-10-04): the design is validated by a prototype in `sinkingsugar/shards2`. The stackless scheduler is the default and the stackful one is maintained alongside it; see that repo's `docs/stackless-experiment.md` for the results and the matched comparisons with 1.x.

The CPU-only 1.x baseline for that work lives in this repo: [`shards/tests/bench-instances.sh`](../shards/tests/bench-instances.sh) (see the 2.0 doc, §5).
