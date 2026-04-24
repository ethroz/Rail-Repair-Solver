#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "Queue.hpp"

static uint8_t width = 0;
static uint8_t height = 0;
static uint8_t movableSpaces = 0;
static constexpr size_t maxDynamicSpaces = (X_MAX - 2) * (Y_MAX - 2);
static uint8_t dynamicSpaces = 0;
static std::array<Position, maxDynamicSpaces> dynamicPositionLookup{};
static uint8_t leverSpaces = 0;
static std::array<Position, X_MAX * Y_MAX> movablePositionLookup{};
static std::array<Position, MAX_LEVERS> leverPositionLookup{};
constexpr size_t numBits = sizeof(size_t) * 8;
static_assert(numBits == 64);

struct CellDescriptor {
    char character;
    bool edge;
};

constexpr bool operator==(CellDescriptor a, CellDescriptor b) { return a.character == b.character && a.edge == b.edge; }

template<>
struct std::hash<CellDescriptor> {
    size_t operator()(const CellDescriptor& cd) const noexcept {
        size_t hash = size_t(cd.character);
        hash |= size_t(cd.edge ? 1 : 0) << 8;
        return hash;
    }
};

size_t gridKey(const Grid& grid) noexcept {
    constexpr uint64_t offsetBasis = 14695981039346656037ULL;
    constexpr uint64_t prime = 1099511628211ULL;
    const auto* data = reinterpret_cast<const uint8_t*>(&grid);
    uint64_t hash = offsetBasis;
    size_t offset = 0;

    while (offset + sizeof(uint64_t) <= sizeof(Grid)) {
        uint64_t chunk = 0;
        std::memcpy(&chunk, data + offset, sizeof(chunk));
        hash ^= chunk;
        hash *= prime;
        offset += sizeof(uint64_t);
    }

    if (offset < sizeof(Grid)) {
        uint64_t tail = 0;
        std::memcpy(&tail, data + offset, sizeof(Grid) - offset);
        hash ^= tail;
        hash *= prime;
    }

    return size_t(hash ^ (hash >> 32));
}

template<>
struct std::hash<Grid> {
    size_t operator()(const Grid& grid) const noexcept {
        return gridKey(grid);
    }
};

uint8_t cellValue(Cell cell) noexcept {
    uint8_t value = 0;
    static_assert(sizeof(Cell) == sizeof(value));
    std::memcpy(&value, &cell, sizeof(value));
    return value;
}

struct StateKey {
    std::array<uint8_t, maxDynamicSpaces> cells{};
};

bool operator==(const StateKey& a, const StateKey& b) noexcept {
    return std::memcmp(a.cells.data(), b.cells.data(), dynamicSpaces) == 0;
}

StateKey makeStateKey(const Grid& grid) noexcept {
    StateKey key;
    for (uint8_t i = 0; i < dynamicSpaces; i++) {
        key.cells[i] = cellValue(grid.at(dynamicPositionLookup[i]));
    }
    return key;
}

template<>
struct std::hash<StateKey> {
    size_t operator()(const StateKey& key) const noexcept {
        constexpr uint64_t offsetBasis = 14695981039346656037ULL;
        constexpr uint64_t prime = 1099511628211ULL;
        const auto* data = key.cells.data();
        uint64_t hash = offsetBasis;
        size_t offset = 0;

        while (offset + sizeof(uint64_t) <= dynamicSpaces) {
            uint64_t chunk = 0;
            std::memcpy(&chunk, data + offset, sizeof(chunk));
            hash ^= chunk;
            hash *= prime;
            offset += sizeof(uint64_t);
        }

        if (offset < dynamicSpaces) {
            uint64_t tail = 0;
            std::memcpy(&tail, data + offset, dynamicSpaces - offset);
            hash ^= tail;
            hash *= prime;
        }

        return size_t(hash ^ (hash >> 32));
    }
};

