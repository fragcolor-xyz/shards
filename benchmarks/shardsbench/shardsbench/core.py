"""Core task loading, Shards execution, and metric aggregation."""

from __future__ import annotations

import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import tempfile
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Iterable


SCHEMA_VERSION = 1
MAX_CAPTURE_CHARS = 20_000
FENCE_RE = re.compile(
    r"```(?P<label>[^\n`]*)\n(?P<body>.*?)```", re.DOTALL | re.IGNORECASE
)


class BenchmarkError(RuntimeError):
    """Raised for benchmark configuration or infrastructure failures."""


@dataclass(frozen=True)
class Task:
    id: str
    title: str
    track: str
    difficulty: str
    tags: tuple[str, ...]
    required_shards: tuple[str, ...]
    timeout_seconds: float
    directory: Path
    prompt_path: Path
    tests_paths: tuple[Path, ...]
    reference_path: Path
    starter_path: Path | None

    @property
    def prompt(self) -> str:
        return self.prompt_path.read_text(encoding="utf-8").strip()

    @property
    def starter(self) -> str | None:
        if self.starter_path is None:
            return None
        return self.starter_path.read_text(encoding="utf-8").rstrip()


def _require_string(data: dict[str, Any], key: str, manifest: Path) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value.strip():
        raise BenchmarkError(f"{manifest}: {key!r} must be a non-empty string")
    return value


def load_task(manifest: Path) -> Task:
    try:
        data = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise BenchmarkError(f"could not read {manifest}: {error}") from error

    if not isinstance(data, dict):
        raise BenchmarkError(f"{manifest}: manifest must be a JSON object")
    if data.get("schema_version") != SCHEMA_VERSION:
        raise BenchmarkError(
            f"{manifest}: schema_version must be {SCHEMA_VERSION}"
        )

    task_id = _require_string(data, "id", manifest)
    if task_id.startswith("/") or ".." in Path(task_id).parts:
        raise BenchmarkError(f"{manifest}: id must be a safe relative path")

    tags = data.get("tags", [])
    if not isinstance(tags, list) or not all(isinstance(tag, str) for tag in tags):
        raise BenchmarkError(f"{manifest}: tags must be an array of strings")

    requirements = data.get("requirements", {})
    if not isinstance(requirements, dict):
        raise BenchmarkError(f"{manifest}: requirements must be an object")
    required_shards = requirements.get("shards", [])
    if not isinstance(required_shards, list) or not all(
        isinstance(name, str) and name for name in required_shards
    ):
        raise BenchmarkError(
            f"{manifest}: requirements.shards must be an array of non-empty strings"
        )

    timeout_seconds = data.get("timeout_seconds", 5)
    if not isinstance(timeout_seconds, (int, float)) or timeout_seconds <= 0:
        raise BenchmarkError(f"{manifest}: timeout_seconds must be positive")

    directory = manifest.parent
    prompt_path = directory / data.get("prompt", "prompt.md")
    tests_value = data.get("tests", "tests.shs")
    if isinstance(tests_value, str):
        tests_names = [tests_value]
    elif (
        isinstance(tests_value, list)
        and tests_value
        and all(isinstance(name, str) for name in tests_value)
    ):
        tests_names = tests_value
    else:
        raise BenchmarkError(f"{manifest}: tests must be a string or non-empty string array")
    tests_paths = tuple(directory / name for name in tests_names)
    reference_path = directory / data.get("reference", "reference.shs")
    starter_name = data.get("starter")
    starter_path = directory / starter_name if isinstance(starter_name, str) else None

    for label, path in (("prompt", prompt_path), ("reference", reference_path)):
        if not path.is_file():
            raise BenchmarkError(f"{manifest}: {label} file does not exist: {path}")
    for path in tests_paths:
        if not path.is_file():
            raise BenchmarkError(f"{manifest}: tests file does not exist: {path}")
    if starter_path is not None and not starter_path.is_file():
        raise BenchmarkError(f"{manifest}: starter file does not exist: {starter_path}")

    return Task(
        id=task_id,
        title=_require_string(data, "title", manifest),
        track=_require_string(data, "track", manifest),
        difficulty=_require_string(data, "difficulty", manifest),
        tags=tuple(tags),
        required_shards=tuple(required_shards),
        timeout_seconds=float(timeout_seconds),
        directory=directory,
        prompt_path=prompt_path,
        tests_paths=tests_paths,
        reference_path=reference_path,
        starter_path=starter_path,
    )


