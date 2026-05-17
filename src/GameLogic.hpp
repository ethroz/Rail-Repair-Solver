#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "FixedChainQueue.hpp"
#include "FixedQueue.hpp"
#include "FixedVector.hpp"
#include "Grid.hpp"
#include "LeverList.hpp"
#include "PathSearch.hpp"
#include "PriorityChainQueue.hpp"
#include "State.hpp"

using Stamp = size_t;

static struct Stats {
    Stamp iterations;
    size_t visited;
} stats;

using CellDescriptor = std::pair<char, bool>;

const static std::map<CellDescriptor, Cell> LEGEND = {
    {{'@', false}, PLAYER},
    {{'#', false}, WALL},
    {{'#', true }, WALL},
    {{'*', false}, HOLE},
    {{' ', false}, FLOOR},
    {{'1', false}, LEVER1},
    {{'2', false}, LEVER2},
    {{'3', false}, LEVER3},
    {{'1', true }, TRACK1},
    {{'2', true }, TRACK2},
    {{'3', true }, TRACK3},
    {{'H', true }, IMMOVABLE_H},
    {{'V', true }, IMMOVABLE_V},
    {{'L', true }, IMMOVABLE_SW},
    {{'U', true }, IMMOVABLE_NW},
    {{'R', true }, IMMOVABLE_NE},
    {{'D', true }, IMMOVABLE_SE},
    {{'H', false}, IMMOVABLE_H},
    {{'V', false}, IMMOVABLE_V},
    {{'L', false}, IMMOVABLE_SW},
    {{'U', false}, IMMOVABLE_NW},
    {{'R', false}, IMMOVABLE_NE},
    {{'D', false}, IMMOVABLE_SE},
    {{'h', false}, MOVABLE_H},
    {{'v', false}, MOVABLE_V},
    {{'l', false}, MOVABLE_SW},
    {{'u', false}, MOVABLE_NW},
    {{'r', false}, MOVABLE_NE},
    {{'d', false}, MOVABLE_SE},
};

std::string readFile(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::invalid_argument("File does not exist");
    }
    std::fstream file(path, std::ios::binary | std::ios::in);
    std::stringstream str;
    str << file.rdbuf();
    return str.str();
}

std::pair<Grid, State> stateFromString(std::string_view board) {
    Grid grid;
    State state;

    size_t width = board.find_first_of("\r\n");
    if (width < 2 || width > X_MAX) {
        throw std::invalid_argument("Invalid level width");
    }
    grid.width = uint8_t(width);
    size_t height = std::count(board.begin(), board.end(), '\n');
    if (height < 2 || height > Y_MAX) {
        throw std::invalid_argument("Invalid level height");
    }
    grid.height = uint8_t(height);

    Position pos;
    uint8_t numLevers = 0;
    for (size_t i = 0; i < board.size(); i++) {
        const char character = board[i];
        if (character == '\n') {
            if (pos.x() != grid.width) {
                throw std::invalid_argument("Inconsistent level widths");
            }
            pos.x(0);
            pos.y(pos.y() + 1);
            if (pos.y() == grid.height) {
                break;
            }
            else {
                continue;
            }
        }
        else if (character == '\r') {
            continue;
        }

        const bool top = pos.y() == 0;
        const bool right = pos.x() == grid.width - 1;
        const bool bottom = pos.y() == grid.height - 1;
        const bool left = pos.x() == 0;
        const bool edge = top || bottom || left || right;
        const auto cell = LEGEND.at({character, edge});

        if (cell.isMovable()) {
            if (cell.isTrack()) {
                state.objects[grid.objectCount] = cell;
                state.objectPositions[grid.objectCount] = pos;
                ++grid.objectCount;
            }
            grid.at(pos) = FLOOR;
        }
        else {
            grid.at(pos) = cell;
        }

        if (cell.isLever()) {
            numLevers++;
        }
        else if (cell == PLAYER) {
            if (edge) {
                throw std::invalid_argument("A player cannot be on the edge");
            }
            if (state.player.x() != 0) {
                throw std::invalid_argument("Cannot have more than one player");
            }

            state.player = pos;
        }

        pos.x(pos.x() + 1);
    }

    if (state.player == Position()) {
        throw std::invalid_argument("Missing a player");
    }
    if (numLevers > MAX_LEVERS || numLevers == 0) {
        throw std::invalid_argument(std::format("Invalid number of levers: {}", numLevers));
    }
    if (grid.objectCount > MAX_OBJECTS) {
        throw std::invalid_argument(std::format("Invalid number of objects: {}", grid.objectCount));
    }

    return { grid, state };
}

