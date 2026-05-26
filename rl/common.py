from __future__ import annotations

import argparse
import os
import re
import subprocess
import time
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Iterable

import torch


REPO_ROOT = Path(__file__).resolve().parents[1]
LEVELS_DIR = REPO_ROOT / "levels"
SOLUTIONS_DIR = REPO_ROOT / "solutions"
DEFAULT_MODEL_PATH = REPO_ROOT / "rl_model.pt"
SOLVER_EXE = REPO_ROOT / "build" / "Release" / f"rail_repair_solver{'.exe' if os.name == 'nt' else ''}"

MAX_WIDTH = 10
MAX_HEIGHT = 10
ACTIONS = "rdlu"
ACTION_TO_INDEX = {action: index for index, action in enumerate(ACTIONS)}
DIRS = {"r": (1, 0), "d": (0, 1), "l": (-1, 0), "u": (0, -1)}
DIR_INDEX = {"r": 1, "d": 2, "l": 3, "u": 4}
INDEX_DIR = {value: key for key, value in DIR_INDEX.items()}
MOVABLE_TRACKS = set("hvlurd")
TRACK_CHARS = set("HVLURDhvlurd123")
LEVERS = set("123")
TRACK_TYPES = {"NW": 0, "NE": 1, "SE": 2, "SW": 3, "H": 4, "V": 5}
TRACK_TO_TYPE = {
    "U": "NW",
    "R": "NE",
    "D": "SE",
    "L": "SW",
    "H": "H",
    "V": "V",
    "u": "NW",
    "r": "NE",
    "d": "SE",
    "l": "SW",
    "h": "H",
    "v": "V",
    "1": "NW",
    "2": "NE",
    "3": "SE",
}

CHANNEL_WALL = 0
CHANNEL_HOLE = 1
CHANNEL_PLAYER = 2
CHANNEL_FIXED_TRACK = 3
CHANNEL_MOVABLE_TRACK = CHANNEL_FIXED_TRACK + 6
CHANNEL_LEVER = CHANNEL_MOVABLE_TRACK + 6
CHANNEL_LEVER_TOGGLED = CHANNEL_LEVER + 3
CHANNEL_COUNT = CHANNEL_LEVER_TOGGLED + 3


def add_pos(pos: tuple[int, int], action: str) -> tuple[int, int]:
    dx, dy = DIRS[action]
    return pos[0] + dx, pos[1] + dy


def is_edge_pos(x: int, y: int, width: int, height: int) -> bool:
    return x == 0 or y == 0 or x == width - 1 or y == height - 1


def ride(cell: str, in_dir: str) -> str | None:
    in_value = DIR_INDEX[in_dir]
    track_type = TRACK_TO_TYPE[cell]
    if track_type == "H":
        return in_dir if in_value % 2 != 0 else None
    if track_type == "V":
        return in_dir if in_value % 2 != 1 else None

    curve_value = TRACK_TYPES[track_type]
    if curve_value == in_value - 1:
        return INDEX_DIR[((in_value + 2) % 4) + 1]
    if (curve_value + 1) % 4 == in_value - 1:
        return INDEX_DIR[(in_value % 4) + 1]
    return None


@dataclass(frozen=True)
class Obj:
    cell: str
    pos: tuple[int, int]


@dataclass(frozen=True)
class State:
    player: tuple[int, int]
    objects: tuple[Obj, ...]
    lever_bits: int = 0