def discover_tasks(tasks_root: Path) -> list[Task]:
    if not tasks_root.is_dir():
        raise BenchmarkError(f"task root does not exist: {tasks_root}")
    tasks = [load_task(path) for path in sorted(tasks_root.rglob("task.json"))]
    if not tasks:
        raise BenchmarkError(f"no task.json manifests found under {tasks_root}")
    ids = [task.id for task in tasks]
    duplicates = sorted({task_id for task_id in ids if ids.count(task_id) > 1})
    if duplicates:
        raise BenchmarkError(f"duplicate task IDs: {', '.join(duplicates)}")
    return sorted(tasks, key=lambda task: task.id)


def extract_candidate(raw: str) -> tuple[str, str]:
    """Extract Shards from a raw answer and return (source, extraction mode)."""
    matches = list(FENCE_RE.finditer(raw))
    for match in matches:
        label = match.group("label").strip().lower()
        if label in {"shards", "shs"}:
            return match.group("body").strip() + "\n", f"{label}_fence"
    if len(matches) == 1:
        return matches[0].group("body").strip() + "\n", "single_fence"
    source = raw.strip()
    return (source + "\n" if source else ""), "raw"


def resolve_shards(repo_root: Path, override: str | None = None) -> Path:
    candidates: list[Path] = []
    if override:
        candidates.append(Path(override).expanduser())
    if os.environ.get("SHARDS"):
        candidates.append(Path(os.environ["SHARDS"]).expanduser())
    candidates.extend(
        [repo_root / "build/Release/shards", repo_root / "build/Debug/shards"]
    )
    on_path = shutil.which("shards")
    if on_path:
        candidates.append(Path(on_path))

    for candidate in candidates:
        candidate = candidate.resolve()
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    attempted = ", ".join(str(path) for path in candidates) or "(none)"
    raise BenchmarkError(f"could not find an executable shards binary; tried {attempted}")


def _bounded(text: str) -> tuple[str, bool]:
    if len(text) <= MAX_CAPTURE_CHARS:
        return text, False
    return text[:MAX_CAPTURE_CHARS], True


def _run(
    command: list[str], cwd: Path, timeout_seconds: float
) -> dict[str, Any]:
    started = time.monotonic()
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout_seconds,
            check=False,
        )
        stdout, stdout_truncated = _bounded(completed.stdout)
        stderr, stderr_truncated = _bounded(completed.stderr)
        return {
            "exit_code": completed.returncode,
            "timed_out": False,
            "duration_seconds": round(time.monotonic() - started, 6),
            "stdout": stdout,
            "stderr": stderr,
            "stdout_truncated": stdout_truncated,
            "stderr_truncated": stderr_truncated,
        }
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout or ""
        stderr = error.stderr or ""
        if isinstance(stdout, bytes):
            stdout = stdout.decode(errors="replace")
        if isinstance(stderr, bytes):
            stderr = stderr.decode(errors="replace")
        stdout, stdout_truncated = _bounded(stdout)
        stderr, stderr_truncated = _bounded(stderr)
        return {
            "exit_code": None,
            "timed_out": True,
            "duration_seconds": round(time.monotonic() - started, 6),
            "stdout": stdout,
            "stderr": stderr,
            "stdout_truncated": stdout_truncated,
            "stderr_truncated": stderr_truncated,
        }


def _diagnostics(check_result: dict[str, Any]) -> list[dict[str, Any]]:
    try:
        payload = json.loads(check_result["stdout"])
    except (json.JSONDecodeError, TypeError):
        return []
    diagnostics = payload.get("diagnostics", []) if isinstance(payload, dict) else []
    return diagnostics if isinstance(diagnostics, list) else []


def _copy_task_assets(task: Task, tests_path: Path, workspace: Path) -> None:
    shutil.copy2(tests_path, workspace / "tests.shs")
    assets = task.directory / "assets"
    if assets.is_dir():
        shutil.copytree(assets, workspace / "assets")


def _solution_function_names(ast: Any) -> list[str]:
    """Return function names contained by the `solution` wire in a Shards AST."""

    def visit(node: Any) -> list[str] | None:
        if isinstance(node, dict):
            function = node.get("func")
            if isinstance(function, dict) and function.get("name") == "wire":
                params = function.get("params", [])
                if (
                    isinstance(params, list)
                    and len(params) >= 2
                    and isinstance(params[0], dict)
                    and params[0].get("id", {}).get("name") == "solution"
                ):
                    names: list[str] = []

                    def collect(value: Any) -> None:
                        if isinstance(value, dict):
                            for key in ("sh", "func"):
                                nested = value.get(key)
                                if isinstance(nested, dict) and isinstance(
                                    nested.get("name"), str
                                ):
                                    names.append(nested["name"])
                            for child in value.values():
                                collect(child)
                        elif isinstance(value, list):
                            for child in value:
                                collect(child)

                    collect(params[1])
                    return names
            for child in node.values():
                found = visit(child)
                if found is not None:
                    return found
        elif isinstance(node, list):
            for child in node:
                found = visit(child)
                if found is not None:
                    return found
        return None

    return visit(ast) or []


