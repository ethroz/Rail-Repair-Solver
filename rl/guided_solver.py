import argparse
import heapq
import random
import time
from dataclasses import dataclass

import torch
import torch.nn as nn
import torch.nn.functional as F

from common import *

class RailNet(nn.Module):
    def __init__(self):
        super().__init__()
        self.conv = nn.Sequential(
            nn.Conv2d(CHANNEL_COUNT, 64, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.Conv2d(64, 96, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.Conv2d(96, 96, kernel_size=3, padding=1),
            nn.ReLU(),
        )
        self.trunk = nn.Sequential(
            nn.Flatten(),
            nn.Linear(96 * MAX_HEIGHT * MAX_WIDTH, 256),
            nn.ReLU(),
        )
        self.policy_head = nn.Linear(256, len(ACTIONS))
        self.value_head = nn.Linear(256, 1)

    def forward(self, x: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
        features = self.trunk(self.conv(x))
        policy_logits = self.policy_head(features)
        remaining_moves = F.softplus(self.value_head(features)).squeeze(-1)
        return policy_logits, remaining_moves


@dataclass
class DemoSample:
    state: torch.Tensor
    action: int
    remaining: float


def encode_batch(env: RailRepairEnv, states: list[State], device: torch.device) -> torch.Tensor:
    return torch.stack([env.encode(state, device) for state in states])


def collect_demonstrations(env: RailRepairEnv, solution: str, device: torch.device) -> list[DemoSample]:
    samples: list[DemoSample] = []
    state = env.reset()

    for step_index, action in enumerate(solution):
        state_tensor = env.encode(state, device)
        next_state, reward, done, valid = env.step(state, action)
        if not valid:
            break
        samples.append(
            DemoSample(
                state=state_tensor.detach().cpu(),
                action=ACTION_TO_INDEX[action],
                remaining=float(len(solution) - step_index),
            )
        )
        state = next_state
        if done:
            break

    return samples


def train_supervised(
    model: RailNet,
    optimizer: torch.optim.Optimizer,
    samples: list[DemoSample],
    epochs: int,
    batch_size: int,
    rng: random.Random,
    device: torch.device,
    progress_interval: int,
) -> tuple[float, float]:
    if not samples:
        print("Supervised training: no demonstration states found; skipping.", flush=True)
        return 0.0, 0.0

    policy_loss_value = 0.0
    value_loss_value = 0.0
    indices = list(range(len(samples)))
    print(
        f"Supervised training: {len(samples)} states, {epochs} epochs, batch size {batch_size}.",
        flush=True,
    )
    for epoch in range(1, epochs + 1):
        rng.shuffle(indices)
        for start in range(0, len(indices), batch_size):
            batch_indices = indices[start : start + batch_size]
            batch_states = torch.stack([samples[index].state for index in batch_indices]).to(device)
            batch_actions = torch.tensor([samples[index].action for index in batch_indices], dtype=torch.long, device=device)
            batch_remaining = torch.tensor(
                [samples[index].remaining for index in batch_indices],
                dtype=torch.float32,
                device=device,
            )
            logits, predicted_remaining = model(batch_states)
            policy_loss = F.cross_entropy(logits, batch_actions)
            value_loss = F.smooth_l1_loss(predicted_remaining, batch_remaining)
            loss = policy_loss + value_loss
            optimizer.zero_grad()
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            policy_loss_value = float(policy_loss.detach().cpu())
            value_loss_value = float(value_loss.detach().cpu())
        if progress_interval > 0 and (epoch == 1 or epoch == epochs or epoch % progress_interval == 0):
            print(
                f"  epoch {epoch:>5}/{epochs}: "
                f"policy_loss={policy_loss_value:.6f}, value_loss={value_loss_value:.6f}",
                flush=True,
            )
    return policy_loss_value, value_loss_value


def choose_action(
    model: RailNet,
    env: RailRepairEnv,
    state: State,
    epsilon: float,
    rng: random.Random,
    device: torch.device,
    valid_only: bool,
) -> str:
    valid_actions = env.valid_actions(state) if valid_only else list(ACTIONS)
    if not valid_actions:
        return rng.choice(list(ACTIONS))
    if rng.random() < epsilon:
        return rng.choice(valid_actions)

    with torch.no_grad():
        logits, _ = model(env.encode(state, device).unsqueeze(0))
        policy_scores = logits[0]
    ranked = sorted(valid_actions, key=lambda action: float(policy_scores[ACTION_TO_INDEX[action]]), reverse=True)
    return ranked[0]


def run_policy(
    model: RailNet,
    env: RailRepairEnv,
    max_steps: int,
    epsilon: float,
    seed: int,
    device: torch.device,
    trace: bool = False,
) -> tuple[str, State, bool]:
    rng = random.Random(seed)
    state = env.reset()
    moves: list[str] = []
    seen: set[tuple] = set()

    for step in range(1, max_steps + 1):
        key = env.state_key(state)
        if epsilon == 0.0 and key in seen:
            if trace:
                print(f"  step {step}: stopping because policy revisited a state", flush=True)
            break
        seen.add(key)

        action = choose_action(model, env, state, epsilon, rng, device, valid_only=True)
        next_state, _, done, valid = env.step(state, action)
        if trace:
            status = "valid" if valid else "invalid"
            print(f"  step {step:>4}: action={action} {status}", flush=True)
        if not valid:
            break
        moves.append(action)
        state = next_state
        if done:
            return "".join(moves), state, True

    return "".join(moves), state, env.is_solved(state)


def evaluate_state(
    model: RailNet,
    env: RailRepairEnv,
    state: State,
    device: torch.device,
) -> tuple[torch.Tensor, float]:
    with torch.no_grad():
        logits, remaining = model(env.encode(state, device).unsqueeze(0))
        log_probs = F.log_softmax(logits[0], dim=0).detach().cpu()
        value = float(remaining[0].detach().cpu())
    return log_probs, value


def guided_search(
    model: RailNet,
    env: RailRepairEnv,
    device: torch.device,
    max_expansions: int,
    max_depth: int,
    beam_width: int,
    policy_weight: float,
    value_weight: float,
    progress_interval: int,
    trace: bool,
) -> tuple[str, State, bool, int]:
    start_state = env.reset()
    if env.is_solved(start_state):
        return "", start_state, True, 0

    counter = 0
    frontier: list[tuple[float, int, str, State]] = [(0.0, counter, "", start_state)]
    best_depth: dict[tuple, int] = {env.state_key(start_state): 0}
    expansions = 0
    started = time.perf_counter()

    while frontier and expansions < max_expansions:
        score, _, moves, state = heapq.heappop(frontier)
        depth = len(moves)
        if depth >= max_depth:
            continue

        expansions += 1
        log_probs, predicted_remaining = evaluate_state(model, env, state, device)
        valid_actions = env.valid_actions(state)
        if trace:
            action_summary = " ".join(
                f"{action}:{float(log_probs[ACTION_TO_INDEX[action]]):.2f}"
                for action in valid_actions
            )
            print(
                f"  expand {expansions}: depth={depth}, score={score:.3f}, "
                f"value={predicted_remaining:.2f}, actions=[{action_summary}]",
                flush=True,
            )

        for action in valid_actions:
            next_state, _, done, valid = env.step(state, action)
            if not valid:
                continue
            next_moves = moves + action
            if done:
                return next_moves, next_state, True, expansions

            key = env.state_key(next_state)
            next_depth = depth + 1
            if best_depth.get(key, max_depth + 1) <= next_depth:
                continue
            best_depth[key] = next_depth

            _, child_remaining = evaluate_state(model, env, next_state, device)
            action_cost = -float(log_probs[ACTION_TO_INDEX[action]])
            next_score = next_depth + value_weight * child_remaining + policy_weight * action_cost
            counter += 1
            heapq.heappush(frontier, (next_score, counter, next_moves, next_state))

        if len(frontier) > beam_width:
            frontier = heapq.nsmallest(beam_width, frontier)
            heapq.heapify(frontier)

        if progress_interval > 0 and (expansions == 1 or expansions % progress_interval == 0):
            elapsed = time.perf_counter() - started
            best_score = frontier[0][0] if frontier else float("inf")
            print(
                f"  search expansions={expansions}, frontier={len(frontier)}, "
                f"visited={len(best_depth)}, best_score={best_score:.3f}, elapsed={elapsed:.1f}s",
                flush=True,
            )

    return "", start_state, False, expansions


def cmd_train(args: argparse.Namespace) -> int:
    rng = random.Random(args.seed)
    torch.manual_seed(args.seed)
    device = torch.device(args.device)
    print("Training configuration", flush=True)
    print(f"  Python model path: {args.model}", flush=True)
    print(f"  Device: {device}", flush=True)
    print(f"  Levels: {args.levels}", flush=True)
    print(f"  Seed: {args.seed}", flush=True)
    print(
        f"  Supervised epochs: {args.epochs}; batch size: {args.batch_size}",
        flush=True,
    )
    print(f"  Loading model: {'existing checkpoint' if args.model.is_file() else 'new model'}", flush=True)
    model = load_model(RailNet, args.model, device)
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)

    demo_samples: list[DemoSample] = []
    demo_steps = 0
    start = time.perf_counter()

    print("Collecting demonstrations", flush=True)
    for level, env in iter_levels(args.levels):
        print(
            f"  Level {level}: board {env.width}x{env.height}, "
            f"objects={len(env.initial_state.objects)}, levers={env.num_levers}",
            flush=True,
        )
        solution = parse_solution(solution_path(level))
        if solution is None and args.use_solver:
            solution = generate_solution(level, args.solver_timeout)
        if solution is not None:
            samples = collect_demonstrations(env, solution, device)
            demo_samples.extend(samples)
            demo_steps += len(samples)
            print(f"    demonstration: {len(samples)} usable steps from {solution_path(level).name}", flush=True)
        else:
            print("    demonstration: none; this level is skipped for supervised training.", flush=True)

    policy_loss, value_loss = train_supervised(
        model,
        optimizer,
        demo_samples,
        epochs=args.epochs,
        batch_size=args.batch_size,
        rng=rng,
        device=device,
        progress_interval=args.progress_interval,
    )

    print(f"Saving model to {args.model}", flush=True)
    save_model(args.model, model, args)
    elapsed = time.perf_counter() - start
    print(f"Saved model: {args.model}")
    print(f"Device: {device}")
    print(f"Demonstration steps: {demo_steps}")
    print(f"Policy loss: {policy_loss:.6f}")
    print(f"Value loss: {value_loss:.6f}")
    print(f"Training time: {elapsed:.3f}s")
    return 0


def cmd_solve(args: argparse.Namespace) -> int:
    device = torch.device(args.device)
    print(f"Loading model from {args.model} on {device}", flush=True)
    model = load_model(RailNet, args.model, device)
    model.eval()
    env = RailRepairEnv(level_path(args.level).read_text(encoding="utf-8"))
    print(
        f"Solving level {args.level}: board {env.width}x{env.height}, "
        f"objects={len(env.initial_state.objects)}, levers={env.num_levers}, max_steps={args.max_steps}",
        flush=True,
    )
    moves, state, solved = run_policy(model, env, args.max_steps, args.epsilon, args.seed, device, trace=args.trace)
    print(f"Level: {args.level}")
    print(f"Solved: {'yes' if solved else 'no'}")
    print(f"Moves: {moves if moves else '(none)'}")
    if args.render:
        print()
        print(env.render(state))
    return 0 if solved else 1


def cmd_eval(args: argparse.Namespace) -> int:
    device = torch.device(args.device)
    print(f"Loading model from {args.model} on {device}", flush=True)
    model = load_model(RailNet, args.model, device)
    model.eval()
    failures: list[int] = []
    for level, env in iter_levels(args.levels):
        print(f"Evaluating level {level}...", flush=True)
        moves, _, solved = run_policy(model, env, args.max_steps, epsilon=0.0, seed=args.seed, device=device)
        status = "OK" if solved else "FAIL"
        print(f"level {level:>2}: {status:4} {len(moves):>5} moves")
        if not solved:
            failures.append(level)
    if failures:
        print("Failed levels: " + ", ".join(str(level) for level in failures))
        return 1
    return 0


def cmd_search(args: argparse.Namespace) -> int:
    device = torch.device(args.device)
    print(f"Loading model from {args.model} on {device}", flush=True)
    model = load_model(RailNet, args.model, device)
    model.eval()
    env = RailRepairEnv(level_path(args.level).read_text(encoding="utf-8"))
    print(
        f"Guided search level {args.level}: board {env.width}x{env.height}, "
        f"objects={len(env.initial_state.objects)}, levers={env.num_levers}",
        flush=True,
    )
    print(
        f"  max_expansions={args.max_expansions}, max_depth={args.max_depth}, "
        f"beam_width={args.beam_width}, policy_weight={args.policy_weight}, "
        f"value_weight={args.value_weight}",
        flush=True,
    )
    started = time.perf_counter()
    moves, state, solved, expansions = guided_search(
        model=model,
        env=env,
        device=device,
        max_expansions=args.max_expansions,
        max_depth=args.max_depth,
        beam_width=args.beam_width,
        policy_weight=args.policy_weight,
        value_weight=args.value_weight,
        progress_interval=args.progress_interval,
        trace=args.trace,
    )
    elapsed = time.perf_counter() - started
    print(f"Level: {args.level}")
    print(f"Solved: {'yes' if solved else 'no'}")
    print(f"Expansions: {expansions}")
    print(f"Runtime: {elapsed:.3f}s")
    print(f"Moves: {moves if moves else '(none)'}")
    if args.render:
        print()
        print(env.render(state))
    return 0 if solved else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Train and run a neural-guided solver for Rail Repair levels.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    train_parser = subparsers.add_parser("train", help="Train the policy/value network from solver traces.")
    train_parser.add_argument("levels", type=parse_level_list, help="Level list, e.g. 1 or 1,2,5-10.")
    train_parser.add_argument("--use-solver", action="store_true", help="Run the C++ solver to create missing demonstrations.")
    train_parser.add_argument("--solver-timeout", type=float, default=None, help="Timeout in seconds for each C++ solver run.")
    train_parser.add_argument("--epochs", type=int, default=300, help="Supervised epochs over solver demonstrations.")
    train_parser.add_argument("--batch-size", type=int, default=128, help="Training batch size.")
    train_parser.add_argument("--lr", type=float, default=0.0003, help="AdamW learning rate.")
    train_parser.add_argument("--weight-decay", type=float, default=0.00001, help="AdamW weight decay.")
    train_parser.add_argument(
        "--progress-interval",
        type=int,
        default=50,
        help="Print training progress every N epochs; use 0 to print only phase summaries.",
    )
    add_common_args(train_parser)
    train_parser.set_defaults(func=cmd_train)

    solve_parser = subparsers.add_parser("solve", help="Run the trained policy for one level.")
    solve_parser.add_argument("level", type=int, help="Level number.")
    solve_parser.add_argument("--max-steps", type=int, default=600, help="Maximum policy steps.")
    solve_parser.add_argument("--epsilon", type=float, default=0.0, help="Optional exploration while solving.")
    solve_parser.add_argument("--render", action="store_true", help="Print the final board.")
    solve_parser.add_argument("--trace", action="store_true", help="Print each selected action while solving.")
    add_common_args(solve_parser)
    solve_parser.set_defaults(func=cmd_solve)

    eval_parser = subparsers.add_parser("eval", help="Evaluate the trained policy on levels.")
    eval_parser.add_argument("levels", type=parse_level_list, help="Level list, e.g. 1 or 1,2,5-10.")
    eval_parser.add_argument("--max-steps", type=int, default=600, help="Maximum policy steps per level.")
    add_common_args(eval_parser)
    eval_parser.set_defaults(func=cmd_eval)

    search_parser = subparsers.add_parser("search", help="Use the policy/value network to guide beam/A* search.")
    search_parser.add_argument("level", type=int, help="Level number.")
    search_parser.add_argument("--max-expansions", type=int, default=200000, help="Maximum states to expand.")
    search_parser.add_argument("--max-depth", type=int, default=1000, help="Maximum move depth.")
    search_parser.add_argument("--beam-width", type=int, default=50000, help="Maximum frontier states to keep.")
    search_parser.add_argument("--policy-weight", type=float, default=0.25, help="Penalty weight for low-policy-probability actions.")
    search_parser.add_argument("--value-weight", type=float, default=1.0, help="Weight for predicted remaining moves.")
    search_parser.add_argument("--progress-interval", type=int, default=1000, help="Print search progress every N expansions; use 0 for summary only.")
    search_parser.add_argument("--trace", action="store_true", help="Print every expanded state; very verbose.")
    search_parser.add_argument("--render", action="store_true", help="Print the final board.")
    add_common_args(search_parser)
    search_parser.set_defaults(func=cmd_search)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