bool simulateTrain(
    const Grid& grid,
    const StartList& startList,
    const State& state,
    uint8_t index
) {
    auto [pos, dir] = startList.at(index);
    assert(dir != NONE);
    assert(grid.at(state, pos).cell.isTrack());
    while (dir != NONE) {
        pos += dir;
        const auto cell = grid.at(state, pos).cell;
        if (!cell.isTrack()) {
            return false;
        }
        dir = cell.trackType().ride(dir);

        if (grid.exits({pos, dir})) {
            return true;
        }
    }
    return false;
}

Direction getStepDirection(
    const Grid& grid,
    const DeadState& currentState,
    const DeadState& prevState
) {
    Direction stepDir = Position::diffStep(prevState.player, currentState.player);
    if (stepDir == NONE) {
        assert(currentState.numToggledLevers() - prevState.numToggledLevers() > 0);
        for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
            const Direction dir = DIRECTION(dirValue);
            if (grid.at(currentState.player + dir).isLever()) {
                stepDir = dir;
                break;
            }
        }
        assert(stepDir != NONE);
    }
    return stepDir;
}

using StateQueue = PriorityChainQueue<State, DeadState, uint32_t, true>;
constexpr size_t MAX_NEXT_STATES = MAX_OBJECTS * MAX_DIR + MAX_LEVERS;
// Based on max number of elements we could possibly see in the stateQueue.
constexpr size_t MAX_MOVE_QUEUE_SIZE = MAX_OBJECTS * 2;
// Round up to the nearest power of two to convert modulo operators to and operators.
constexpr size_t MOVE_QUEUE_SIZE = std::bit_ceil(MAX_MOVE_QUEUE_SIZE);
using MoveQueue = FixedChainQueue<RankedDeadState, DeadState, State, MOVE_QUEUE_SIZE, BASE, MAX_NEXT_STATES, uint8_t>;

std::vector<Direction> buildSolution(
    const Grid& grid,
    const StateQueue& stateQueue
) {
    std::vector<Direction> solution;

    auto stateIt = stateQueue.begin();
    DeadState currentState = *stateIt;
    for (++stateIt; stateIt != stateQueue.end(); ++stateIt) {
        DeadState prevState = *stateIt;
        Direction stepDir = getStepDirection(grid, currentState, prevState);
        solution.push_back(stepDir);
        currentState = std::move(prevState);
    }

    std::reverse(solution.begin(), solution.end());
    return solution;
}

