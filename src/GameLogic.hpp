#pragma once

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <ranges>

#include <absl/container/flat_hash_map.h>

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "PriorityQueue.hpp"
#include "Queue.hpp"
#include "StableQueue.hpp"

static struct Stats {
    size_t iterations;
    size_t visited;
} stats;

static void printStats(std::ostream& out) {
    out << "Total iterations: " << stats.iterations << std::endl;
    out << "visited size: " << stats.visited << std::endl;
}

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

struct Vector {
    Position pos{};
    Direction dir{};
};

struct StartList {
    constexpr size_t size() const { return m_size; }

    constexpr const Vector& at(uint8_t index) const {
        if (m_data.at(index).dir == NONE) {
            throw std::invalid_argument(std::format("Invalid railroad index: {}", index));
        }
        return m_data[index];
    }

    constexpr void insert(uint8_t index, Vector&& vec) {
        if (m_data.at(index).dir != NONE) {
            throw std::invalid_argument("Cannot have two starting railroads with the same index");
        }
        m_data[index] = std::move(vec);
        m_size++;
    }

private:
    std::array<Vector, MAX_LEVERS> m_data = {};
    size_t m_size = 0;
};

StartList createStartList(const Grid& grid) {
    StartList list;

    for (uint8_t x = 0; x < grid.width; x++) {
        for (uint8_t y = 0; y < grid.height; y++) {
            const auto cell = grid.at(x, y);
            if (cell.isStart()) {
                Direction dir;

                const bool top = y == 0;
                const bool right = x == grid.width - 1;
                const bool bottom = y == grid.height - 1;
                const bool left = x == 0;
                uint8_t flags = (top ? 0b1000 : 0) | (right ? 0b0100 : 0) | (bottom ? 0b0010 : 0) | (left ? 0b0001 : 0);
                switch (flags) {
                case 0b1000: dir = DOWN;  break;
                case 0b0100: dir = LEFT;  break;
                case 0b0010: dir = UP;    break;
                case 0b0001: dir = RIGHT; break;
                default: throw std::invalid_argument("Cannot have a starting railroad on a corner");
                }

                list.insert(cell.index(), {{x, y}, dir});
            }
        }
    }

    return list;
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

        const bool exitsGrid =
            (pos.y() == 0 && dir == UP) ||
            (pos.x() == grid.width - 1 && dir == RIGHT) ||
            (pos.y() == grid.height - 1 && dir == DOWN) ||
            (pos.x() == 0 && dir == LEFT);
        if (exitsGrid) {
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
        for (Direction dir = MIN_DIR; dir <= MAX_DIR; dir = DIRECTION(dir + 1)) {
            if (grid.at(currentState.player + dir).isLever()) {
                stepDir = dir;
                break;
            }
        }
        assert(stepDir != NONE);
    }
    return stepDir;
}

using StateQueue = StableQueue<PriorityQueue, State, DeadState, uint32_t>;
using PosQueue = StableQueue<Queue, DeadState, DeadState, uint8_t, false>;

