#pragma once

#include <vector>

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "FixedQueue.hpp"
#include "FixedVector.hpp"
#include "StablePriorityQueue.hpp"

using PathQueue = StablePriorityQueue<RankedPosition, Position, uint32_t, false>;
static PathQueue pathQueue(BASE, BASE);

std::vector<Position> findPathOverHoles(
    const Grid& grid,
    Position from,
    Position to
) {
    if (from == to) {
        return {from};
    }
    
    std::array<std::array<bool, 16>, 16> visited = {};
    pathQueue.reset();
    pathQueue.push(RankedPosition{from, 0});
    visited[from.y()][from.x()] = true;
    
    while (!pathQueue.empty()) {
        const auto& front = pathQueue.peek();
        
        for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
            const Direction dir = DIRECTION(dirValue);
            
            const Position backward = front.pos - dir;
            if (!grid.at(backward).isEmpty()) {
                continue;
            }
            
            Position forward = front.pos + dir;
            bool& used = visited[forward.y()][forward.x()];
            if (used) {
                continue;
            }
            used = true;

            if (forward == to) {
                std::vector<Position> path;
                path.push_back(to);
                for (const auto& pos : pathQueue) {
                    path.push_back(pos);
                }
                std::reverse(path.begin(), path.end());
                return path;
            }

            const Cell cell = grid.at(forward);
            if (cell.isEmpty()) {
                uint8_t rank = front.rank + 1;
                if (cell == HOLE) {
                    constexpr uint8_t HOLE_OFFSET = BASE;
                    rank += HOLE_OFFSET;
                }
                pathQueue.push(RankedPosition{std::move(forward), std::move(rank)});
            }
        }

        pathQueue.removeFront();
    }

    return {};
}

bool hasPathWithoutHoles(
    const Grid& grid,
    std::span<const Position> extraFloors,
    Position from,
    Position to
) {
    const auto getCell = [&](const Position& p) -> Cell {
        if (std::ranges::contains(extraFloors, p)) {
            return FLOOR;
        }
        else {
            return grid.at(p);
        }
    };

    if (from == to) {
        return getCell(to) == FLOOR;
    }
    
    std::array<std::array<bool, 16>, 16> visited = {};
    FixedQueue<Position, BASE> queue;
    queue.push(from);
    visited[from.y()][from.x()] = true;
    
    while (!queue.empty()) {
        const auto front = queue.pop();
        
        for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
            const Direction dir = DIRECTION(dirValue);
            
            const Position backward = front - dir;
            if (!getCell(backward).isEmpty()) {
                continue;
            }
            
            Position forward = front + dir;
            bool& used = visited[forward.y()][forward.x()];
            if (used) {
                continue;
            }
            used = true;

            const Cell cell = getCell(forward);
            if (cell == FLOOR) {
                if (forward == to) {
                    return true;
                }
                queue.push(std::move(forward));
            }
        }
    }

    return false;
}

void addAllValidEndStates(
    const Grid& grid,
    const State& startState,
    std::vector<State>& endStates,
    State& state,
    std::span<bool> used,
    const Position& leverPos,
    std::span<Position> holes
) {
    if (holes.empty()) {
        for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
            const Direction dir = DIRECTION(dirValue);
            Position pos = leverPos + dir;

            if (grid.at(state, pos).cell.isEmpty()) {
                state.player = pos;
                endStates.push_back(state);
                state.player = INVALID_POS;
            }
        }
    }
    else {
        for (size_t i = 0; i < used.size(); ++i) {
            if (used[i]) {
                continue;
            }
    
            Position hole = holes.front();
            const bool foundPath = hasPathWithoutHoles(
                grid,
                holes,
                startState.objectPositions[i],
                hole
            );
            if (!foundPath) {
                continue;
            }
    
            state.objectPositions[i] = std::move(hole);
            Cell cell = std::exchange(state.objects[i], FLOOR);
            used[i] = true;
            addAllValidEndStates(grid, startState, endStates, state, used, leverPos, holes.subspan(1));
            used[i] = false;
            state.objects[i] = cell;
            state.objectPositions[i] = INVALID_POS;
        }
    }
}

struct IsEmptyList {
    constexpr bool operator()(const std::vector<State>& v) const {
        return v.empty();
    }
};
using EndList = LeverList<std::vector<State>, IsEmptyList>;

