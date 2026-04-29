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
#include "StableQueue.hpp"


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

TEST(PriorityQueue_PushPopKeepsOrder) {
    PriorityQueue<int> queue;
    queue.push(5);
    queue.push(1);
    queue.push(3);
    EXPECT_EQ(1, queue.peek());
    EXPECT_EQ(1, queue.pop());
    EXPECT_EQ(3, queue.pop());
    EXPECT_EQ(5, queue.pop());
    EXPECT_TRUE(queue.empty());
}

TEST(PriorityQueue_ClearAndEmpty) {
    PriorityQueue<int> queue;
    queue.push(42);
    queue.push(7);
    EXPECT_FALSE(queue.empty());
    queue.clear();
    EXPECT_TRUE(queue.empty());
}

TEST(PriorityQueue_ThrowsOnEmpty) {
    PriorityQueue<int> queue;
    bool caughtMinimum = false;
    bool caughtExtract = false;

    EXPECT_THROW(queue.peek(), std::runtime_error);
    EXPECT_THROW(queue.pop(), std::runtime_error);
}

TEST(PriorityQueue_CustomComparator) {
    PriorityQueue<int, std::greater<int>> queue;
    queue.push(2);
    queue.push(7);
    queue.push(4);
    EXPECT_EQ(7, queue.peek());
    EXPECT_EQ(7, queue.pop());
    EXPECT_EQ(4, queue.peek());
    EXPECT_EQ(4, queue.pop());
    EXPECT_EQ(2, queue.peek());
    EXPECT_EQ(2, queue.pop());
    EXPECT_TRUE(queue.empty());
}

using FifoStableQueue =
    StableQueue<Queue, int, int, std::uint32_t, false>;

using AutoPruneFifoStableQueue =
    StableQueue<Queue, int, int, std::uint32_t, true>;

using PriorityStableQueue =
    StableQueue<PriorityQueue, int, int, std::uint32_t, false>;

TEST(StableQueue_Queue_DefaultConstructedQueueIsEmpty) {
    FifoStableQueue q;

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(0u, q.totalSize());
    EXPECT_EQ(0u, q.aliveSize());
    EXPECT_EQ(0u, q.deadSize());
    EXPECT_EQ(FifoStableQueue::NO_INDEX, q.peekIndex());

    EXPECT_THROW((void)q.peek(), std::runtime_error);
    EXPECT_THROW(q.removeFront(), std::runtime_error);
    EXPECT_THROW((void)q.at(0), std::invalid_argument);
    EXPECT_THROW((void)q.getPrevIndex(0), std::invalid_argument);
}

TEST(StableQueue_Queue_PushCreatesStableIndicesAndPrevLinks) {
    FifoStableQueue q;

    q.push(10);

    ASSERT_EQ(1u, q.totalSize());
    ASSERT_EQ(1u, q.aliveSize());
    ASSERT_EQ(0u, q.deadSize());

    EXPECT_EQ(0u, q.peekIndex());
    EXPECT_EQ(10, q.peek());

    EXPECT_EQ(10, q.at(0u));
    EXPECT_EQ(FifoStableQueue::NO_INDEX, q.getPrevIndex(0u));

    q.push(20);
    q.push(30);

    ASSERT_EQ(3u, q.totalSize());
    ASSERT_EQ(3u, q.aliveSize());
    ASSERT_EQ(0u, q.deadSize());

    EXPECT_EQ(10, q.at(0u));
    EXPECT_EQ(FifoStableQueue::NO_INDEX, q.getPrevIndex(0u));

    EXPECT_EQ(20, q.at(1u));
    EXPECT_EQ(0u, q.getPrevIndex(1u));

    EXPECT_EQ(30, q.at(2u));
    EXPECT_EQ(0u, q.getPrevIndex(2u));

    EXPECT_EQ(0u, q.peekIndex());
    EXPECT_EQ(10, q.peek());
}

TEST(StableQueue_Queue_RemoveFrontMovesAliveItemToDeadHistory) {
    FifoStableQueue q;

    q.push(10);
    q.push(20);
    q.push(30);

    q.removeFront();

    EXPECT_FALSE(q.empty());
    EXPECT_EQ(3u, q.totalSize());
    EXPECT_EQ(1u, q.deadSize());
    EXPECT_EQ(2u, q.aliveSize());

    EXPECT_EQ(10, q.at(0u));
    EXPECT_EQ(FifoStableQueue::NO_INDEX, q.getPrevIndex(0u));

    EXPECT_EQ(20, q.at(1u));
    EXPECT_EQ(0u, q.getPrevIndex(1u));

    EXPECT_EQ(30, q.at(2u));
    EXPECT_EQ(0u, q.getPrevIndex(2u));

    EXPECT_EQ(1u, q.peekIndex());
    EXPECT_EQ(20, q.peek());

    q.removeFront();

    EXPECT_EQ(3u, q.totalSize());
    EXPECT_EQ(2u, q.deadSize());
    EXPECT_EQ(1u, q.aliveSize());

    EXPECT_EQ(10, q.at(0u));
    EXPECT_EQ(FifoStableQueue::NO_INDEX, q.getPrevIndex(0u));

    EXPECT_EQ(20, q.at(1u));
    EXPECT_EQ(0u, q.getPrevIndex(1u));

    EXPECT_EQ(30, q.at(2u));
    EXPECT_EQ(0u, q.getPrevIndex(2u));

    EXPECT_EQ(2u, q.peekIndex());
    EXPECT_EQ(30, q.peek());
}

