#include <filesystem>
#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_set>

#include "Test.hpp"

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "GameLogic.hpp"
#include "FixedIndexedQueue.hpp"
#include "FixedIndexedVector.hpp"
#include "IndexedPriorityQueue.hpp"
#include "IndexedVector.hpp"
#include "StableFixedQueue.hpp"
#include "StablePriorityQueue.hpp"


template <>
struct std::hash<Position> {
    std::size_t operator()(const Position& p) const noexcept {
        return std::hash<uint8_t>{}(p.value());
    }
};

TEST(Tracks_NW) {
    const TrackType t = NW;
    {
        const auto res = t.ride(UP);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(RIGHT);
        EXPECT_EQ(UP, res);
    }
    {
        const auto res = t.ride(DOWN);
        EXPECT_EQ(LEFT, res);
    }
    {
        const auto res = t.ride(LEFT);
        EXPECT_EQ(NONE, res);
    }
}

TEST(Tracks_NE) {
    const TrackType t = NE;
    {
        const auto res = t.ride(UP);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(RIGHT);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(DOWN);
        EXPECT_EQ(RIGHT, res);
    }
    {
        const auto res = t.ride(LEFT);
        EXPECT_EQ(UP, res);
    }
}

TEST(Tracks_SE) {
    const TrackType t = SE;
    {
        const auto res = t.ride(UP);
        EXPECT_EQ(RIGHT, res);
    }
    {
        const auto res = t.ride(RIGHT);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(DOWN);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(LEFT);
        EXPECT_EQ(DOWN, res);
    }
}

TEST(Tracks_SW) {
    const TrackType t = SW;
    {
        const auto res = t.ride(UP);
        EXPECT_EQ(LEFT, res);
    }
    {
        const auto res = t.ride(RIGHT);
        EXPECT_EQ(DOWN, res);
    }
    {
        const auto res = t.ride(DOWN);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(LEFT);
        EXPECT_EQ(NONE, res);
    }
}

TEST(Tracks_H) {
    const TrackType t = H;
    {
        const auto res = t.ride(UP);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(RIGHT);
        EXPECT_EQ(RIGHT, res);
    }
    {
        const auto res = t.ride(DOWN);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(LEFT);
        EXPECT_EQ(LEFT, res);
    }
}

TEST(Tracks_V) {
    const TrackType t = V;
    {
        const auto res = t.ride(UP);
        EXPECT_EQ(UP, res);
    }
    {
        const auto res = t.ride(RIGHT);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = t.ride(DOWN);
        EXPECT_EQ(DOWN, res);
    }
    {
        const auto res = t.ride(LEFT);
        EXPECT_EQ(NONE, res);
    }
}

TEST(DirToChar) {
    EXPECT_EQ('u', char(Direction(UP)));
    EXPECT_EQ('r', char(Direction(RIGHT)));
    EXPECT_EQ('d', char(Direction(DOWN)));
    EXPECT_EQ('l', char(Direction(LEFT)));
}

TEST(Position) {
    Position p(1, 1);
    {
        Position actual = p + UP;
        Position expected(1, 0);
        EXPECT_EQ(expected, actual);
    }

    {
        Position actual = p + RIGHT;
        Position expected(2, 1);
        EXPECT_EQ(expected, actual);
    }

    {
        Position actual = p + DOWN;
        Position expected(1, 2);
        EXPECT_EQ(expected, actual);
    }

    {
        Position actual = p + LEFT;
        Position expected(0, 1);
        EXPECT_EQ(expected, actual);
    }

    {
        Position actual = p + NONE;
        Position expected(1, 1);
        EXPECT_EQ(expected, actual);
    }
}

static std::regex LEVEL_FILE_REGEX{"level\\d+.txt"};
TEST(LevelParsing) {
    const std::filesystem::path levelsPath = std::filesystem::canonical(
        std::filesystem::path(__FILE__).parent_path() / ".." / ".." / "levels"
    );
    for (const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(levelsPath)) {
        if (!dirEntry.is_regular_file()) {
            continue;
        }

        const std::filesystem::path& levelPath = dirEntry.path();
        const std::string levelFilename = levelPath.filename().string();
        if (!std::regex_match(levelFilename, LEVEL_FILE_REGEX)) {
            continue;
        }

        const std::string fileContents = readFile(levelPath);
        stateFromString(fileContents);
    }
}