def _check_requirements(
    task: Task, workspace: Path, shards: Path
) -> tuple[bool, dict[str, Any]]:
    if not task.required_shards:
        return True, {"required_shards": [], "missing_shards": []}

    ast_path = workspace / "candidate.ast.json"
    command = _run(
        [str(shards), "ast", "candidate.shs", "-o", str(ast_path)],
        workspace,
        task.timeout_seconds,
    )
    if command["timed_out"] or command["exit_code"] != 0 or not ast_path.is_file():
        return False, {
            "required_shards": list(task.required_shards),
            "missing_shards": list(task.required_shards),
            "ast_command": command,
        }
    try:
        ast = json.loads(ast_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return False, {
            "required_shards": list(task.required_shards),
            "missing_shards": list(task.required_shards),
            "error": str(error),
            "ast_command": command,
        }
    found = _solution_function_names(ast)
    found_set = set(found)
    missing = [name for name in task.required_shards if name not in found_set]
    return not missing, {
        "required_shards": list(task.required_shards),
        "found_shards": sorted(found_set),
        "missing_shards": missing,
        "ast_command": command,
    }


def score_task(task: Task, candidate: Path | None, shards: Path) -> dict[str, Any]:
    base: dict[str, Any] = {
        "id": task.id,
        "title": task.title,
        "track": task.track,
        "difficulty": task.difficulty,
        "tags": list(task.tags),
        "candidate_at_1": False,
        "parse_at_1": False,
        "construct_at_1": False,
        "compose_at_1": False,
        "requirements_at_1": False,
        "pass_at_1": False,
    }
    if candidate is None or not candidate.is_file():
        return {**base, "status": "missing_candidate"}

    try:
        raw = candidate.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as error:
        return {**base, "status": "unreadable_candidate", "error": str(error)}
    source, extraction_mode = extract_candidate(raw)
    if not source.strip():
        return {
            **base,
            "status": "empty_candidate",
            "extraction_mode": extraction_mode,
        }
    base["candidate_at_1"] = True
    base["extraction_mode"] = extraction_mode

    with tempfile.TemporaryDirectory(prefix="shardsbench-") as temp:
        workspaces: list[tuple[Path, Path]] = []
        checks: list[dict[str, Any]] = []
        all_phases: set[Any] = set()
        for index, tests_path in enumerate(task.tests_paths):
            workspace = Path(temp) / f"case-{index:03d}-{tests_path.stem}"
            workspace.mkdir()
            _copy_task_assets(task, tests_path, workspace)
            (workspace / "candidate.shs").write_text(source, encoding="utf-8")
            workspaces.append((tests_path, workspace))

            check = _run(
                [str(shards), "check", "--json", "tests.shs"],
                workspace,
                task.timeout_seconds,
            )
            diagnostics = _diagnostics(check)
            all_phases.update(
                diagnostic.get("phase")
                for diagnostic in diagnostics
                if isinstance(diagnostic, dict)
            )
            checks.append(
                {"case": tests_path.name, **check, "diagnostics": diagnostics}
            )

        base["parse_at_1"] = not any(check["timed_out"] for check in checks) and (
            "parse" not in all_phases
        )
        base["construct_at_1"] = base["parse_at_1"] and "construct" not in all_phases
        base["compose_at_1"] = all(check["exit_code"] == 0 for check in checks)
        base["checks"] = checks

        if any(check["timed_out"] for check in checks):
            return {**base, "status": "check_timeout"}
        if any(check["exit_code"] == 2 for check in checks):
            return {**base, "status": "check_infrastructure_error"}
        if not base["compose_at_1"]:
            return {**base, "status": "check_failed"}

        requirements_ok, requirements = _check_requirements(
            task, workspaces[0][1], shards
        )
        base["requirements_at_1"] = requirements_ok
        base["requirements"] = requirements
        if not requirements_ok:
            if requirements.get("ast_command", {}).get("timed_out"):
                return {**base, "status": "requirements_timeout"}
            if requirements.get("missing_shards"):
                return {**base, "status": "requirements_failed"}
            return {**base, "status": "requirements_infrastructure_error"}

        runs: list[dict[str, Any]] = []
        for tests_path, workspace in workspaces:
            run = _run(
                [str(shards), "run", "tests.shs"],
                workspace,
                task.timeout_seconds,
            )
            runs.append({"case": tests_path.name, **run})
        base["runs"] = runs
        if any(run["timed_out"] for run in runs):
            return {**base, "status": "run_timeout"}
        if any(run["exit_code"] != 0 for run in runs):
            return {**base, "status": "tests_failed"}
        base["pass_at_1"] = True
        return {**base, "status": "passed"}


def _rate(results: list[dict[str, Any]], key: str) -> float:
    if not results:
        return 0.0
    return round(sum(bool(result[key]) for result in results) / len(results), 6)


def _aggregate_rates(results: list[dict[str, Any]]) -> dict[str, Any]:
    status_counts: dict[str, int] = {}
    for result in results:
        status = str(result["status"])
        status_counts[status] = status_counts.get(status, 0) + 1
    return {
        "task_count": len(results),
        "candidate_at_1": _rate(results, "candidate_at_1"),
        "parse_at_1": _rate(results, "parse_at_1"),
        "construct_at_1": _rate(results, "construct_at_1"),
        "compose_at_1": _rate(results, "compose_at_1"),
        "requirements_at_1": _rate(results, "requirements_at_1"),
        "pass_at_1": _rate(results, "pass_at_1"),
        "status_counts": dict(sorted(status_counts.items())),
    }


def aggregate(results: list[dict[str, Any]]) -> dict[str, Any]:
    diagnostic_phases: dict[str, int] = {}
    diagnostic_kinds: dict[str, int] = {}
    for result in results:
        for check in result.get("checks", []):
            for diagnostic in check.get("diagnostics", []):
                if not isinstance(diagnostic, dict):
                    continue
                phase = str(diagnostic.get("phase", "unknown"))
                kind = str(diagnostic.get("kind", "unknown"))
                diagnostic_phases[phase] = diagnostic_phases.get(phase, 0) + 1
                diagnostic_kinds[kind] = diagnostic_kinds.get(kind, 0) + 1

    def grouped(field: str) -> dict[str, dict[str, Any]]:
        values = sorted({str(result.get(field, "unknown")) for result in results})
        return {
            value: _aggregate_rates(
                [result for result in results if str(result.get(field, "unknown")) == value]
            )
            for value in values
        }

    return {
        **_aggregate_rates(results),
        "diagnostics": {
            "by_phase": dict(sorted(diagnostic_phases.items())),
            "by_kind": dict(sorted(diagnostic_kinds.items())),
        },
        "by_track": grouped("track"),
        "by_difficulty": grouped("difficulty"),
    }


def _git_commit(repo_root: Path) -> str | None:
    completed = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=repo_root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return completed.stdout.strip() if completed.returncode == 0 else None


def _catalog_fingerprint(shards: Path, repo_root: Path) -> dict[str, Any] | None:
    completed = subprocess.run(
        [str(shards), "enumerate", "--json"],
        cwd=repo_root,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if completed.returncode != 0:
        return None
    try:
        catalog = json.loads(completed.stdout)
    except json.JSONDecodeError:
        return None
    return {
        "count": len(catalog) if isinstance(catalog, list) else None,
        "sha256": hashlib.sha256(completed.stdout).hexdigest(),
    }


def score_tasks(
    tasks: Iterable[Task],
    candidate_for: Callable[[Task], Path | None],
    shards: Path,
    repo_root: Path,
    suite: str,
    model: str | None = None,
    run_name: str | None = None,
) -> dict[str, Any]:
    task_list = list(tasks)
    results = [score_task(task, candidate_for(task), shards) for task in task_list]
    stat = shards.stat()
    return {
        "schema_version": SCHEMA_VERSION,
        "benchmark": {
            "name": "ShardsBench",
            "suite": suite,
            "git_commit": _git_commit(repo_root),
            "shards_binary": str(shards),
            "shards_binary_size": stat.st_size,
            "shards_binary_mtime_ns": stat.st_mtime_ns,
            "catalog": _catalog_fingerprint(shards, repo_root),
        },
        "run": {
            "name": run_name,
            "model": model,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "platform": platform.platform(),
            "python": platform.python_version(),
        },
        "summary": aggregate(results),
        "tasks": results,
    }