EndList findEndStates(
    const Grid& grid,
    const StartList& startList,
    const State& startState
) {
    EndList endList;
    State state = startState;
    for (uint8_t i = 0; i < grid.objectCount; ++i) {
        state.objectPositions[i] = INVALID_POS;
    }
    state.player = INVALID_POS;
    
    std::array<bool, MAX_OBJECTS> usedBuffer{};
    std::span<bool> used = std::span(usedBuffer).subspan(0, grid.objectCount);

    std::array<bool, MAX_OBJECTS> hasPathBuffer{};
    std::span<bool> hasPath = std::span(hasPathBuffer).subspan(0, grid.objectCount);
    
    FixedVector<Position, MAX_OBJECTS> requiredHoles;
    FixedVector<Position, BASE> optionalHoles;

    for (const auto [index, startVec] : startList) {
        std::vector<State> endStates;
        Position leverPos = grid.find(CELL(IMMOVABLE | LEVER | index));
        assert(leverPos != INVALID_POS);
        [&](this auto&& self, size_t numUsed, Vector v) -> void {
            while (true) {
                v.pos += v.dir;
                Cell cell = grid.at(state, v.pos).cell;

                if (cell.isTrack()) {
                    if (cell.isStart()) {
                        return;
                    }

                    v.dir = cell.trackType().ride(v.dir);
                    if (v.dir == NONE) {
                        return;
                    }

                    if (grid.exits(v)) {
                        if (requiredHoles.size() + numUsed > used.size()) {
                            return;
                        }
                        if (optionalHoles.empty()) {
                            addAllValidEndStates(
                                grid,
                                startState,
                                endStates,
                                state,
                                used,
                                leverPos,
                                requiredHoles
                            );
                        }
                        else {
                            std::ranges::fill(hasPath, false);

                            size_t completedPaths = 0;
                            assert(used.size() == hasPath.size());
                            for (size_t i = 0; i < used.size(); ++i) {
                                if (!used[i]) {
                                    continue;
                                }

                                const bool foundPath = hasPathWithoutHoles(
                                    grid,
                                    requiredHoles,
                                    startState.objectPositions[i],
                                    state.objectPositions[i]
                                );
                                if (foundPath) {
                                    hasPath[i] = true;
                                    ++completedPaths;
                                }
                            }

                            [&](this auto&& self, std::span<Position> remainingHoles) -> void {
                                if (completedPaths == numUsed) {
                                    if (requiredHoles.size() + numUsed <= used.size()) {
                                        addAllValidEndStates(
                                            grid,
                                            startState,
                                            endStates,
                                            state,
                                            used,
                                            leverPos,
                                            requiredHoles
                                        );
                                    }
                                }
                                else {
                                    for (size_t i = 0; i < remainingHoles.size(); ++i) {
                                        requiredHoles.push_back(remainingHoles[i]);
                                        const size_t backIndex = remainingHoles.size() - 1;
                                        if (i != backIndex) {
                                            std::swap(remainingHoles[i], remainingHoles[backIndex]);
                                        }
    
                                        std::vector<size_t> newFoundPaths;
                                        for (size_t i = 0; i < used.size(); ++i) {
                                            if (!used[i] || hasPath[i]) {
                                                continue;
                                            }
    
                                            const bool foundPath = hasPathWithoutHoles(
                                                grid,
                                                requiredHoles,
                                                startState.objectPositions[i],
                                                state.objectPositions[i]
                                            );
                                            if (foundPath) {
                                                hasPath[i] = true;
                                                ++completedPaths;
                                                newFoundPaths.push_back(i);
                                            }
                                        }
                                        self(remainingHoles.subspan(0, backIndex));
                                        for (size_t index : newFoundPaths) {
                                            hasPath[index] = false;
                                        }
                                        completedPaths -= newFoundPaths.size();
    
                                        if (i != backIndex) {
                                            std::swap(remainingHoles[i], remainingHoles[backIndex]);
                                        }
                                        requiredHoles.pop_back();
                                    }
                                }
                            }(optionalHoles);
                        }
                        return;
                    }
                }
                else {
                    if (cell.isEmpty()) {
                        for (size_t i = 0; i < used.size(); ++i) {
                            if (used[i]) {
                                continue;
                            }

                            Cell object = state.objects[i];
                            assert(object.isTrack());
                            Direction newDir = object.trackType().ride(v.dir);
                            if (newDir != NONE) {
                                std::vector<Position> path = findPathOverHoles(
                                    grid,
                                    startState.objectPositions[i],
                                    v.pos
                                );

                                if (!path.empty()) {
                                    const bool isOnHole = cell == HOLE;
                                    if (isOnHole) {
                                        assert(!std::ranges::contains(requiredHoles, v.pos));
                                        requiredHoles.push_back(v.pos);
                                    }
                                    const size_t optionalBefore = optionalHoles.size();
                                    for (const auto& pos : path | std::views::take(path.size() - 1)) {
                                        if (grid.at(pos) == HOLE &&
                                            !std::ranges::contains(requiredHoles, pos) &&
                                            !std::ranges::contains(optionalHoles, pos)) {
                                            optionalHoles.push_back(pos);
                                        }
                                    }

                                    state.objectPositions[i] = v.pos;
                                    used[i] = true;
                                    self(numUsed + 1, {state.objectPositions[i], newDir});
                                    used[i] = false;
                                    state.objectPositions[i] = INVALID_POS;

                                    optionalHoles.resize(optionalBefore);
                                    if (isOnHole) {
                                        requiredHoles.pop_back();
                                    }
                                }
                            }
                        }
                    }
                    return;
                }
            }
        }(0, startVec);

        if (!endStates.empty()) {
            endList.insert(index, std::move(endStates));
        }
    }

    return endList;
}