class RailRepairEnv:
    def __init__(self, level_text: str):
        lines = level_text.splitlines()
        if not lines or len({len(line) for line in lines}) != 1:
            raise ValueError("Level must be a non-empty rectangle")
        if len(lines) > MAX_HEIGHT or len(lines[0]) > MAX_WIDTH:
            raise ValueError(f"Level exceeds {MAX_WIDTH}x{MAX_HEIGHT}")

        self.width = len(lines[0])
        self.height = len(lines)
        self.grid = [list(line) for line in lines]
        self.starts: dict[int, tuple[tuple[int, int], str]] = {}
        player: tuple[int, int] | None = None
        objects: list[Obj] = []

        for y, row in enumerate(self.grid):
            for x, cell in enumerate(row):
                edge = x == 0 or y == 0 or x == self.width - 1 or y == self.height - 1
                if cell == "@":
                    player = (x, y)
                    self.grid[y][x] = " "
                elif cell in MOVABLE_TRACKS:
                    objects.append(Obj(cell, (x, y)))
                    self.grid[y][x] = " "
                elif edge and cell in LEVERS:
                    self.starts[int(cell) - 1] = ((x, y), self._start_dir(x, y))

        if player is None:
            raise ValueError("Missing player")
        if not self.starts:
            raise ValueError("Missing start tracks")

        self.initial_state = State(player=player, objects=tuple(objects), lever_bits=0)
        self.num_levers = len(self.starts)
        self.target_lever_bits = sum(1 << index for index in self.starts)

    def reset(self) -> State:
        return self.initial_state

    def _start_dir(self, x: int, y: int) -> str:
        on_top = y == 0
        on_right = x == self.width - 1
        on_bottom = y == self.height - 1
        on_left = x == 0
        if sum([on_top, on_right, on_bottom, on_left]) != 1:
            raise ValueError("Start track must be on one non-corner edge")
        if on_top:
            return "d"
        if on_right:
            return "l"
        if on_bottom:
            return "u"
        return "r"

    def state_key(self, state: State) -> tuple:
        return (
            state.player,
            state.lever_bits,
            tuple((obj.cell, obj.pos) for obj in state.objects),
        )

    def at(self, state: State, pos: tuple[int, int]) -> tuple[str, int | None]:
        floor_index: int | None = None
        for index, obj in enumerate(state.objects):
            if obj.pos != pos:
                continue
            if obj.cell != " ":
                return obj.cell, index
            floor_index = index
        if floor_index is not None:
            return " ", floor_index
        if state.player == pos:
            return "@", None
        x, y = pos
        if x < 0 or y < 0 or x >= self.width or y >= self.height:
            return "#", None
        return self.grid[y][x], None

    def is_track_cell(self, cell: str, pos: tuple[int, int]) -> bool:
        if cell not in TRACK_CHARS:
            return False
        if cell in LEVERS:
            return is_edge_pos(pos[0], pos[1], self.width, self.height)
        return True

    def valid_actions(self, state: State) -> list[str]:
        actions: list[str] = []
        for action in ACTIONS:
            _, _, _, valid = self.step(state, action)
            if valid:
                actions.append(action)
        return actions

    def is_solved(self, state: State) -> bool:
        return (state.lever_bits & self.target_lever_bits) == self.target_lever_bits

    def exits(self, pos: tuple[int, int], direction: str) -> bool:
        x, y = pos
        return (
            (y == 0 and direction == "u")
            or (x == self.width - 1 and direction == "r")
            or (y == self.height - 1 and direction == "d")
            or (x == 0 and direction == "l")
        )

    def simulate_train(self, state: State, lever_index: int) -> bool:
        pos, direction = self.starts[lever_index]
        while direction is not None:
            pos = add_pos(pos, direction)
            cell, _ = self.at(state, pos)
            if not self.is_track_cell(cell, pos):
                return False
            direction = ride(cell, direction)
            if direction is not None and self.exits(pos, direction):
                return True
        return False

    def step(self, state: State, action: str) -> tuple[State, float, bool, bool]:
        next_pos = add_pos(state.player, action)
        cell, obj_index = self.at(state, next_pos)

        if cell == " ":
            next_state = replace(state, player=next_pos)
            return next_state, -0.01, self.is_solved(next_state), True

        if cell in MOVABLE_TRACKS:
            assert obj_index is not None
            pushed_pos = add_pos(next_pos, action)
            pushed_cell, _ = self.at(state, pushed_pos)
            if pushed_cell not in {" ", "*"}:
                return state, -0.08, False, False

            objects = list(state.objects)
            pushed_obj = objects[obj_index]
            objects[obj_index] = Obj(" " if pushed_cell == "*" else pushed_obj.cell, pushed_pos)
            next_state = State(player=next_pos, objects=tuple(objects), lever_bits=state.lever_bits)
            return next_state, -0.01, self.is_solved(next_state), True

        if cell in LEVERS:
            lever_index = int(cell) - 1
            if (state.lever_bits >> lever_index) & 1:
                return state, -0.08, False, False
            if not self.simulate_train(state, lever_index):
                return state, -0.08, False, False

            next_state = replace(state, lever_bits=state.lever_bits | (1 << lever_index))
            solved = self.is_solved(next_state)
            return next_state, 25.0 if solved else 4.0, solved, True

        return state, -0.08, False, False

    def encode(self, state: State, device: torch.device) -> torch.Tensor:
        channels = torch.zeros((CHANNEL_COUNT, MAX_HEIGHT, MAX_WIDTH), dtype=torch.float32, device=device)

        for y, row in enumerate(self.grid):
            for x, cell in enumerate(row):
                if cell == "#":
                    channels[CHANNEL_WALL, y, x] = 1.0
                elif cell == "*":
                    channels[CHANNEL_HOLE, y, x] = 1.0
                elif cell in LEVERS and not is_edge_pos(x, y, self.width, self.height):
                    channels[CHANNEL_LEVER + int(cell) - 1, y, x] = 1.0
                elif cell in TRACK_TO_TYPE:
                    channel = CHANNEL_FIXED_TRACK + TRACK_TYPES[TRACK_TO_TYPE[cell]]
                    channels[channel, y, x] = 1.0

        for obj in state.objects:
            if obj.cell != " ":
                x, y = obj.pos
                channel = CHANNEL_MOVABLE_TRACK + TRACK_TYPES[TRACK_TO_TYPE[obj.cell]]
                channels[channel, y, x] = 1.0

        px, py = state.player
        channels[CHANNEL_PLAYER, py, px] = 1.0
        for lever_index in range(3):
            if (state.lever_bits >> lever_index) & 1:
                channels[CHANNEL_LEVER_TOGGLED + lever_index, :, :] = 1.0
        return channels

    def render(self, state: State) -> str:
        rows = [
            [self.at(state, (x, y))[0] for x in range(self.width)]
            for y in range(self.height)
        ]
        return "\n".join("".join(row) for row in rows)