TEST(FindEndStates_Level1) {
    constexpr std::string_view level =
        "######\n"
        "1 HHL#\n"
        "# v  #\n"
        "#h# V#\n"
        "#@#1V#\n"
        "####V#\n";

    const auto [grid, state] = stateFromString(level);
    const auto startList = createStartList(grid);

    const auto endList = findEndStates(grid, startList, state);
    ASSERT_EQ(1u, endList.size());
    ASSERT_TRUE(endList.has(0));
    const auto& endStates = endList.at(0);
    ASSERT_EQ(1u, endStates.size());
    EXPECT_EQ((Position{3,3}), endStates[0].player);
    ASSERT_EQ(2u, grid.objectCount);
    EXPECT_EQ(MOVABLE_V,       endStates[0].objects[0]);
    EXPECT_EQ((Position{4,2}), endStates[0].objectPositions[0]);
    EXPECT_EQ(MOVABLE_H,       endStates[0].objects[1]);
    EXPECT_EQ((Position{1,1}), endStates[0].objectPositions[1]);
}

TEST(FindEndStates_Level2) {
    constexpr std::string_view level =
        "#######\n"
        "#D HHL#\n"
        "#V  1V#\n"
        "#  v V#\n"
        "#Vhv  #\n"
        "HU@  V#\n"
        "#####1#\n";


    const auto [grid, state] = stateFromString(level);
    const auto startList = createStartList(grid);

    const auto endList = findEndStates(grid, startList, state);
    ASSERT_EQ(1u, endList.size());
    ASSERT_TRUE(endList.has(0));
    const auto& endStates = endList.at(0);
    ASSERT_EQ(4u, endStates.size());
    std::unordered_set<Position> expectedPositions{
        Position{3, 2},
        Position{4, 3},
    };
    for (const auto& state : endStates) {
        EXPECT_TRUE(expectedPositions.contains(state.player));
    }
    ASSERT_EQ(3u, grid.objectCount);
    EXPECT_EQ(MOVABLE_V,       endStates[0].objects[0]);
    EXPECT_EQ((Position{5,4}), endStates[0].objectPositions[0]);
    EXPECT_EQ(MOVABLE_H,       endStates[0].objects[1]);
    EXPECT_EQ((Position{2,1}), endStates[0].objectPositions[1]);
    EXPECT_EQ(MOVABLE_V,       endStates[0].objects[2]);
    EXPECT_EQ((Position{1,3}), endStates[0].objectPositions[2]);

    EXPECT_EQ(MOVABLE_V,       endStates[2].objects[0]);
    EXPECT_EQ((Position{1,3}), endStates[2].objectPositions[0]);
    EXPECT_EQ(MOVABLE_H,       endStates[2].objects[1]);
    EXPECT_EQ((Position{2,1}), endStates[2].objectPositions[1]);
    EXPECT_EQ(MOVABLE_V,       endStates[2].objects[2]);
    EXPECT_EQ((Position{5,4}), endStates[2].objectPositions[2]);
}

TEST(FindEndStates_Level18) {
    constexpr std::string_view level =
        "###1###\n"
        "HH   ##\n"
        "#1   ##\n"
        "###*###\n"
        "#     #\n"
        "# uhv #\n"
        "## @ ##\n"
        "#######\n";

    const auto [grid, state] = stateFromString(level);
    const auto startList = createStartList(grid);

    const auto endList = findEndStates(grid, startList, state);
    ASSERT_EQ(1u, endList.size());
    ASSERT_TRUE(endList.has(0));
    const auto& endStates = endList.at(0);
    ASSERT_EQ(1u, endStates.size());
    EXPECT_EQ((Position{2,2}), endStates[0].player);
    ASSERT_EQ(3u, grid.objectCount);
    EXPECT_EQ(MOVABLE_NW,      endStates[0].objects[0]);
    EXPECT_EQ((Position{3,1}), endStates[0].objectPositions[0]);
    EXPECT_EQ(MOVABLE_H,       endStates[0].objects[1]);
    EXPECT_EQ((Position{2,1}), endStates[0].objectPositions[1]);
    EXPECT_EQ(FLOOR,           endStates[0].objects[2]);
    EXPECT_EQ((Position{3,3}), endStates[0].objectPositions[2]);
}

