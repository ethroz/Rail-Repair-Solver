import argparse
import random
import time
from collections import deque
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
        self.head = nn.Sequential(
            nn.Flatten(),
            nn.Linear(96 * MAX_HEIGHT * MAX_WIDTH, 256),
            nn.ReLU(),
            nn.Linear(256, len(ACTIONS)),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.head(self.conv(x))


@dataclass
class Transition:
    state: torch.Tensor
    action: int
    reward: float
    next_state: torch.Tensor
    done: bool


class ReplayBuffer:
    def __init__(self, capacity: int):
        self.items: deque[Transition] = deque(maxlen=capacity)

    def push(self, item: Transition) -> None:
        self.items.append(item)

    def sample(self, batch_size: int, rng: random.Random) -> list[Transition]:
        return rng.sample(list(self.items), min(batch_size, len(self.items)))

    def __len__(self) -> int:
        return len(self.items)


def encode_batch(env: RailRepairEnv, states: list[State], device: torch.device) -> torch.Tensor:
    return torch.stack([env.encode(state, device) for state in states])


def collect_demonstrations(
    env: RailRepairEnv,
    solution: str,
    device: torch.device,
    replay: ReplayBuffer,
) -> tuple[list[torch.Tensor], list[int]]:
    states: list[torch.Tensor] = []
    labels: list[int] = []
    state = env.reset()

    for action in solution:
        state_tensor = env.encode(state, device)
        next_state, reward, done, valid = env.step(state, action)
        if not valid:
            break
        next_tensor = env.encode(next_state, device)
        replay.push(
            Transition(
                state=state_tensor.detach().cpu(),
                action=ACTION_TO_INDEX[action],
                reward=reward + 1.0,
                next_state=next_tensor.detach().cpu(),
                done=done,
            )
        )
        states.append(state_tensor)
        labels.append(ACTION_TO_INDEX[action])
        state = next_state
        if done:
            break

    return states, labels


def behavior_clone(
    model: RailNet,
    optimizer: torch.optim.Optimizer,
    states: list[torch.Tensor],
    labels: list[int],
    epochs: int,
    batch_size: int,
    rng: random.Random,
    device: torch.device,
    progress_interval: int,
) -> float:
    if not states:
        print("Behavior cloning: no demonstration states found; skipping.", flush=True)
        return 0.0

    loss_value = 0.0
    indices = list(range(len(states)))
    print(
        f"Behavior cloning: {len(states)} states, {epochs} epochs, batch size {batch_size}.",
        flush=True,
    )
    for epoch in range(1, epochs + 1):
        rng.shuffle(indices)
        for start in range(0, len(indices), batch_size):
            batch_indices = indices[start : start + batch_size]
            batch_states = torch.stack([states[index] for index in batch_indices]).to(device)
            batch_labels = torch.tensor([labels[index] for index in batch_indices], dtype=torch.long, device=device)
            logits = model(batch_states)
            loss = F.cross_entropy(logits, batch_labels)
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
            loss_value = float(loss.detach().cpu())
        if progress_interval > 0 and (epoch == 1 or epoch == epochs or epoch % progress_interval == 0):
            print(f"  BC epoch {epoch:>5}/{epochs}: loss={loss_value:.6f}", flush=True)
    return loss_value


def dqn_update(
    model: RailNet,
    target_model: RailNet,
    optimizer: torch.optim.Optimizer,
    replay: ReplayBuffer,
    batch_size: int,
    gamma: float,
    rng: random.Random,
    device: torch.device,
) -> float:
    if len(replay) == 0:
        return 0.0

    batch = replay.sample(batch_size, rng)
    states = torch.stack([item.state for item in batch]).to(device)
    actions = torch.tensor([item.action for item in batch], dtype=torch.long, device=device)
    rewards = torch.tensor([item.reward for item in batch], dtype=torch.float32, device=device)
    next_states = torch.stack([item.next_state for item in batch]).to(device)
    done = torch.tensor([item.done for item in batch], dtype=torch.float32, device=device)

    q_values = model(states).gather(1, actions.unsqueeze(1)).squeeze(1)
    with torch.no_grad():
        next_q = target_model(next_states).max(dim=1).values
        target = rewards + gamma * next_q * (1.0 - done)

    loss = F.smooth_l1_loss(q_values, target)
    optimizer.zero_grad()
    loss.backward()
    nn.utils.clip_grad_norm_(model.parameters(), 1.0)
    optimizer.step()
    return float(loss.detach().cpu())


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
        q_values = model(env.encode(state, device).unsqueeze(0))[0]
    ranked = sorted(valid_actions, key=lambda action: float(q_values[ACTION_TO_INDEX[action]]), reverse=True)
    return ranked[0]


def train_exploration(
    model: RailNet,
    target_model: RailNet,
    optimizer: torch.optim.Optimizer,
    env: RailRepairEnv,
    replay: ReplayBuffer,
    episodes: int,
    max_steps: int,
    batch_size: int,
    gamma: float,
    epsilon: float,
    epsilon_min: float,
    epsilon_decay: float,
    target_sync: int,
    rng: random.Random,
    device: torch.device,
    level: int,
    progress_interval: int,
) -> tuple[int, int, float]:
    solves = 0
    total_steps = 0
    loss = 0.0

    if episodes == 0:
        print(f"Level {level}: exploration disabled (--episodes 0).", flush=True)
        return solves, total_steps, loss

    print(
        f"Level {level}: exploration start ({episodes} episodes, max {max_steps} steps, replay {len(replay)}).",
        flush=True,
    )
    level_start = time.perf_counter()
    for episode in range(1, episodes + 1):
        state = env.reset()
        current_epsilon = max(epsilon_min, epsilon * (epsilon_decay ** (episode - 1)))
        episode_steps = 0
        for _ in range(max_steps):
            action = choose_action(model, env, state, current_epsilon, rng, device, valid_only=False)
            next_state, reward, done, valid = env.step(state, action)
            shaped_reward = reward if valid else reward - 0.10
            replay.push(
                Transition(
                    state=env.encode(state, device).detach().cpu(),
                    action=ACTION_TO_INDEX[action],
                    reward=shaped_reward,
                    next_state=env.encode(next_state, device).detach().cpu(),
                    done=done,
                )
            )
            loss = dqn_update(model, target_model, optimizer, replay, batch_size, gamma, rng, device)
            total_steps += 1
            if total_steps % target_sync == 0:
                target_model.load_state_dict(model.state_dict())
            if valid:
                state = next_state
            episode_steps += 1
            if done:
                solves += 1
                break
        if progress_interval > 0 and (episode == 1 or episode == episodes or episode % progress_interval == 0):
            elapsed = time.perf_counter() - level_start
            print(
                f"  Level {level} episode {episode:>5}/{episodes}: "
                f"solves={solves}, last_steps={episode_steps}, "
                f"epsilon={current_epsilon:.3f}, loss={loss:.6f}, "
                f"replay={len(replay)}, elapsed={elapsed:.1f}s",
                flush=True,
            )

    return solves, total_steps, loss


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
        f"  BC epochs: {args.bc_epochs}; DQN episodes per level: {args.episodes}; "
        f"max steps: {args.max_steps}",
        flush=True,
    )
    print(f"  Loading model: {'existing checkpoint' if args.model.is_file() else 'new model'}", flush=True)
    model = load_model(RailNet, args.model, device)
    target_model = RailNet().to(device)
    target_model.load_state_dict(model.state_dict())
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    replay = ReplayBuffer(args.replay_size)

    demo_states: list[torch.Tensor] = []
    demo_labels: list[int] = []
    demo_steps = 0
    exploration_steps = 0
    exploration_solves = 0
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
            states, labels = collect_demonstrations(env, solution, device, replay)
            demo_states.extend(states)
            demo_labels.extend(labels)
            demo_steps += len(labels)
            print(f"    demonstration: {len(labels)} usable steps from {solution_path(level).name}", flush=True)
        else:
            print("    demonstration: none; this level will only use exploration.", flush=True)

    bc_loss = behavior_clone(
        model,
        optimizer,
        demo_states,
        demo_labels,
        epochs=args.bc_epochs,
        batch_size=args.batch_size,
        rng=rng,
        device=device,
        progress_interval=args.progress_interval,
    )
    target_model.load_state_dict(model.state_dict())

    dqn_loss = 0.0
    print("Starting DQN exploration", flush=True)
    for level, env in iter_levels(args.levels):
        solves, steps, dqn_loss = train_exploration(
            model,
            target_model,
            optimizer,
            env,
            replay,
            episodes=args.episodes,
            max_steps=args.max_steps,
            batch_size=args.batch_size,
            gamma=args.gamma,
            epsilon=args.epsilon,
            epsilon_min=args.epsilon_min,
            epsilon_decay=args.epsilon_decay,
            target_sync=args.target_sync,
            rng=rng,
            device=device,
            level=level,
            progress_interval=args.progress_interval,
        )
        exploration_solves += solves
        exploration_steps += steps
        print(
            f"Level {level}: exploration done, solves={solves}/{args.episodes}, steps={steps}, last_loss={dqn_loss:.6f}",
            flush=True,
        )

    print(f"Saving model to {args.model}", flush=True)
    save_model(args.model, model, args)
    elapsed = time.perf_counter() - start
    print(f"Saved model: {args.model}")
    print(f"Device: {device}")
    print(f"Demonstration steps: {demo_steps}")
    print(f"Replay size: {len(replay)}")
    print(f"BC loss: {bc_loss:.6f}")
    print(f"DQN loss: {dqn_loss:.6f}")
    print(f"Exploration steps: {exploration_steps}")
    print(f"Exploration solves: {exploration_solves}")
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


