from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from shardsbench.core import (
    BenchmarkError,
    SENTINEL_FILE,
    _sentinel_lines,
    _sentinel_reached,
    _solution_function_names,
    _toplevel_violations,
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
                    "contract_at_1": True,
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
                    "contract_at_1": False,
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


class ContractTests(unittest.TestCase):
    # Mirrors the real `shards ast` shape: a top-level sequence of pipelines,
    # each pipeline a list of blocks keyed by content kind plus line_info.
    def test_accepts_definition_only_candidates(self) -> None:
        ast = {
            "sequence": [
                [
                    {
                        "func": {"name": "wire", "params": [{"id": {"name": "solution"}}]},
                        "line_info": {"line": 1, "column": 1},
                    }
                ],
                [
                    {
                        "func": {"name": "define", "params": [{"id": {"name": "x"}}]},
                        "line_info": {"line": 4, "column": 1},
                    }
                ],
            ]
        }
        self.assertEqual(_toplevel_violations(ast), [])

    def test_flags_toplevel_shard_statements(self) -> None:
        ast = {
            "sequence": [
                [
                    {
                        "func": {"name": "wire", "params": [{"id": {"name": "solution"}}]},
                        "line_info": {"line": 1, "column": 1},
                    }
                ],
                [
                    {"const": {"num": {"int": 0}}, "line_info": {"line": 5, "column": 1}},
                    {"sh": {"name": "Stop"}, "line_info": {"line": 5, "column": 5}},
                ],
            ]
        }
        violations = _toplevel_violations(ast)
        self.assertEqual(
            [(v["kind"], v["line"]) for v in violations],
            [("const", 5), ("sh", 5)],
        )

    def test_flags_disallowed_funcs_and_malformed_input(self) -> None:
        ast = {
            "sequence": [
                [
                    {
                        "func": {"name": "schedule", "params": []},
                        "line_info": {"line": 2, "column": 1},
                    }
                ]
            ]
        }
        self.assertEqual(
            _toplevel_violations(ast),
            [{"kind": "func", "name": "schedule", "line": 2}],
        )
        self.assertTrue(_toplevel_violations("not an ast"))
        self.assertTrue(_toplevel_violations({"sequence": "bogus"}))


class SentinelTests(unittest.TestCase):
    def test_sentinel_lines_never_embed_a_token(self) -> None:
        lines = _sentinel_lines("shardsbench-sentinel-cafe0123")
        self.assertIn(SENTINEL_FILE, lines)
        self.assertIn("@define(shardsbench-sentinel-cafe0123", lines)
        self.assertIn("IgnoreRedefined: true", lines)
        self.assertIn("Contents: @shardsbench-sentinel-cafe0123", lines)
        self.assertIn("FS.Write", lines)

    def test_sentinel_reached_requires_exact_token(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            workspace = Path(temp)
            self.assertFalse(_sentinel_reached(workspace, "token"))
            (workspace / SENTINEL_FILE).write_text("other", encoding="utf-8")
            self.assertFalse(_sentinel_reached(workspace, "token"))
            (workspace / SENTINEL_FILE).write_text("token", encoding="utf-8")
            self.assertTrue(_sentinel_reached(workspace, "token"))


if __name__ == "__main__":
    unittest.main()
