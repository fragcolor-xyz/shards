"""Command-line interface for ShardsBench."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from .core import BenchmarkError, Task, discover_tasks, resolve_shards, score_tasks


BENCH_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = BENCH_ROOT.parents[1]
DEFAULT_SUITE = "dev"


def _tasks_root(args: argparse.Namespace) -> Path:
    if args.tasks_root:
        return Path(args.tasks_root).expanduser().resolve()
    return BENCH_ROOT / "tasks" / args.suite


def _task_record(task: Task) -> dict[str, Any]:
    return {
        "id": task.id,
        "title": task.title,
        "track": task.track,
        "difficulty": task.difficulty,
        "tags": list(task.tags),
        "required_shards": list(task.required_shards),
        "timeout_seconds": task.timeout_seconds,
    }


def _render_task_prompt(task: Task) -> str:
    parts = [task.prompt]
    if task.starter is not None:
        parts.extend(["\nStarter program:\n", f"```shards\n{task.starter}\n```"])
    parts.append(
        "\nReturn only a complete Shards source file defining `@wire(solution { ... })`."
    )
    return "\n".join(parts).strip()


def _write_json(payload: dict[str, Any], output: str | None) -> None:
    rendered = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if output and output != "-":
        Path(output).expanduser().write_text(rendered, encoding="utf-8")
    else:
        sys.stdout.write(rendered)


def _print_summary(report: dict[str, Any]) -> None:
    summary = report["summary"]
    print(
        "ShardsBench "
        f"{report['benchmark']['suite']}: {summary['status_counts'].get('passed', 0)}"
        f"/{summary['task_count']} passed"
    )
    for key in (
        "candidate_at_1",
        "parse_at_1",
        "construct_at_1",
        "compose_at_1",
        "requirements_at_1",
        "pass_at_1",
    ):
        print(f"  {key:16} {summary[key]:.3f}")
    failures = [result for result in report["tasks"] if not result["pass_at_1"]]
    for result in failures:
        print(f"  FAIL {result['id']}: {result['status']}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="shardsbench",
        description="Deterministically grade model-generated Shards programs.",
    )
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--suite", default=DEFAULT_SUITE)
    common.add_argument("--tasks-root")

    subparsers = parser.add_subparsers(dest="command", required=True)

    list_parser = subparsers.add_parser("list", parents=[common])
    list_parser.add_argument("--json", action="store_true")

    export_parser = subparsers.add_parser("export", parents=[common])
    export_parser.add_argument("--output", default="-")

    validate_parser = subparsers.add_parser("validate", parents=[common])
    validate_parser.add_argument("--shards")
    validate_parser.add_argument("--output")

    score_parser = subparsers.add_parser("score", parents=[common])
    score_parser.add_argument("answers")
    score_parser.add_argument("--shards")
    score_parser.add_argument("--model")
    score_parser.add_argument("--run-name")
    score_parser.add_argument("--output")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        tasks = discover_tasks(_tasks_root(args))

        if args.command == "list":
            records = [_task_record(task) for task in tasks]
            if args.json:
                print(json.dumps(records, indent=2, sort_keys=True))
            else:
                for task in tasks:
                    print(
                        f"{task.id:28} {task.track:10} {task.difficulty:8} {task.title}"
                    )
            return 0

        if args.command == "export":
            lines = [
                json.dumps(
                    {**_task_record(task), "prompt": _render_task_prompt(task)},
                    sort_keys=True,
                )
                for task in tasks
            ]
            rendered = "\n".join(lines) + "\n"
            if args.output == "-":
                sys.stdout.write(rendered)
            else:
                Path(args.output).expanduser().write_text(rendered, encoding="utf-8")
            return 0

        shards = resolve_shards(REPO_ROOT, args.shards)
        if args.command == "validate":
            report = score_tasks(
                tasks,
                lambda task: task.reference_path,
                shards,
                REPO_ROOT,
                args.suite,
                model="reference",
                run_name="suite-validation",
            )
        else:
            answers = Path(args.answers).expanduser().resolve()
            report = score_tasks(
                tasks,
                lambda task: answers / f"{task.id}.shs",
                shards,
                REPO_ROOT,
                args.suite,
                model=args.model,
                run_name=args.run_name,
            )

        if args.output:
            _write_json(report, args.output)
        _print_summary(report)
        return 0 if report["summary"]["pass_at_1"] == 1.0 else 1
    except BenchmarkError as error:
        parser.error(str(error))
    return 2
