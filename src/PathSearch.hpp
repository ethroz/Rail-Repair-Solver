#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "FixedQueue.hpp"
#include "FixedVector.hpp"
#include "StablePriorityQueue.hpp"
#include "State.hpp"

struct IsEmptyList {
    constexpr bool operator()(const std::vector<State>& v) const {
        return v.empty();
    }
};
using EndList = LeverList<std::vector<State>, IsEmptyList>;

class PathSearch {
public:
    constexpr PathSearch(const Grid& grid, const State& startState) :
        m_grid(grid),
        m_startState(startState),
        m_pathQueue(BASE, BASE),
        m_used(m_usedBuffer.data(), 0),
        m_hasPath(m_hasPathBuffer.data(), 0)
    {
        m_used = std::span<bool>(m_usedBuffer).subspan(0, m_grid.objectCount);
        m_hasPath = std::span<bool>(m_hasPathBuffer).subspan(0, m_grid.objectCount);
    }

    constexpr EndList findEndStates(const StartList& startList) {
        EndList endList;
        m_currentState = m_startState;
        for (uint8_t i = 0; i < m_grid.objectCount; ++i) {
            m_currentState.objectPositions[i] = INVALID_POS;
        }
        m_currentState.player = INVALID_POS;
        std::ranges::fill(m_used, false);
        m_requiredHoles.clear();
        m_optionalHoles.clear();
        m_newFoundPaths.clear();

        for (const auto [index, startVec] : startList) {
            m_endStates.clear();
            m_currentLeverPos = m_grid.find(CELL(IMMOVABLE | LEVER | index));
            assert(m_currentLeverPos != INVALID_POS);
            m_numUsed = 0;

            traverseVector(startVec);

            if (!m_endStates.empty()) {
                endList.insert(index, std::move(m_endStates));
            }
        }

        return endList;
    }

private:
    constexpr void traverseVector(Vector v) {
        while (true) {
            v.pos += v.dir;
            Cell cell = m_grid.at(m_currentState, v.pos).cell;

            if (cell.isTrack()) {
                if (!handleTrackCell(v, cell)) {
                    continue;
                }
            }
            else {
                handleEmptyCell(v, cell);
            }

            return;
        }
    }

    constexpr bool handleTrackCell(Vector& v, Cell cell) {
        if (cell.isStart()) {
            return true;
        }

        v.dir = cell.trackType().ride(v.dir);
        if (v.dir == NONE) {
            return true;
        }

        if (!m_grid.exits(v)) {
            return false;
        }

        if (m_requiredHoles.size() + m_numUsed > m_used.size()) {
            return true;
        }

        if (m_optionalHoles.empty()) {
            addAllValidEndStates(m_requiredHoles);
        }
        else {
            handleOptionalHoles();
        }
        return true;
    }

    constexpr void handleEmptyCell(const Vector& v, Cell cell) {
        if (!cell.isEmpty()) {
            return;
        }

        for (size_t i = 0; i < m_used.size(); ++i) {
            if (m_used[i]) {
                continue;
            }

            Cell object = m_currentState.objects[i];
            assert(object.isTrack());
            Direction newDir = object.trackType().ride(v.dir);
            if (newDir == NONE) {
                continue;
            }

            std::vector<Position> path = findPathOverHoles(
                m_startState.objectPositions[i],
                v.pos
            );
            if (path.empty()) {
                continue;
            }

            tryPlaceObject(i, v, newDir, cell, path);
        }
    }

    constexpr void tryPlaceObject(
        size_t objectIndex,
        Vector v,
        Direction newDir,
        Cell cell,
        const std::vector<Position>& path
    ) {
        const bool isOnHole = cell == HOLE;
        if (isOnHole) {
            assert(!std::ranges::contains(m_requiredHoles, v.pos));
            m_requiredHoles.push_back(v.pos);
        }

        const size_t optionalBefore = m_optionalHoles.size();
        addOptionalHoleCandidates(path);

        m_currentState.objectPositions[objectIndex] = v.pos;
        m_used[objectIndex] = true;
        ++m_numUsed;
        traverseVector({m_currentState.objectPositions[objectIndex], newDir});
        --m_numUsed;
        m_used[objectIndex] = false;
        m_currentState.objectPositions[objectIndex] = INVALID_POS;

        m_optionalHoles.resize(optionalBefore);
        if (isOnHole) {
            m_requiredHoles.pop_back();
        }
    }

    constexpr void addOptionalHoleCandidates(std::span<const Position> path) {
        for (const auto& pos : path.subspan(0, path.size() - 1)) {
            if (m_grid.at(pos) == HOLE &&
                !std::ranges::contains(m_requiredHoles, pos) &&
                !std::ranges::contains(m_optionalHoles, pos)) {
                m_optionalHoles.push_back(pos);
            }
        }
    }

    constexpr void handleOptionalHoles() {
        std::ranges::fill(m_hasPath, false);

        size_t completedPaths = 0;
        assert(m_used.size() == m_hasPath.size());
        for (size_t i = 0; i < m_used.size(); ++i) {
            if (!m_used[i]) {
                continue;
            }

            const bool foundPath = hasPathWithoutHoles(
                m_requiredHoles,
                m_startState.objectPositions[i],
                m_currentState.objectPositions[i]
            );
            if (foundPath) {
                m_hasPath[i] = true;
                ++completedPaths;
            }
        }

        tryOptionalHoles(
            m_optionalHoles,
            completedPaths
        );
    }

