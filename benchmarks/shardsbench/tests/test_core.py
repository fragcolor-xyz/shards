from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from shardsbench.core import (
    BenchmarkError,
    _solution_function_names,
    aggregate,
    discover_tasks,
    extract_candidate,
)


class CandidateExtractionTests(unittest.TestCase):
    def test_prefers_labeled_shards_fence(self) -> None:
        source, mode = extract_candidate(
            "Explanation\n```text\nignore\n```\n```shards\n@wire(solution {Mul(2)})\n```"
        )
        self.assertEqual(source, "@wire(solution {Mul(2)})\n")
        self.assertEqual(mode, "shards_fence")

    def test_accepts_raw_source(self) -> None:
        source, mode = extract_candidate("  @wire(solution {Add(1)})  ")
        self.assertEqual(source, "@wire(solution {Add(1)})\n")
        self.assertEqual(mode, "raw")


class DiscoveryTests(unittest.TestCase):
    def test_rejects_duplicate_ids(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for name in ("a", "b"):
                task = root / name
                task.mkdir()
                manifest = {
                    "schema_version": 1,
                    "id": "duplicate",
                    "title": name,
                    "track": "generation",
                    "difficulty": "easy",
                    "tags": []
                }
                (task / "task.json").write_text(json.dumps(manifest), encoding="utf-8")
                for filename in ("prompt.md", "tests.shs", "reference.shs"):
                    (task / filename).write_text("placeholder", encoding="utf-8")
            with self.assertRaises(BenchmarkError):
                discover_tasks(root)


class AggregationTests(unittest.TestCase):
    def test_aggregates_stage_rates(self) -> None:
        result = aggregate(
            [
                {
                    "status": "passed",
                    "track": "generation",
                    "difficulty": "easy",
                    "candidate_at_1": True,
                    "parse_at_1": True,
                    "construct_at_1": True,
                    "compose_at_1": True,
                    "requirements_at_1": True,
                    "pass_at_1": True,
                },
                {
                    "status": "check_failed",
                    "track": "repair",
                    "difficulty": "easy",
                    "candidate_at_1": True,
                    "parse_at_1": True,
                    "construct_at_1": False,
                    "compose_at_1": False,
                    "requirements_at_1": False,
                    "pass_at_1": False,
                },
            ]
        )
        self.assertEqual(result["candidate_at_1"], 1.0)
        self.assertEqual(result["parse_at_1"], 1.0)
        self.assertEqual(result["construct_at_1"], 0.5)
        self.assertEqual(result["pass_at_1"], 0.5)
        self.assertEqual(result["by_track"]["generation"]["pass_at_1"], 1.0)
        self.assertEqual(result["by_track"]["repair"]["pass_at_1"], 0.0)


class AstRequirementTests(unittest.TestCase):
    def test_collects_only_functions_inside_solution_wire(self) -> None:
        ast = {
            "sequence": [
                {"func": {"name": "wire", "params": [
                    {"id": {"name": "helper"}},
                    {"shards": [{"func": {"name": "Wrong", "params": []}}]},
                ]}},
                {"func": {"name": "wire", "params": [
                    {"id": {"name": "solution"}},
                    {"shards": [
                        {"sh": {"name": "GFX.MainWindow", "params": [
                            {"sh": {"name": "UI.Label", "params": []}}
                        ]}}
                    ]},
                ]}},
            ]
        }
        self.assertEqual(
            _solution_function_names(ast), ["GFX.MainWindow", "UI.Label"]
        )


if __name__ == "__main__":
    unittest.main()
