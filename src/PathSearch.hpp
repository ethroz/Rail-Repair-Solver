#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "FixedQueue.hpp"
#include "FixedVector.hpp"
#include "PriorityChainQueue.hpp"
#include "State.hpp"

struct IsEmptyList {
    constexpr bool operator()(const std::vector<State>& v) const {
        return v.empty();
    }
};
using EndList = LeverList<std::vector<State>, IsEmptyList>;

class PathSearch {
public:
    inline PathSearch(const Grid& grid, const StartList& startList, const State& startState) :
        m_grid(grid),
        m_startList(startList),
        m_startState(startState),
        m_pathQueue(BASE, BASE),
        m_used(m_usedBuffer.data(), 0),
        m_hasPath(m_hasPathBuffer.data(), 0)
    {
        m_used = std::span<bool>(m_usedBuffer).subspan(0, m_grid.objectCount);
        m_hasPath = std::span<bool>(m_hasPathBuffer).subspan(0, m_grid.objectCount);
    }

    constexpr EndList findEndStates() {
        EndList endList;
        m_currentState = m_startState;
        for (uint8_t i = 0; i < m_grid.objectCount; ++i) {
            m_currentState.objectPositions[i] = INVALID_POS;
        }
        m_currentState.player = INVALID_POS;
        std::ranges::fill(m_used, false);
        
        for (const auto [index, startVec] : m_startList) {
            m_endStates.clear();
            m_holes.clear();
            m_currentLeverPos = m_grid.find(CELL(IMMOVABLE | LEVER | index));
            assert(m_currentLeverPos != INVALID_POS);
            m_numUsed = 0;

            traverseVector(startVec);

            if (!m_endStates.empty()) {
                std::vector<State> uniqueStates;
                for (auto const& state : m_endStates) {
                    if (!std::ranges::contains(uniqueStates, state)) {
                        uniqueStates.push_back(state);
                    }
                }
                endList.insert(index, std::move(uniqueStates));
            }
        }

        return endList;
    }

private:
    using HoleList = FixedVector<Position, MAX_OBJECTS>;

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

        if (m_holes.size() + m_numUsed > m_used.size()) {
            return true;
        }

        addAllValidEndStates(m_holes);
        return true;
    }

    constexpr HoleMask holeMask() const {
        HoleMask mask = 0;
        for (const auto& hole : m_holes) {
            const uint8_t bit = hole.rank();
            assert(bit < std::numeric_limits<HoleMask>::digits);
            const auto maskBit = HoleMask(1) << bit;
            if (mask & maskBit) {
                continue;
            }
            mask |= maskBit;
        }
        return mask;
    }

    constexpr PositionalEncoding posEncode() const {
        PositionalEncoding encoding = 0;
        constexpr size_t ENC_BITS = sizeof(encoding) * 8;
        constexpr auto POS_BITS = std::bit_width([](uint64_t base, int exp) constexpr {
            uint64_t result = 1;
            while (exp > 0) { result *= base; --exp; }
            return result;
        }(BASE + 1, MAX_OBJECTS) - 1);
        static_assert(POS_BITS <= ENC_BITS);

        PositionalEncoding multiplier = 1;
        for (uint8_t i = 0; i < m_grid.objectCount; ++i) {
            if (m_currentState.objectPositions[i] == INVALID_POS) {
                encoding += BASE * multiplier;
            }
            else {
                encoding += PositionalEncoding(m_currentState.objectPositions[i].rank()) * multiplier;
            }
            multiplier *= BASE + 1;
        }

        return encoding;
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

            auto paths = findPathsOverHoles(
                m_startState.objectPositions[i],
                v.pos
            );
            if (paths.empty()) {
                continue;
            }

            for (const auto& holeList : paths) {
                placeObject(i, holeList, {v.pos, newDir});
            }
        }
    }
    
    
    constexpr void placeObject(size_t objectIndex, const HoleList& holeList, Vector v) {
        const size_t holesBefore = m_holes.size();
        addHoleCandidates(holeList);

        m_currentState.objectPositions[objectIndex] = v.pos;
        m_used[objectIndex] = true;
        ++m_numUsed;
        traverseVector({m_currentState.objectPositions[objectIndex], v.dir});
        --m_numUsed;
        m_used[objectIndex] = false;
        m_currentState.objectPositions[objectIndex] = INVALID_POS;

        m_holes.resize(holesBefore);
    }

    constexpr void addHoleCandidates(const HoleList& holeList) {
        for (const auto& pos : holeList) {
            if (!std::ranges::contains(m_holes, pos)) {
                m_holes.push_back(pos);
            }
        }
    }
    
    constexpr bool hasPathWithoutHoles(
        std::span<const Position> extraFloors,
        Position from,
        Position to
    ) const {
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

    constexpr std::vector<HoleList> findPathsOverHoles(Position from, Position to) {
        std::vector<HoleList> paths;
        absl::flat_hash_set<RankedPosition> visited;
        int bestRank = std::numeric_limits<int>::max();
        m_pathQueue.reset();
        m_pathQueue.push(RankedPosition{0, from});

        while (!m_pathQueue.empty()) {
            const auto& front = m_pathQueue.peek();

            if (front.rank() > bestRank) {
                break;
            }

            if (!visited.insert(front).second) {
                m_pathQueue.removeFront();
                continue;
            }

            if (front.pos == to) {
                if (front.rank() <= bestRank) {
                    assert(paths.empty() || front.rank() == bestRank);
                    bestRank = front.rank();
                    HoleList list;
                    for (const auto& pos : m_pathQueue) {
                        if (m_grid.at(pos) == HOLE) {
                            list.push_back(pos);
                        }
                    }
                    std::reverse(list.begin(), list.end());
                    paths.push_back(std::move(list));
                }
            }

            for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
                const Direction dir = DIRECTION(dirValue);

                const Position backward = front.pos - dir;
                if (!m_grid.at(backward).isEmpty()) {
                    continue;
                }

                Position forward = front.pos + dir;
                const Cell cell = m_grid.at(forward);
                if (cell.isEmpty()) {
                    auto mask = front.mask;
                    if (cell == HOLE) {
                        const uint8_t bit = forward.rank();
                        assert(bit < std::numeric_limits<HoleMask>::digits);
                        const auto maskBit = HoleMask(1) << bit;
                        if (mask & maskBit) {
                            continue;
                        }
                        mask |= maskBit;
                    }
                    m_pathQueue.push(RankedPosition{std::move(mask), std::move(forward)});
                }
            }

            m_pathQueue.removeFront();
        }

        return paths;
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
    const StartList& m_startList;
    const State& m_startState;
    PriorityChainQueue<RankedPosition, Position, uint32_t, false> m_pathQueue;
    State m_currentState;
    std::array<bool, MAX_OBJECTS> m_usedBuffer{};
    std::span<bool> m_used;
    std::array<bool, MAX_OBJECTS> m_hasPathBuffer{};
    std::span<bool> m_hasPath;
    FixedVector<Position, BASE> m_holes;
    std::vector<State> m_endStates;
    Position m_currentLeverPos = INVALID_POS;
    size_t m_numUsed = 0;
};

EndList findEndStates(
    const Grid& grid,
    const StartList& startList,
    const State& startState
) {
    return PathSearch(grid, startList, startState).findEndStates();
}