    constexpr void tryOptionalHoles(
        std::span<Position> remainingHoles,
        size_t completedPaths
    ) {
        if (completedPaths == m_numUsed) {
            if (m_requiredHoles.size() + m_numUsed <= m_used.size()) {
                addAllValidEndStates(m_requiredHoles);
            }
            return;
        }

        for (size_t i = 0; i < remainingHoles.size(); ++i) {
            m_requiredHoles.push_back(remainingHoles[i]);
            const size_t backIndex = remainingHoles.size() - 1;
            if (i != backIndex) {
                std::swap(remainingHoles[i], remainingHoles[backIndex]);
            }

            size_t newPathsBefore = m_newFoundPaths.size();
            for (size_t j = 0; j < m_used.size(); ++j) {
                if (!m_used[j] || m_hasPath[j]) {
                    continue;
                }

                const bool foundPath = hasPathWithoutHoles(
                    m_requiredHoles,
                    m_startState.objectPositions[j],
                    m_currentState.objectPositions[j]
                );
                if (foundPath) {
                    m_hasPath[j] = true;
                    m_newFoundPaths.push_back(j);
                }
            }
            completedPaths += m_newFoundPaths.size() - newPathsBefore;

            tryOptionalHoles(
                remainingHoles.subspan(0, backIndex),
                completedPaths
            );

            completedPaths -= m_newFoundPaths.size() - newPathsBefore;
            for (size_t index : m_newFoundPaths | std::views::drop(newPathsBefore)) {
                m_hasPath[index] = false;
            }
            m_newFoundPaths.resize(newPathsBefore);

            if (i != backIndex) {
                std::swap(remainingHoles[i], remainingHoles[backIndex]);
            }
            m_requiredHoles.pop_back();
        }
    }

    constexpr bool hasPathWithoutHoles(
        std::span<const Position> extraFloors,
        Position from,
        Position to
    ) {
        const auto getCell = [&](const Position& p) -> Cell {
            if (std::ranges::contains(extraFloors, p)) {
                return FLOOR;
            }
            return m_grid.at(p);
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

    constexpr std::vector<Position> findPathOverHoles(Position from, Position to) {
        if (from == to) {
            return {from};
        }

        std::array<std::array<bool, 16>, 16> visited = {};
        m_pathQueue.reset();
        m_pathQueue.push(RankedPosition{from, 0});
        visited[from.y()][from.x()] = true;

        while (!m_pathQueue.empty()) {
            const auto& front = m_pathQueue.peek();

            for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
                const Direction dir = DIRECTION(dirValue);

                const Position backward = front.pos - dir;
                if (!m_grid.at(backward).isEmpty()) {
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
                    for (const auto& pos : m_pathQueue) {
                        path.push_back(pos);
                    }
                    std::reverse(path.begin(), path.end());
                    return path;
                }

                const Cell cell = m_grid.at(forward);
                if (cell.isEmpty()) {
                    uint8_t rank = front.rank + 1;
                    if (cell == HOLE) {
                        constexpr uint8_t HOLE_OFFSET = BASE;
                        rank += HOLE_OFFSET;
                    }
                    m_pathQueue.push(RankedPosition{std::move(forward), std::move(rank)});
                }
            }

            m_pathQueue.removeFront();
        }

        return {};
    }

    constexpr void addAllValidEndStates(std::span<Position> holes) {
        if (holes.empty()) {
            for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
                const Direction dir = DIRECTION(dirValue);
                Position pos = m_currentLeverPos + dir;

                if (m_grid.at(m_currentState, pos).cell.isEmpty()) {
                    m_currentState.player = pos;
                    m_endStates.push_back(m_currentState);
                    m_currentState.player = INVALID_POS;
                }
            }
        }
        else {
            for (size_t i = 0; i < m_used.size(); ++i) {
                if (m_used[i]) {
                    continue;
                }

                Position hole = holes.front();
                const bool foundPath = hasPathWithoutHoles(
                    holes,
                    m_startState.objectPositions[i],
                    hole
                );
                if (!foundPath) {
                    continue;
                }

                m_currentState.objectPositions[i] = std::move(hole);
                Cell cell = std::exchange(m_currentState.objects[i], FLOOR);
                m_used[i] = true;
                addAllValidEndStates(holes.subspan(1));
                m_used[i] = false;
                m_currentState.objects[i] = cell;
                m_currentState.objectPositions[i] = INVALID_POS;
            }
        }
    }

    const Grid& m_grid;
    const State& m_startState;
    StablePriorityQueue<RankedPosition, Position, uint32_t, false> m_pathQueue;
    State m_currentState;
    std::array<bool, MAX_OBJECTS> m_usedBuffer{};
    std::span<bool> m_used;
    std::array<bool, MAX_OBJECTS> m_hasPathBuffer{};
    std::span<bool> m_hasPath;
    FixedVector<Position, MAX_OBJECTS> m_requiredHoles;
    FixedVector<Position, BASE> m_optionalHoles;
    FixedVector<size_t, MAX_OBJECTS> m_newFoundPaths;
    std::vector<State> m_endStates;
    Position m_currentLeverPos = INVALID_POS;
    size_t m_numUsed = 0;
};

EndList findEndStates(
    const Grid& grid,
    const StartList& startList,
    const State& startState
) {
    return PathSearch(grid, startState).findEndStates(startList);
}
