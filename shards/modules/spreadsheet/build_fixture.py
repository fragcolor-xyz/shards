#!/usr/bin/env -S uv run --quiet --with openpyxl python3
"""
Generate the test fixture spreadsheet for the Spreadsheet module.

Run with: uv run --with openpyxl python3 build_fixture.py
or:       ./build_fixture.py     (uv interprets the shebang)

Produces shards/tests/fixtures/spreadsheet_test.xlsx with:

  Sheet "People":
    A1: Name      B1: Age   C1: Active   D1: Joined
    A2: Alice     B2: 30    C2: TRUE     D2: 2024-01-15
    A3: Bob       B3: 25    C3: FALSE    D3: 2025-06-01
    A4: Charlie   B4: 42    C4: TRUE     D4: <empty>

  Sheet "Sparse":
    deliberately sparse with gaps and a duplicate header to exercise both layouts.

  Sheet "DupHeaders":
    A1: Score   B1: Score   C1: <empty>     -> exercises header dedup
    A2: 1       B2: 2       C2: 3
"""

from openpyxl import Workbook
from datetime import datetime
from pathlib import Path

OUT = Path(__file__).parent.parent.parent / "tests" / "data" / "spreadsheet_test.xlsx"

wb = Workbook()

# ---- Sheet 1: People ----
ws = wb.active
ws.title = "People"
ws.append(["Name", "Age", "Active", "Joined"])
ws.append(["Alice", 30, True, datetime(2024, 1, 15, 9, 0, 0)])
ws.append(["Bob", 25, False, datetime(2025, 6, 1, 14, 30, 0)])
ws.append(["Charlie", 42, True, None])

# ---- Sheet 2: Sparse ----
ws = wb.create_sheet("Sparse")
# Row 1 is header-ish but with a gap
ws["A1"] = "Item"
ws["C1"] = "Price"
# Row 3 (skip 2) populated only at A and C
ws["A3"] = "Widget"
ws["C3"] = 9.99
# Row 5 (skip 4) only middle column
ws["B5"] = "lonely"

# ---- Sheet 3: DupHeaders ----
ws = wb.create_sheet("DupHeaders")
ws.append(["Score", "Score", None])
ws.append([1, 2, 3])

OUT.parent.mkdir(parents=True, exist_ok=True)
wb.save(OUT)
print(f"Wrote {OUT}")