TEST(StableQueue_Queue_ClearRemovesAliveDeadAndHistory) {
    FifoStableQueue q;

    q.push(1);
    q.push(2);
    q.removeFront();

    ASSERT_EQ(2u, q.totalSize());
    ASSERT_EQ(1u, q.deadSize());
    ASSERT_EQ(1u, q.aliveSize());

    q.clear();

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(0u, q.totalSize());
    EXPECT_EQ(0u, q.deadSize());
    EXPECT_EQ(0u, q.aliveSize());
    EXPECT_EQ(FifoStableQueue::NO_INDEX, q.peekIndex());
}

TEST(StableQueue_Queue_PushIndexedRangePreservesGivenPrevIndices) {
    FifoStableQueue q;

    const std::array<std::pair<int, std::uint32_t>, 4> items {{
        {10, FifoStableQueue::NO_INDEX},
        {20, 0u},
        {30, 1u},
        {40, 1u},
    }};

    q.pushIndexedRange(items);

    ASSERT_EQ(4u, q.totalSize());
    ASSERT_EQ(0u, q.deadSize());
    ASSERT_EQ(4u, q.aliveSize());

    EXPECT_EQ(10, q.at(0u));
    EXPECT_EQ(FifoStableQueue::NO_INDEX, q.getPrevIndex(0u));

    EXPECT_EQ(20, q.at(1u));
    EXPECT_EQ(0u, q.getPrevIndex(1u));

    EXPECT_EQ(30, q.at(2u));
    EXPECT_EQ(1u, q.getPrevIndex(2u));

    EXPECT_EQ(40, q.at(3u));
    EXPECT_EQ(1u, q.getPrevIndex(3u));

    EXPECT_EQ(0u, q.peekIndex());
    EXPECT_EQ(10, q.peek());
}

TEST(StableQueue_Queue_PruneDeadNodesKeepsOnlyDeadAncestorsOfAliveNodes) {
    FifoStableQueue q;

    const auto N = FifoStableQueue::NO_INDEX;

    const std::array<std::pair<int, std::uint32_t>, 5> items {{
        {10, N},
        {20, 0u},
        {30, 1u},
        {40, N},
        {50, 3u},
    }};

    q.pushIndexedRange(items);

    q.removeFront();
    q.removeFront();
    q.removeFront();

    ASSERT_EQ(3u, q.deadSize());
    ASSERT_EQ(2u, q.aliveSize());
    ASSERT_EQ(5u, q.totalSize());

    q.pruneDeadNodes();

    EXPECT_EQ(0u, q.deadSize());
    EXPECT_EQ(2u, q.aliveSize());
    EXPECT_EQ(2u, q.totalSize());

    EXPECT_EQ(40, q.at(0u));
    EXPECT_EQ(FifoStableQueue::NO_INDEX, q.getPrevIndex(0u));

    EXPECT_EQ(50, q.at(1u));
    EXPECT_EQ(0u, q.getPrevIndex(1u));

    EXPECT_EQ(0u, q.peekIndex());
    EXPECT_EQ(40, q.peek());
}

TEST(StableQueue_Queue_AutoPruneBeforeGrowthCompactsHistoryWhenPossible) {
    AutoPruneFifoStableQueue q(2, 2);

    const auto N = AutoPruneFifoStableQueue::NO_INDEX;

    const std::array<std::pair<int, std::uint32_t>, 4> initial {{
        {10, N},
        {20, 0u},
        {30, N},
        {40, 2u},
    }};

    q.pushIndexedRange(initial);

    q.removeFront();
    q.removeFront();

    ASSERT_EQ(2u, q.deadSize());
    ASSERT_EQ(2u, q.aliveSize());

    q.push(50);
    q.push(60);
    q.push(70);

    EXPECT_EQ(0u, q.deadSize());
    EXPECT_EQ(5u, q.aliveSize());
    EXPECT_EQ(5u, q.totalSize());

    EXPECT_EQ(30, q.at(0u));
    EXPECT_EQ(AutoPruneFifoStableQueue::NO_INDEX, q.getPrevIndex(0u));

    EXPECT_EQ(40, q.at(1u));
    EXPECT_EQ(0u, q.getPrevIndex(1u));
}