def main() -> int:
    parser = argparse.ArgumentParser(description="Train and run a PyTorch DQN solver for Rail Repair levels.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    train_parser = subparsers.add_parser("train", help="Train the neural RL agent.")
    train_parser.add_argument("levels", type=parse_level_list, help="Level list, e.g. 1 or 1,2,5-10.")
    train_parser.add_argument("--use-solver", action="store_true", help="Run the C++ solver to create missing demonstrations.")
    train_parser.add_argument("--solver-timeout", type=float, default=None, help="Timeout in seconds for each C++ solver run.")
    train_parser.add_argument("--bc-epochs", type=int, default=300, help="Behavior cloning epochs over solver demonstrations.")
    train_parser.add_argument("--episodes", type=int, default=0, help="DQN exploration episodes per level.")
    train_parser.add_argument("--max-steps", type=int, default=600, help="Maximum steps per exploration episode.")
    train_parser.add_argument("--batch-size", type=int, default=128, help="Training batch size.")
    train_parser.add_argument("--replay-size", type=int, default=200000, help="Replay buffer capacity.")
    train_parser.add_argument("--lr", type=float, default=0.0003, help="AdamW learning rate.")
    train_parser.add_argument("--weight-decay", type=float, default=0.00001, help="AdamW weight decay.")
    train_parser.add_argument("--gamma", type=float, default=0.985, help="DQN discount factor.")
    train_parser.add_argument("--epsilon", type=float, default=0.35, help="Initial exploration probability.")
    train_parser.add_argument("--epsilon-min", type=float, default=0.03, help="Minimum exploration probability.")
    train_parser.add_argument("--epsilon-decay", type=float, default=0.997, help="Per-episode epsilon multiplier.")
    train_parser.add_argument("--target-sync", type=int, default=500, help="Steps between target network updates.")
    train_parser.add_argument(
        "--progress-interval",
        type=int,
        default=50,
        help="Print training progress every N behavior-cloning epochs and exploration episodes; use 0 to print only phase summaries.",
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

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