std::vector<Direction> buildSolution(
    const Grid& grid,
    const StateQueue& queue,
    const PosQueue& posQueue,
    const State& lastState
) {
    std::vector<Direction> solution;

    Direction stepDir = getStepDirection(grid, lastState, posQueue.peek());
    solution.push_back(stepDir);

    PosQueue::IndexType currentPosIndex = posQueue.peekIndex();
    while (true) {
        PosQueue::IndexType prevIndex = posQueue.getPrevIndex(currentPosIndex);
        if (prevIndex == PosQueue::NO_INDEX) {
            break;
        }
        const DeadState prevState = posQueue.at(prevIndex);
        const DeadState currentState = posQueue.at(currentPosIndex);
        Direction stepDir = getStepDirection(grid, currentState, prevState);
        solution.push_back(stepDir);
        currentPosIndex = prevIndex;
    }
    
    StateQueue::IndexType currentIndex = queue.peekIndex();
    while (true) {
        StateQueue::IndexType prevIndex = queue.getPrevIndex(currentIndex);
        if (prevIndex == StateQueue::NO_INDEX) {
            break;
        }
        const DeadState prevState = queue.at(prevIndex);
        const DeadState currentState = queue.at(currentIndex);
        Direction stepDir = getStepDirection(grid, currentState, prevState);
        solution.push_back(stepDir);
        currentIndex = prevIndex;
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
    absl::flat_hash_map<StateEncoding, Rank> visited(5000000);
    StateQueue queue(1000000, 5000000);

    Rank bestRank = std::numeric_limits<Rank>::max();
    std::vector<Direction> bestMoves;
    
    constexpr size_t MAX_NEXT_STATES = MAX_OBJECTS * MAX_DIR + MAX_LEVERS;
    // Based on max number of elements we could possibly see in the queue.
    constexpr size_t MAX_POS_QUEUE_SIZE = MAX_OBJECTS * 2;
    constexpr size_t POS_QUEUE_SIZE = std::max(MAX_POS_QUEUE_SIZE, MAX_NEXT_STATES);

    std::array<std::array<bool, 16>, 16> posVisited = {};
    PosQueue posQueue(POS_QUEUE_SIZE, BASE);
    std::array<uint8_t, BASE> posRanks = {};
    std::vector<Position> movable = grid.getAllMovable();

    std::vector<std::pair<State, PosQueue::IndexType>> nextStates;
    nextStates.reserve(MAX_NEXT_STATES);
    
    for (auto& row : posVisited) {
        row.fill(true);
    }

    queue.push(initialState);
    visited[initialState.encode(grid.objectCount)] = initialState.rank;

    stats.iterations = 0;

    while (!queue.empty()) {
        const State& currentState = queue.peek();

        if (currentState.rank > bestRank) {
            break;
        }

        stats.iterations++;
        if (stats.iterations % 100000 == 0) {
            if (done) {
                break;
            }
            std::cout << std::format(
                "\rStates checked: {}. Queue size: {}. Queue dead size: {}. Visited cache: {}. ",
                stats.iterations,
                queue.totalSize(),
                queue.deadSize(),
                visited.size()
            );
            std::cout.flush();
        }

        for (Position pos : movable) {
            posVisited[pos.y()][pos.x()] = false;
        }
        posQueue.clear();
        nextStates.clear();

        posRanks[posQueue.totalSize()] = 0;
        posQueue.push(currentState);

        while (!posQueue.empty()) {
            const auto& posState = posQueue.peek();
            bool& used = posVisited[posState.player.y()][posState.player.x()];
            if (used) {
                posQueue.removeFront();
                continue;
            }
            used = true;

            for (Direction dir = MIN_DIR; dir <= MAX_DIR; dir = DIRECTION(dir + 1)) {
                const auto nextMove = posState.player + dir;
                if (posVisited[nextMove.y()][nextMove.x()]) {
                    continue;
                }

                const auto [nextCell, nextObjIndex] = grid.at(currentState, nextMove);
                if (nextCell.isMovable()) {
                    if (nextCell == FLOOR) {
                        auto nextPosState = posState;
                        nextPosState.player = nextMove;
                        posRanks[posQueue.totalSize()] = posRanks[posQueue.peekIndex()] + 1;
                        posQueue.push(std::move(nextPosState));
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

                        nextState.rank += Rank(posRanks[posQueue.peekIndex()] + 1);
                        auto [it, inserted] = visited.try_emplace(nextState.encode(grid.objectCount), nextState.rank);
                        if (inserted || it->second > nextState.rank) {
                            it->second = nextState.rank;
                            nextStates.push_back({std::move(nextState), posQueue.peekIndex()});
                        }
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
                    nextState.rank += Rank(posRanks[posQueue.peekIndex()] + 1);

                    if (nextState.numToggledLevers() == startList.size() && nextState.rank < bestRank) {
                        bestRank = nextState.rank;
                        bestMoves = buildSolution(grid, queue, posQueue, nextState);
                        std::cout << std::format("Found a solution with {} moves.\n", bestRank);
                    }
                    else {
                        auto [it, inserted] = visited.try_emplace(nextState.encode(grid.objectCount), nextState.rank);
                        if (inserted || it->second > nextState.rank) {
                            it->second = nextState.rank;
                            nextStates.push_back({std::move(nextState), posQueue.peekIndex()});
                        }
                    }
                }
            }

            posQueue.removeFront();
        }

        queue.removeFront();

        posQueue.pushIndexedRange(nextStates);
        posQueue.pruneDeadNodes();

        queue.pushDeadQueue(posQueue, nextStates | std::views::keys);
    }

    stats.visited = visited.size();
    return bestMoves;
}
