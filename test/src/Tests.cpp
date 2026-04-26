#include <filesystem>
#include <functional>
#include <regex>
#include <string>
#include <string_view>

#include "Test.hpp"

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "GameLogic.hpp"
#include "PriorityQueue.hpp"

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
        std::filesystem::path(__FILE__) / ".." / ".." / ".." / "levels"
    );
    for (const std::filesystem::directory_entry dirEntry : std::filesystem::directory_iterator(levelsPath)) {
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

TEST(PriorityQueue_InsertExtractKeepsOrder) {
    PriorityQueue<int> queue;
    queue.insert(5);
    queue.insert(1);
    queue.insert(3);
    EXPECT_EQ(1, queue.minimum());
    EXPECT_EQ(1, queue.extract_min());
    EXPECT_EQ(3, queue.extract_min());
    EXPECT_EQ(5, queue.extract_min());
    EXPECT_TRUE(queue.empty());
}

TEST(PriorityQueue_ClearAndEmpty) {
    PriorityQueue<int> queue;
    queue.insert(42);
    queue.insert(7);
    EXPECT_FALSE(queue.empty());
    queue.clear();
    EXPECT_TRUE(queue.empty());
}

TEST(PriorityQueue_ThrowsOnEmpty) {
    PriorityQueue<int> queue;
    bool caughtMinimum = false;
    bool caughtExtract = false;

    EXPECT_THROW(queue.minimum(), std::runtime_error);
    EXPECT_THROW(queue.extract_min(), std::runtime_error);
}

TEST(PriorityQueue_CustomComparator) {
    PriorityQueue<int, std::greater<int>> queue;
    queue.insert(2);
    queue.insert(7);
    queue.insert(4);
    EXPECT_EQ(7, queue.minimum());
    EXPECT_EQ(7, queue.extract_min());
    EXPECT_EQ(4, queue.minimum());
    EXPECT_EQ(4, queue.extract_min());
    EXPECT_EQ(2, queue.minimum());
    EXPECT_EQ(2, queue.extract_min());
    EXPECT_TRUE(queue.empty());
}