TEST(StableQueue_Queue_PushDeadQueueAppendsDeadHistoryAndAliveItems) {
    FifoStableQueue destination;
    FifoStableQueue source;

    destination.push(100);
    destination.push(200);
    destination.removeFront();

    ASSERT_EQ(1u, destination.deadSize());
    ASSERT_EQ(1u, destination.aliveSize());

    source.push(1);
    source.push(2);
    source.push(3);
    source.removeFront();
    source.removeFront();

    ASSERT_EQ(2u, source.deadSize());
    ASSERT_EQ(1u, source.aliveSize());
    ASSERT_EQ(3, source.peek());

    const std::array<int, 1> replacementAliveItems { 300 };

    destination.pushDeadQueue(source, replacementAliveItems);

    EXPECT_EQ(2u, destination.deadSize());
    EXPECT_EQ(2u, destination.aliveSize());
    EXPECT_EQ(4u, destination.totalSize());

    EXPECT_EQ(100, destination.at(0u));
    EXPECT_EQ(FifoStableQueue::NO_INDEX, destination.getPrevIndex(0u));

    EXPECT_EQ(2, destination.at(1u));
    EXPECT_EQ(0u, destination.getPrevIndex(1u));

    EXPECT_EQ(200, destination.at(2u));
    EXPECT_EQ(0u, destination.getPrevIndex(2u));

    EXPECT_EQ(300, destination.at(3u));
    EXPECT_EQ(1u, destination.getPrevIndex(3u));
}

TEST(StableQueue_Queue_PushDeadQueueRejectsInvalidInputs) {
    FifoStableQueue destination;
    FifoStableQueue source;

    source.push(1);
    source.push(2);
    source.removeFront();

    const std::array<int, 1> oneItem { 10 };

    EXPECT_THROW(destination.pushDeadQueue(source, oneItem), std::runtime_error);

    destination.push(100);
    destination.removeFront();

    FifoStableQueue sourceWithNoDead;
    sourceWithNoDead.push(1);

    EXPECT_THROW(
        destination.pushDeadQueue(sourceWithNoDead, oneItem),
        std::runtime_error
    );

    const std::array<int, 2> wrongAliveItemCount { 10, 20 };

    EXPECT_THROW(
        destination.pushDeadQueue(source, wrongAliveItemCount),
        std::invalid_argument
    );
}

TEST(StableQueue_PriorityQueue_PeekAndRemoveFrontUsePriorityOrder) {
    PriorityStableQueue q;

    q.push(30);
    q.push(10);
    q.push(20);

    EXPECT_EQ(10, q.peek());
    EXPECT_EQ(0u, q.peekIndex());

    q.removeFront();

    EXPECT_EQ(1u, q.deadSize());
    EXPECT_EQ(2u, q.aliveSize());
    EXPECT_EQ(10, q.at(0u));

    EXPECT_EQ(20, q.peek());
    EXPECT_EQ(1u, q.peekIndex());

    q.removeFront();

    EXPECT_EQ(2u, q.deadSize());
    EXPECT_EQ(1u, q.aliveSize());
    EXPECT_EQ(20, q.at(1u));

    EXPECT_EQ(30, q.peek());
    EXPECT_EQ(2u, q.peekIndex());
}

TEST(StableQueue_PriorityQueue_StableIndicesFollowHeapSwaps) {
    PriorityStableQueue q;

    q.push(50);
    q.push(40);
    q.push(30);
    q.push(20);
    q.push(10);

    ASSERT_EQ(5u, q.totalSize());
    ASSERT_EQ(5u, q.aliveSize());
    ASSERT_EQ(0u, q.deadSize());

    EXPECT_EQ(10, q.peek());
    EXPECT_EQ(0u, q.peekIndex());

    q.removeFront();

    EXPECT_EQ(10, q.at(0u));
    EXPECT_EQ(20, q.peek());
    EXPECT_EQ(1u, q.peekIndex());

    q.removeFront();

    EXPECT_EQ(20, q.at(1u));
    EXPECT_EQ(30, q.peek());
    EXPECT_EQ(2u, q.peekIndex());
}

TEST(StableQueue_PriorityQueue_PushIndexedRangeWithPriorityQueueKeepsHistoryValid) {
    PriorityStableQueue q;

    const auto N = PriorityStableQueue::NO_INDEX;

    const std::array<std::pair<int, std::uint32_t>, 5> items {{
        {50, N},
        {40, 0u},
        {30, 1u},
        {20, 2u},
        {10, 3u},
    }};

    q.pushIndexedRange(items);

    ASSERT_EQ(5u, q.totalSize());
    ASSERT_EQ(5u, q.aliveSize());

    EXPECT_EQ(10, q.peek());

    q.removeFront();
    q.removeFront();

    EXPECT_EQ(2u, q.deadSize());
    EXPECT_EQ(3u, q.aliveSize());

    EXPECT_EQ(10, q.at(0u));
    EXPECT_EQ(20, q.at(1u));
    EXPECT_EQ(30, q.peek());

    q.pruneDeadNodes();

    EXPECT_LE(q.deadSize(), 2u);
    EXPECT_EQ(3u, q.aliveSize());
    EXPECT_EQ(30, q.peek());
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
