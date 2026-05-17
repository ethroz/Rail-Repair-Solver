#pragma once

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "FixedQueue.hpp"
#include "Grid.hpp"
#include "LeverList.hpp"
#include "State.hpp"

struct IsEmptyList {
    constexpr bool operator()(const std::vector<GoalState>& v) const {
        return v.empty();
    }
};
class EndList : public LeverList<std::vector<GoalState>, IsEmptyList> {
public:
    constexpr EndList() noexcept = default;
    constexpr ~EndList() noexcept = default;

    constexpr void connectLists(const Grid& grid) {
        for (uint8_t from = 0; from < MAX_LEVERS; ++from) {
            if (!has(from)) {
                continue;
            }
            for (uint8_t to = 0; to < MAX_LEVERS; ++to) {
                if (from == to || !has(to)) {
                    continue;
                }
                auto& links = getEdges(m_links, from, to);

                for (const auto [fromIndex, fromState] : m_data[from] | std::views::enumerate) {
                    for (const auto [toIndex, toState] : m_data[to] | std::views::enumerate) {
                        if (canTransformTo(grid, fromState, toState)) {
                            links.emplace_back(fromIndex, toIndex);
                        }
                    }
                }
            }
        }
        
        prune();
    }

    constexpr std::string nodeSummary() const {
        std::string summary;
        summary += '{';
        bool first = true;
        for (const auto [index, endStates] : *this) {
            if (!first) {
                summary += ", ";
            }
            first = false;
            summary += std::to_string(int(index + 1));
            summary += ": ";
            summary += std::to_string(endStates.size());
        }
        summary += '}';
        return summary;
    }

    constexpr std::string edgeSummary() const {
        std::string summary;
        summary += '{';
        bool first = true;
        for (uint8_t from = 0; from < MAX_LEVERS; ++from) {
            if (!has(from)) {
                continue;
            }
            if (!first) {
                summary += ", ";
            }
            first = false;

            bool first2 = true;
            for (uint8_t to = 0; to < MAX_LEVERS; ++to) {
                if (from == to || !has(to)) {
                    continue;
                }
                const auto& links = getEdges(m_links, from, to);

                if (first2) {
                    summary += std::to_string(int(from + 1));
                    summary += ": {";
                }
                else {
                    summary += ", ";
                }
                first2 = false;
                summary += std::to_string(int(to + 1));
                summary += ": ";
                summary += std::to_string(links.size());
            }

            if (!first2) {
                summary += '}';
            }
        }
        summary += '}';
        return summary;
    }

private:
    template<typename T>
    using EdgeContainer = NodeContainer<std::array<T, MAX_LEVERS - 1>>;

    template<typename T>
    static constexpr T& getEdges(EdgeContainer<T>& container, uint8_t from, uint8_t to) {
        switch (from) {
        case 0: return container[from][to - 1];
        case 1: return container[from][to / 2];
        case 2: return container[from][to];
        default: throw std::invalid_argument("Index out of range");
        }
    }

    template<typename T>
    static constexpr const T& getEdges(const EdgeContainer<T>& container, uint8_t from, uint8_t to) {
        switch (from) {
        case 0: return container[from][to - 1];
        case 1: return container[from][to / 2];
        case 2: return container[from][to];
        default: throw std::invalid_argument("Index out of range");
        }
    }

    constexpr bool canTransformTo(const Grid& grid, const GoalState& from, const GoalState& to) const {
        for (uint8_t i = 0; i < grid.objectCount; ++i) {
            const Cell& fromCell = from.objects[i];
            const Position& fromPos = from.objectPositions[i];
            const Cell& toCell = to.objects[i];
            const Position& toPos = to.objectPositions[i];

            if (fromPos == INVALID_POS || toPos == INVALID_POS) {
                continue;
            }

            if (fromCell == toCell || fromCell != FLOOR) {
                if (fromCell == FLOOR) {
                    if (fromPos != toPos) {
                        return false;
                    }
                    continue;
                }

                if (!hasPath(grid, fromPos, toPos)) {
                    return false;
                }
            }
            else {
                return false;
            }
        }

        return true;
    }

    constexpr bool hasPath(const Grid& grid, Position from, Position to) const {
        std::array<std::array<bool, 16>, 16> visited = {};
        FixedQueue<Position, BASE> queue;
        queue.push(from);
        visited[from.y()][from.x()] = true;

        while (!queue.empty()) {
            const auto front = queue.pop();

            for (uint8_t dirValue = MIN_DIR; dirValue <= MAX_DIR; ++dirValue) {
                const Direction dir = DIRECTION(dirValue);

                const Position backward = front - dir;
                if (!grid.at(backward).isEmpty()) {
                    continue;
                }

                Position forward = front + dir;
                bool& used = visited[forward.y()][forward.x()];
                if (used) {
                    continue;
                }
                used = true;

                const Cell cell = grid.at(forward);
                if (cell.isEmpty()) {
                    if (forward == to) {
                        return true;
                    }
                    queue.push(std::move(forward));
                }
            }
        }

        return false;
    }

    constexpr void prune() {
        if (m_size < 2) {
            return;
        }
        size_t minDegree = m_size - 1;

        decltype(m_data) newData;
        decltype(m_links) newLinks;

        NodeContainer<absl::flat_hash_map<size_t, size_t>> nodeDegrees;

        for (uint8_t from = 0; from < MAX_LEVERS; ++from) {
            for (uint8_t to = 0; to < MAX_LEVERS; ++to) {
                if (from == to) {
                    continue;
                }
                const auto& edges = getEdges(m_links, from, to);
                auto& fromDegree = nodeDegrees[from];
                auto& toDegree = nodeDegrees[to];
                for (const auto [fromIndex, toIndex] : edges) {
                    ++fromDegree.try_emplace(fromIndex, size_t(0)).first->second;
                    ++toDegree.try_emplace(toIndex, size_t(0)).first->second;
                }
            }
        }

        NodeContainer<absl::flat_hash_map<size_t, size_t>> oldToNew;

        for (uint8_t i = 0; i < MAX_LEVERS; ++i) {
            const auto& degree = nodeDegrees[i];
            const auto& nodes = m_data[i];
            auto& newNodes = newData[i];
            auto& remap = oldToNew[i];
            for (size_t oldIndex = 0; oldIndex < nodes.size(); ++oldIndex) {
                if (auto it = degree.find(oldIndex);
                    it != degree.end() && it->second >= minDegree
                ) {
                    size_t newIndex = newNodes.size();
                    remap[oldIndex] = newIndex;
                    newNodes.push_back(nodes[oldIndex]);
                }
            }
        }

        for (uint8_t from = 0; from < MAX_LEVERS; ++from) {
            for (uint8_t to = 0; to < MAX_LEVERS; ++to) {
                if (from == to) {
                    continue;
                }
                const auto& edges = getEdges(m_links, from, to);
                const auto& fromRemap = oldToNew[from];
                const auto& toRemap = oldToNew[to];
                auto& newEdges = getEdges(newLinks, from, to);
                for (const auto& [fromIndex, toIndex] : edges) {
                    auto fromIt = fromRemap.find(fromIndex);
                    auto toIt = toRemap.find(toIndex);

                    // Keep only edges where both endpoints survived
                    if (fromIt != fromRemap.end() && toIt != toRemap.end()) {
                        newEdges.emplace_back(fromIt->second, toIt->second);
                    }
                }
            }
        }

        m_data = std::move(newData);
        m_links = std::move(newLinks);
    }

    using Link = std::pair<size_t, size_t>;
    EdgeContainer<std::vector<Link>> m_links = {};
};