std::vector<Direction> search(
    const Grid& grid,
    const StartList& startList,
    const State& initialState,
    const std::atomic_bool& done = {}
) {
    EndList endList = findEndStates(grid, startList, initialState);
    endList.connectLists(grid);

    absl::flat_hash_map<StateEncoding, Rank> visited(5000000);
    StateQueue stateQueue(1000000, 5000000);

    std::vector<Direction> bestMoveSequence;

    constexpr Stamp MAX_STAMP = std::numeric_limits<Stamp>::max();
    std::array<std::array<Stamp, 16>, 16> moveVisitedStamp = {};
    MoveQueue moveQueue;

    for (uint8_t y = 0; y < grid.height; ++y) {
        for (uint8_t x = 0; x < grid.width; ++x) {
            if (!grid.at(x, y).isWalkable()) {
                moveVisitedStamp[y][x] = MAX_STAMP;
            }
        }
    }

    const auto heuristic = [&](const State& state) -> Rank {
        return state.moves + std::ranges::min(
            endList.at(0) |
            std::views::transform([&](const GoalState& end) {
                return end.distance(state, grid.objectCount);
            }));
    };

    {
        State state = initialState;
        state.rank = heuristic(state);
        stateQueue.push(std::move(state));
    }

    stats.iterations = 0;
    constexpr uint32_t PROGRESS_RESET = 100000;
    uint32_t progressCountdown = PROGRESS_RESET;

    while (!stateQueue.empty()) {
        const State& currentState = stateQueue.peek();

        if (currentState.numToggledLevers() == startList.size()) {
            bestMoveSequence = buildSolution(grid, stateQueue);
            break;
        }

        ++stats.iterations;
        if (--progressCountdown == 0) {
            progressCountdown = PROGRESS_RESET;
            if (done) {
                break;
            }
            std::cout <<
                "\rStates checked: " << stats.iterations << ". "
                "Min rank: " << currentState.rank << ". "
                "Queue size: " << stateQueue.size() << ". "
                "Queue dead size: " << stateQueue.deadSize() << ". "
                "Visited cache: " << visited.size() << ". ";
            std::cout.flush();
        }

        auto [it, inserted] = visited.try_emplace(currentState.encode(grid.objectCount), currentState.moves);
        if (!inserted) {
            if (it->second <= currentState.moves) {
                stateQueue.removeFront();
                continue;
            }
            it->second = currentState.moves;
        }

        moveQueue.reset();

        moveQueue.push(currentState);

        while (!moveQueue.empty()) {
            const auto& posState = moveQueue.peek();
            Stamp& stamp = moveVisitedStamp[posState.player.y()][posState.player.x()];
            if (stamp >= stats.iterations) {
                moveQueue.removeFront();
                continue;
            }
            stamp = stats.iterations;

            for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
                const Direction dir = DIRECTION(dirValue);
                const auto nextMove = posState.player + dir;
                if (moveVisitedStamp[nextMove.y()][nextMove.x()] >= stats.iterations) {
                    continue;
                }

                const auto [nextCell, nextObjIndex] = grid.at(currentState, nextMove);
                if (nextCell.isMovable()) {
                    if (nextCell == FLOOR) {
                        auto nextPosState = posState;
                        nextPosState.player = nextMove;
                        nextPosState.rank++;
                        moveQueue.push(std::move(nextPosState));
                    }
                    else if (nextCell.isTrack()) {
                        const auto nextNextMove = nextMove + dir;
                        const auto [nextNextCell, _] = grid.at(currentState, nextNextMove);
                        if (!nextNextCell.isEmpty()) {
                            continue;
                        }

                        State nextState = currentState;
                        nextState.player = nextMove;
                        assert(nextObjIndex < MAX_OBJECTS);
                        nextState.objectPositions[nextObjIndex] = nextNextMove;
                        if (nextNextCell == HOLE) {
                            nextState.objects[nextObjIndex] = FLOOR;
                        }

                        nextState.moves = posState.rank + 1;
                        nextState.rank = heuristic(nextState);
                        moveQueue.pushExtern(std::move(nextState));
                    }
                }
                else if (
                    nextCell.isLever() &&
                    !currentState.leverToggled(nextCell.index()) &&
                    simulateTrain(grid, startList, currentState, nextCell.index())
                ) {
                    State nextState = currentState;
                    nextState.player = posState.player;
                    nextState.toggleLever(nextCell.index());
                    nextState.moves = posState.rank + 1;
                    nextState.rank = heuristic(nextState);
                    moveQueue.pushExtern(std::move(nextState));
                }
            }

            moveQueue.removeFront();
        }
        
        moveQueue.pruneDead();
        
        stateQueue.removeFrontWithDeadSubqueue(moveQueue);
    }

    stats.visited = visited.size();
    return bestMoveSequence;
}