TEST(SimulateTrain) {
    constexpr std::string_view level =
        "########\n"
        "1HHHHL@#\n"
        "#DHHLV1#\n"
        "#VDLVVDH\n"
        "#VVRUVV#\n"
        "#VRHHUV#\n"
        "#RHHHHU#\n"
        "########\n";

    const auto [grid, state] = stateFromString(level);

    const auto startList = createStartList(grid);
    const auto result = simulateTrain(grid, startList, state, 0);
    EXPECT_TRUE(result);
}

TEST(SimulateTrain_DoesNotExitWhenEdgeTrackTurnsBackIntoGrid) {
    constexpr std::string_view level =
        "#DD##\n"
        "1UH@#\n"
        "##1 #\n"
        "#####\n";

    const auto [grid, state] = stateFromString(level);

    const auto startList = createStartList(grid);
    const auto result = simulateTrain(grid, startList, state, 0);
    EXPECT_FALSE(result);
}

TEST(StateEncoding) {
    State state1{};
    state1.objects[0] = MOVABLE_H;
    state1.objectPositions[0] = {2, 1};
    state1.player = {1, 1};
    State state2{};
    state2.objects[0] = MOVABLE_H;
    state2.objectPositions[0] = {1, 1};
    state2.player = {2, 1};

    StateEncoding encoding1 = state1.encode(1);
    StateEncoding encoding2 = state2.encode(1);
    EXPECT_NE(encoding1, encoding2);
}

TEST(FixedIndexedVectorTest_StartsEmptyWithFixedCapacity) {
    FixedIndexedVector<int, uint32_t, 4> vec;

    EXPECT_TRUE(vec.empty());
    EXPECT_FALSE(vec.full());
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.capacity(), 4u);
}

TEST(FixedIndexedVectorTest_PushBackStoresValuesAndIndices) {
    FixedIndexedVector<std::string, uint16_t, 3> vec;

    vec.push_back("alpha", 10);
    vec.push_back("beta", 20);

    EXPECT_FALSE(vec.empty());
    EXPECT_FALSE(vec.full());
    EXPECT_EQ(vec.size(), 2u);

    EXPECT_EQ(vec.value(0), "alpha");
    EXPECT_EQ(vec.index(0), 10u);

    EXPECT_EQ(vec.value(1), "beta");
    EXPECT_EQ(vec.index(1), 20u);
}

TEST(FixedIndexedVectorTest_ReportsFullAtCapacity) {
    FixedIndexedVector<int, uint8_t, 2> vec;

    vec.push_back(11, 1);
    EXPECT_FALSE(vec.full());

    vec.push_back(22, 2);
    EXPECT_TRUE(vec.full());
    EXPECT_EQ(vec.size(), 2u);
}

TEST(FixedIndexedVectorTest_PopBackRemovesDefaultOneElement) {
    FixedIndexedVector<int, uint8_t, 3> vec;

    vec.push_back(10, 1);
    vec.push_back(20, 2);
    vec.push_back(30, 3);

    vec.pop_back();

    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec.value(0), 10);
    EXPECT_EQ(vec.index(0), 1u);
    EXPECT_EQ(vec.value(1), 20);
    EXPECT_EQ(vec.index(1), 2u);
}

TEST(FixedIndexedVectorTest_PopBackCanRemoveMultipleElements) {
    FixedIndexedVector<int, uint8_t, 5> vec;

    vec.push_back(10, 1);
    vec.push_back(20, 2);
    vec.push_back(30, 3);
    vec.push_back(40, 4);

    vec.pop_back(3);

    EXPECT_EQ(vec.size(), 1u);
    EXPECT_EQ(vec.value(0), 10);
    EXPECT_EQ(vec.index(0), 1u);
}