def level_path(level: int) -> Path:
    return LEVELS_DIR / f"level{level}.txt"


def solution_path(level: int) -> Path:
    return SOLUTIONS_DIR / f"level{level}.txt"


def parse_solution(path: Path) -> str | None:
    if not path.is_file():
        return None
    match = re.search(r"^Solution:\s*([udlr]+)\s*$", path.read_text(encoding="utf-8"), re.MULTILINE)
    return match.group(1) if match else None


def generate_solution(level: int, timeout: float | None) -> str | None:
    if not SOLVER_EXE.is_file():
        print(f"  C++ solver not found: {SOLVER_EXE}", flush=True)
        return None
    print(f"  Running C++ solver for missing level {level} demonstration...", flush=True)
    subprocess.run(
        [str(SOLVER_EXE), str(level)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=timeout,
        check=False,
    )
    return parse_solution(solution_path(level))


def iter_levels(levels: Iterable[int]) -> Iterable[tuple[int, RailRepairEnv]]:
    for level in levels:
        path = level_path(level)
        if not path.is_file():
            raise FileNotFoundError(path)
        yield level, RailRepairEnv(path.read_text(encoding="utf-8"))


def parse_level_list(text: str) -> list[int]:
    levels: list[int] = []
    for part in text.split(","):
        if "-" in part:
            start, end = (int(value) for value in part.split("-", 1))
            levels.extend(range(start, end + 1))
        else:
            levels.append(int(part))
    return levels


def add_common_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL_PATH, help="Path to the PyTorch model.")
    parser.add_argument("--device", default="cpu", help="PyTorch device, e.g. cpu or cuda.")
    parser.add_argument("--seed", type=int, default=0, help="Random seed.")


def load_model[T](cls: type[T], path: Path, device: torch.device) -> T:
    model = cls().to(device)
    if path.is_file():
        payload = torch.load(path, map_location=device, weights_only=False)
        model.load_state_dict(payload["model"])
    return model


def save_model(path: Path, model: torch.nn.Module, args: argparse.Namespace) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    saved_args = {
        key: str(value) if isinstance(value, Path) else value
        for key, value in vars(args).items()
    }
    torch.save(
        {
            "model": model.state_dict(),
            "channels": CHANNEL_COUNT,
            "actions": ACTIONS,
            "saved_at": time.time(),
            "args": saved_args,
        },
        path,
    )