const static std::unordered_map<CellDescriptor, Cell> LEGEND = {
    {{'@', false}, PLAYER},
    {{'#', false}, WALL},
    {{'#', true }, WALL},
    {{'*', false}, HOLE},
    {{' ', false}, FLOOR},
    {{'1', false}, LEVER1_OFF},
    {{'2', false}, LEVER2_OFF},
    {{'3', false}, LEVER3_OFF},
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

State stateFromString(std::string_view board) {
    State state;

    size_t temp = board.find_first_of("\r\n");
    if (temp < 2 || temp > X_MAX) {
        throw std::invalid_argument("Invalid level width");
    }
    width = uint8_t(temp);
    temp = std::count(board.begin(), board.end(), '\n');
    if (temp < 2 || temp > Y_MAX) {
        throw std::invalid_argument("Invalid level height");
    }
    height = uint8_t(temp);

    Position pos;
    uint8_t numLevers = 0;
    uint8_t numObjects = 0;
    movableSpaces = 0;
    dynamicSpaces = 0;
    leverSpaces = 0;
    for (size_t i = 0; i < board.size(); i++) {
        const char character = board[i];
        if (character == '\n') {
            if (pos.x != width) {
                throw std::invalid_argument("Inconsistent level widths");
            }
            pos.x = 0;
            pos.y++;
            if (pos.y == height) {
                break;
            }
            else {
                continue;
            }
        }
        else if (character == '\r') {
            continue;
        }

        const bool top = pos.y == 0;
        const bool right = pos.x == width - 1;
        const bool bottom = pos.y == height - 1;
        const bool left = pos.x == 0;
        const bool edge = top || bottom || left || right;
        if (!LEGEND.contains({ character, edge })) {
            throw std::invalid_argument(std::format("Unrecognized cell description: [{}, {}]", character, edge ? "edge" : "middle"));
        }
        const auto cell = LEGEND.at(CellDescriptor{ character, edge });
        state.grid.at(pos) = cell;

        if (cell.isMovable() || cell == HOLE || cell.isLever()) {
            assert(dynamicSpaces < dynamicPositionLookup.size());
            dynamicPositionLookup[dynamicSpaces++] = pos;
        }

        if (cell.isMovable()) {
            movablePositionLookup[movableSpaces] = pos;
            if (cell != FLOOR) {
                numObjects++;
            }
            movableSpaces++;
        }

        if (cell.isLever()) {
            leverPositionLookup[leverSpaces++] = pos;
            numLevers++;
        }
        else if (cell == PLAYER) {
            assert(!edge);
            if (state.player.x != 0) {
                throw std::invalid_argument("Cannot have more than one player");
            }

            state.player = pos;
        }

        pos.x++;
    }

    if (state.player.x == 0) {
        throw std::invalid_argument("Missing a player");
    }
    if (numLevers > MAX_LEVERS || numLevers == 0) {
        throw std::invalid_argument(std::format("Invalid number of levers: {}", numLevers));
    }
    if (numObjects > MAX_OBJECTS) {
        throw std::invalid_argument(std::format("Invalid number of objects: {}", numObjects));
    }

    size_t maxPossibilities = (size_t(1) << (numBits - numLevers)) - 1;
    size_t hash = 1;
    for (uint8_t i = 0; i < numObjects; i++) {
        const auto factor = movableSpaces - i;
        if (maxPossibilities / factor < hash) {
            throw std::logic_error(std::format("Cannot store all the possible states in a hash of {} bits", numBits));
        }
        hash *= factor;
    }

    return state;
}

using StartList = std::unordered_map<uint8_t, std::pair<Position, Direction>>;

StartList createStartList(const State& state) {
    StartList list;

    for (int8_t x = 0; x < width; x++) {
        for (int8_t y = 0; y < height; y++) {
            const auto cell = state.grid.at(x, y);
            if (cell.isStart()) {
                Direction dir;

                const bool top = y == 0;
                const bool right = x == width - 1;
                const bool bottom = y == height - 1;
                const bool left = x == 0;
                uint8_t flags = (top ? 0b1000 : 0) | (right ? 0b0100 : 0) | (bottom ? 0b0010 : 0) | (left ? 0b0001 : 0);
                switch (flags) {
                case 0b1000: dir = DOWN;  break;
                case 0b0100: dir = LEFT;  break;
                case 0b0010: dir = UP;    break;
                case 0b0001: dir = RIGHT; break;
                default: throw std::invalid_argument("Cannot have a starting railroad on a corner"); break;
                }

                if (list.contains(cell.index())) {
                    throw std::invalid_argument("Cannot have two of the same starting railroads");
                }

                list.insert({ cell.index(), { { x, y }, dir } });
            }
        }
    }

    return list;
}

bool simulateTrain(const StartList& startList, const Grid& grid, uint8_t index) {
    auto [pos, dir] = startList.at(index);
    while (dir != NONE) {
        if (!grid.at(pos).isTrack()) {
            return false;
        }

        const bool exitsGrid =
            (pos.y == 0 && dir == UP) ||
            (pos.x == width - 1 && dir == RIGHT) ||
            (pos.y == height - 1 && dir == DOWN) ||
            (pos.x == 0 && dir == LEFT);
        if (exitsGrid) {
            return true;
        }

        pos += dir;
        dir = trackToDirection(grid.at(pos), dir);
    }
    return false;
}

struct SearchState {
    Grid grid{};
    Position player{};
    uint8_t toggledLevers = 0;
    uint32_t pathIndex = std::numeric_limits<uint32_t>::max();
};

struct PathNode {
    uint32_t parent = std::numeric_limits<uint32_t>::max();
    Direction move = NONE;
};

std::vector<Direction> buildSolution(const std::vector<PathNode>& path, uint32_t nodeIndex) {
    std::vector<Direction> moves;

    while (path[nodeIndex].parent != std::numeric_limits<uint32_t>::max()) {
        moves.push_back(path[nodeIndex].move);
        nodeIndex = path[nodeIndex].parent;
    }

    std::reverse(moves.begin(), moves.end());
    return moves;
}

std::vector<Direction> search(const StartList& startList, const State& initialState, const std::atomic_bool& done = {}) {
    std::unordered_set<StateKey> visited(3000000);
    Queue<SearchState> queue(5000000);
    std::vector<PathNode> path;
    const auto goalLevers = uint8_t(startList.size());

    path.push_back({});
    queue.push({ initialState.grid, initialState.player, initialState.toggledLevers, 0 });
    visited.insert(makeStateKey(initialState.grid));

    size_t count = 0;

    while (!queue.empty()) {
        const SearchState current = queue.pop();

        count++;
        if (count % 1000000 == 0) {
            std::cout << std::format("States checked: {}\r", count);
            std::cout.flush();
            if (done) {
                break;
            }
        }

        for (Direction dir = MIN_DIR; dir < MAX_DIR; dir = Direction(dir + 1)) {
            SearchState nextState = current;
            nextState.player += Position(dir);
            const auto nextCell = nextState.grid.at(nextState.player);

            assert(nextCell != PLAYER);
            if (nextCell.isMovable()) {
                if (nextCell != FLOOR) {
                    const auto nextNextMove = nextState.player + Position(dir);
                    const auto nextNextCell = nextState.grid.at(nextNextMove);
                    if (!nextNextCell.isEmpty()) {
                        continue;
                    }

                    if (nextNextCell == HOLE) {
                        nextState.grid.at(nextNextMove) = FLOOR;
                    }
                    else {
                        nextState.grid.at(nextNextMove) = nextState.grid.at(nextState.player);
                    }
                }

                nextState.grid.at(nextState.player) = PLAYER;
                nextState.grid.at(current.player) = FLOOR;
            }
            else if (nextCell.isLever() && !nextCell.leverState() && simulateTrain(startList, nextState.grid, nextCell.index())) {
                nextState.toggledLevers++;

                if (nextState.toggledLevers == goalLevers) {
                    path.push_back({ current.pathIndex, dir });
                    std::cout << "Total iterations: " << count << std::endl;
                    std::cout << "visited size: " << visited.size() << std::endl;
                    return buildSolution(path, uint32_t(path.size() - 1));
                }

                nextState.grid.at(nextState.player).toggleLever();
                nextState.player = current.player;
            }
            else {
                continue;
            }

            if (!visited.insert(makeStateKey(nextState.grid)).second) {
                continue;
            }

            nextState.pathIndex = uint32_t(path.size());
            path.push_back({ current.pathIndex, dir });
            queue.push(std::move(nextState));
        }
    }

    return {};
}