TEST(FixedIndexedVectorTest_ClearMakesVectorEmptyButKeepsCapacity) {
    FixedIndexedVector<int, uint8_t, 3> vec;

    vec.push_back(10, 1);
    vec.push_back(20, 2);

    vec.clear();

    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.size(), 0u);
    EXPECT_EQ(vec.capacity(), 3u);

    vec.push_back(99, 9);
    EXPECT_EQ(vec.size(), 1u);
    EXPECT_EQ(vec.value(0), 99);
    EXPECT_EQ(vec.index(0), 9u);
}

TEST(FixedIndexedVectorTest_MoveAssignFromMovesValueAndIndex) {
    FixedIndexedVector<std::string, uint16_t, 3> vec;

    vec.push_back("zero", 100);
    vec.push_back("one", 200);
    vec.push_back("two", 300);

    vec.move_assign_from(0, 2);

    EXPECT_EQ(vec.value(0), "two");
    EXPECT_EQ(vec.index(0), 300u);
}

TEST(FixedIndexedVectorTest_SupportsMoveOnlyValues) {
    FixedIndexedVector<std::unique_ptr<int>, uint8_t, 2> vec;

    vec.push_back(std::make_unique<int>(42), 7);

    ASSERT_NE(vec.value(0), nullptr);
    EXPECT_EQ(*vec.value(0), 42);
    EXPECT_EQ(vec.index(0), 7u);
}

TEST(FixedIndexedQueueTest_StartsEmptyWithFixedCapacity) {
    FixedIndexedQueue<int, uint32_t, 4> queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0u);
    EXPECT_EQ(queue.capacity(), 4u);
}

TEST(FixedIndexedQueueTest_PushStoresValueAndIndex) {
    FixedIndexedQueue<int, uint16_t, 3> queue;

    queue.push(123, 9);

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue.peek(), 123);
    EXPECT_EQ(queue.index(0), 9u);
}

TEST(FixedIndexedQueueTest_PopReturnsValueAndIndex) {
    FixedIndexedQueue<std::string, uint16_t, 3> queue;

    queue.push("alpha", 10);

    auto [value, index] = queue.pop();

    EXPECT_EQ(value, "alpha");
    EXPECT_EQ(index, 10u);
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0u);
}

TEST(FixedIndexedQueueTest_PopAdvancesToNextElement) {
    FixedIndexedQueue<int, uint16_t, 3> queue;

    queue.push(10, 1);
    queue.push(20, 2);

    auto [firstValue, firstIndex] = queue.pop();

    EXPECT_EQ(firstValue, 10);
    EXPECT_EQ(firstIndex, 1u);
    EXPECT_EQ(queue.size(), 1u);

    EXPECT_EQ(queue.peek(), 20);
}

TEST(FixedIndexedQueueTest_PopsInFifoOrder) {
    FixedIndexedQueue<int, uint16_t, 4> queue;

    queue.push(10, 1);
    queue.push(20, 2);
    queue.push(30, 3);

    {
        auto [value, index] = queue.pop();
        EXPECT_EQ(value, 10);
        EXPECT_EQ(index, 1u);
    }

    {
        auto [value, index] = queue.pop();
        EXPECT_EQ(value, 20);
        EXPECT_EQ(index, 2u);
    }

    {
        auto [value, index] = queue.pop();
        EXPECT_EQ(value, 30);
        EXPECT_EQ(index, 3u);
    }

    EXPECT_TRUE(queue.empty());
}

TEST(FixedIndexedQueueTest_WrapsAroundAfterPopAndPush) {
    FixedIndexedQueue<int, uint16_t, 3> queue;

    queue.push(10, 1);
    queue.push(20, 2);
    queue.push(30, 3);

    auto [firstValue, firstIndex] = queue.pop();
    EXPECT_EQ(firstValue, 10);
    EXPECT_EQ(firstIndex, 1u);

    queue.push(40, 4);

    {
        auto [value, index] = queue.pop();
        EXPECT_EQ(value, 20);
        EXPECT_EQ(index, 2u);
    }

    {
        auto [value, index] = queue.pop();
        EXPECT_EQ(value, 30);
        EXPECT_EQ(index, 3u);
    }

    {
        auto [value, index] = queue.pop();
        EXPECT_EQ(value, 40);
        EXPECT_EQ(index, 4u);
    }

    EXPECT_TRUE(queue.empty());
}

