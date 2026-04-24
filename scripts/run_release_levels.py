from __future__ import annotations

import argparse
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SOLVER_EXE = REPO_ROOT / "build" / "Release" / "rail_repair_solver.exe"


@dataclass
class LevelResult:
    level: int
    return_code: int
    elapsed_seconds: float


def level_path(level: int) -> Path:
    return REPO_ROOT / "levels" / f"level{level}.txt"


def run_level(level: int, timeout: float | None) -> LevelResult:
    start = time.perf_counter()
    completed = subprocess.run(
        [str(SOLVER_EXE), str(level)],
        text=True,
        timeout=timeout,
        check=False,
    )
    elapsed_seconds = time.perf_counter() - start
    return LevelResult(
        level=level,
        return_code=completed.returncode,
        elapsed_seconds=elapsed_seconds,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the release solver for levels 1 through N."
    )
    parser.add_argument("max_level", type=int, help="Run levels 1 through this number.")
    parser.add_argument(
        "--timeout",
        type=float,
        default=None,
        help="Optional timeout in seconds per level.",
    )
    args = parser.parse_args()
    
    if args.max_level < 1:
        parser.error("max_level must be at least 1")

    missing_levels = [level for level in range(1, args.max_level + 1) if not level_path(level).is_file()]
    if missing_levels:
        joined = ", ".join(str(level) for level in missing_levels)
        parser.error(f"Missing level files for: {joined}")

    results: list[LevelResult] = []
    print(f"Running levels 1..{args.max_level}\n")

    for level in range(1, args.max_level + 1):
        try:
            result = run_level(level, args.timeout)
        except subprocess.TimeoutExpired:
            print(f"level {level:>2}: TIMEOUT")
            return 1

        results.append(result)
        status = "OK" if result.return_code == 0 else "FAIL"
        print(f"level {level:>2}: {status:4} {result.elapsed_seconds:9.3f}s\n")

    total_seconds = sum(result.elapsed_seconds for result in results)
    failures = [result.level for result in results if result.return_code != 0]

    print(f"Total wall time: {total_seconds:.3f}s")
    print(f"Successful levels: {len(results) - len(failures)}/{len(results)}")
    if failures:
        failed_list = ", ".join(str(level) for level in failures)
        print(f"Failed levels: {failed_list}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
