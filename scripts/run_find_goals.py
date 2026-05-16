import argparse
import os
import subprocess
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
LEVELS_DIR = REPO_ROOT / "levels"


def get_binary(build_type: str) -> Path:
    """Get the binary for the specified build type (Debug or Release)."""
    exe = REPO_ROOT / "build" / build_type / f"rail_repair_solver{'.exe' if os.name == 'nt' else ''}"
    
    if not exe.exists():
        raise FileNotFoundError(f"{build_type} binary not found at {exe}")
    
    return exe


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the solver on all levels with --find-goals flag"
    )
    parser.add_argument(
        "build_type",
        default="Debug",
        nargs="?",
        choices=["Debug", "Release"],
        help="Build type to use",
    )
    parser.add_argument(
        "--save",
        action="store_true",
        help="Allow the solver to save its solution to its respective file."
    )
    args = parser.parse_args()
    
    solver_exe = get_binary(args.build_type)
    
    level_files = sorted(LEVELS_DIR.glob("level*.txt"))
    
    if not level_files:
        print(f"No level files found in {LEVELS_DIR}")
        return 1
    
    elapsed_time = 0
    
    for level_file in level_files:
        level_num = int(level_file.stem.replace("level", ""))
        command = [str(solver_exe), str(level_num), "--find-goals"]
        if not args.save:
            command.append("--no-save")
        
        start_time = time.perf_counter()
        subprocess.run(
            command,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=True,
        )
        elapsed_time += time.perf_counter() - start_time
    
    print(f"Completed all levels in {elapsed_time:.2f} seconds")
    
    return 0


if __name__ == "__main__":
    exit(main())