TEST(FixedIndexedQueueTest_ClearResetsQueue) {
    FixedIndexedQueue<int, uint16_t, 3> queue;

    queue.push(10, 1);
    queue.push(20, 2);

    queue.clear();

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0u);

    queue.push(99, 9);

    EXPECT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue.peek(), 99);

    auto [value, index] = queue.pop();
    EXPECT_EQ(value, 99);
    EXPECT_EQ(index, 9u);
}

TEST(FixedIndexedQueueTest_SupportsMoveOnlyValues) {
    FixedIndexedQueue<std::unique_ptr<int>, uint8_t, 2> queue;

    queue.push(std::make_unique<int>(42), 7);

    auto [value, index] = queue.pop();

    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 42);
    EXPECT_EQ(index, 7u);
}

TEST(SolutionSearch_OneMoveLongTrack) {
    constexpr std::string_view level =
        "########\n"
        "1HHHHL@#\n"
        "#DHHLV1#\n"
        "#VDLVVDH\n"
        "#VVRUVV#\n"
        "#VRHHUV#\n"
        "#RHHHHU#\n"
        "########\n";

    const auto [grid, state] = stateFromString(level);

    const auto startList = createStartList(grid);
    const auto solution = search(grid, startList, state);
    ASSERT_EQ(1, solution.size());
    EXPECT_EQ(DOWN, solution[0]);
}

TEST(SolutionSearch_TwoMoves) {
    constexpr std::string_view level =
        "####\n"
        "1 HH\n"
        "#h1#\n"
        "#@##\n"
        "####\n";

    const auto [grid, state] = stateFromString(level);

    const auto startList = createStartList(grid);
    const auto solution = search(grid, startList, state);
    ASSERT_EQ(2, solution.size());
    EXPECT_EQ(UP, solution[0]);
    EXPECT_EQ(RIGHT, solution[1]);
}

TEST(SolutionSearch_FillHole) {
    constexpr std::string_view level =
        "####\n"
        "#*1#\n"
        "#hDH\n"
        "#@V#\n"
        "##1#\n";

    auto [grid, state] = stateFromString(level);

    const auto startList = createStartList(grid);
    const auto solution = search(grid, startList, state);
    ASSERT_EQ(3, solution.size());
    EXPECT_EQ(UP, solution[0]);
    EXPECT_EQ(UP, solution[1]);
    EXPECT_EQ(RIGHT, solution[2]);
}

TEST(SolutionSearch_PushBlockOverHole) {
    constexpr std::string_view level =
        "#####\n"
        "1 HHH\n"
        "#  1#\n"
        "#*###\n"
        "#hh #\n"
        "#  @#\n"
        "#####\n";

    auto [grid, state] = stateFromString(level);

    const auto startList = createStartList(grid);
    const auto solution = search(grid, startList, state);
    ASSERT_EQ(15, solution.size());
    EXPECT_EQ(LEFT, solution[0]);
    EXPECT_EQ(LEFT, solution[1]);
    EXPECT_EQ(UP, solution[2]);
    EXPECT_EQ(DOWN, solution[3]);
    EXPECT_EQ(RIGHT, solution[4]);
    EXPECT_EQ(RIGHT, solution[5]);
    EXPECT_EQ(UP, solution[6]);
    EXPECT_EQ(LEFT, solution[7]);
    EXPECT_EQ(DOWN, solution[8]);
    EXPECT_EQ(LEFT, solution[9]);
    EXPECT_EQ(UP, solution[10]);
    EXPECT_EQ(UP, solution[11]);
    EXPECT_EQ(UP, solution[12]);
    EXPECT_EQ(RIGHT, solution[13]);
    EXPECT_EQ(RIGHT, solution[14]);
}
