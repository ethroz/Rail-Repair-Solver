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
    StableQueue<Queue, int, int, uint32_t, false>;

using AutoPruneFifoStableQueue =
    StableQueue<Queue, int, int, uint32_t, true>;

using PriorityStableQueue =
    StableQueue<PriorityQueue, int, int, uint32_t, false>;

using AutoPrunePriorityStableQueue =
    StableQueue<PriorityQueue, int, int, uint32_t, true>;

TEST(StableQueue_Queue_DefaultConstructedQueueIsEmptyAndNotDead) {
    FifoStableQueue q;

    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(0u, q.size());
    EXPECT_EQ(0u, q.deadSize());

    EXPECT_THROW((void)q.peek(), std::runtime_error);
    EXPECT_THROW(q.removeFront(), std::runtime_error);
    EXPECT_THROW(q.addRefForFront(), std::runtime_error);

    std::vector<int> chain;
    for (const int item : q) {
        chain.push_back(item);
    }

    EXPECT_EQ(std::vector<int>{}, chain);
}

TEST(StableQueue_Queue_PushAndRemoveFrontExposeChainThroughIterator) {
    FifoStableQueue q;

    q.push(10);
    q.push(20);
    q.push(30);

    ASSERT_FALSE(q.empty());
    ASSERT_FALSE(q.isDead());
    ASSERT_EQ(3u, q.size());
    ASSERT_EQ(0u, q.deadSize());

    EXPECT_EQ(10, q.peek());

    {
        std::vector<int> chain;
        for (const int item : q) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{10}), chain);
    }

    q.removeFront();

    EXPECT_FALSE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(2u, q.size());
    EXPECT_EQ(1u, q.deadSize());
    EXPECT_EQ(20, q.peek());

    {
        std::vector<int> chain;
        for (const int item : q) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{20, 10}), chain);
    }

    q.removeFront();

    EXPECT_FALSE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(1u, q.size());
    EXPECT_EQ(2u, q.deadSize());
    EXPECT_EQ(30, q.peek());

    {
        std::vector<int> chain;
        for (const int item : q) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{30, 10}), chain);
    }

    q.removeFront();

    EXPECT_TRUE(q.empty());
    EXPECT_TRUE(q.isDead());
    EXPECT_EQ(0u, q.size());
    EXPECT_EQ(3u, q.deadSize());

    {
        std::vector<int> chain;
        for (const int item : q) {
            chain.push_back(item);
        }

        EXPECT_EQ(std::vector<int>{}, chain);
    }
}

TEST(StableQueue_Queue_CannotPushOntoDeadQueue) {
    FifoStableQueue q;

    q.push(10);
    q.removeFront();

    ASSERT_TRUE(q.empty());
    ASSERT_TRUE(q.isDead());
    ASSERT_EQ(0u, q.size());
    ASSERT_EQ(1u, q.deadSize());

    EXPECT_THROW(q.push(20), std::invalid_argument);
}

TEST(StableQueue_Queue_ResetClearsDeadQueueAndAllowsReuse) {
    FifoStableQueue q;

    q.push(10);
    q.push(20);
    q.removeFront();
    q.removeFront();

    ASSERT_TRUE(q.empty());
    ASSERT_TRUE(q.isDead());
    ASSERT_EQ(0u, q.size());
    ASSERT_EQ(2u, q.deadSize());

    q.reset();

    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(0u, q.size());
    EXPECT_EQ(0u, q.deadSize());

    q.push(30);
    q.push(40);

    EXPECT_FALSE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(2u, q.size());
    EXPECT_EQ(0u, q.deadSize());
    EXPECT_EQ(30, q.peek());

    std::vector<int> chain;
    for (const int item : q) {
        chain.push_back(item);
    }

    EXPECT_EQ((std::vector<int>{30}), chain);
}

TEST(StableQueue_Queue_AddRefForFrontAllowsDeadSubqueueToProduceReplacementItem) {
    FifoStableQueue destination;
    FifoStableQueue subqueue;

    destination.push(100);
    destination.push(200);

    subqueue.push(1);
    subqueue.addRefForFront();
    subqueue.removeFront();

    ASSERT_TRUE(subqueue.empty());
    ASSERT_TRUE(subqueue.isDead());
    ASSERT_EQ(0u, subqueue.size());
    ASSERT_EQ(1u, subqueue.deadSize());

    const std::array replacementItems { 300 };

    destination.removeFrontWithDeadSubqueue(subqueue, replacementItems);

    EXPECT_FALSE(destination.empty());
    EXPECT_FALSE(destination.isDead());
    EXPECT_EQ(2u, destination.size());
    EXPECT_EQ(1u, destination.deadSize());
    EXPECT_EQ(200, destination.peek());

    {
        std::vector<int> chain;
        for (const int item : destination) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{200, 100}), chain);
    }

    destination.removeFront();

    EXPECT_EQ(300, destination.peek());

    {
        std::vector<int> chain;
        for (const int item : destination) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{300, 100}), chain);
    }
}

TEST(StableQueue_Queue_DeadSubqueueWithMultipleDeadNodesMapsReplacementChains) {
    FifoStableQueue destination;
    FifoStableQueue subqueue;

    destination.push(100);
    destination.push(200);

    subqueue.push(10);
    subqueue.push(20);
    subqueue.addRefForFront();
    subqueue.removeFront();
    subqueue.addRefForFront();
    subqueue.removeFront();

    ASSERT_TRUE(subqueue.empty());
    ASSERT_TRUE(subqueue.isDead());
    ASSERT_EQ(0u, subqueue.size());
    ASSERT_EQ(2u, subqueue.deadSize());

    const std::array replacementItems {
        300,
        400,
    };

    destination.removeFrontWithDeadSubqueue(subqueue, replacementItems);

    EXPECT_FALSE(destination.empty());
    EXPECT_FALSE(destination.isDead());
    EXPECT_EQ(3u, destination.size());
    EXPECT_EQ(2u, destination.deadSize());
    EXPECT_EQ(200, destination.peek());

    {
        std::vector<int> chain;
        for (const int item : destination) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{200, 100}), chain);
    }

    destination.removeFront();

    EXPECT_EQ(300, destination.peek());

    {
        std::vector<int> chain;
        for (const int item : destination) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{300, 100}), chain);
    }

    destination.removeFront();

    EXPECT_EQ(400, destination.peek());

    {
        std::vector<int> chain;
        for (const int item : destination) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{400, 20, 100}), chain);
    }
}

TEST(StableQueue_Queue_RemoveFrontWithEmptySubqueueJustRemovesFront) {
    FifoStableQueue destination;
    FifoStableQueue subqueue;

    destination.push(100);
    destination.push(200);

    const std::array<int, 0> replacementItems {};

    destination.removeFrontWithDeadSubqueue(subqueue, replacementItems);

    EXPECT_FALSE(destination.empty());
    EXPECT_FALSE(destination.isDead());
    EXPECT_EQ(1u, destination.size());
    EXPECT_EQ(1u, destination.deadSize());
    EXPECT_EQ(200, destination.peek());

    std::vector<int> chain;
    for (const int item : destination) {
        chain.push_back(item);
    }

    EXPECT_EQ((std::vector<int>{200, 100}), chain);
}

TEST(StableQueue_Queue_RemoveFrontWithDeadSubqueueRejectsMismatchedReplacementCount) {
    FifoStableQueue destination;
    FifoStableQueue subqueue;

    destination.push(100);

    subqueue.push(1);
    subqueue.addRefForFront();
    subqueue.removeFront();

    ASSERT_TRUE(subqueue.isDead());

    const std::array<int, 0> tooFewItems {};
    const std::array tooManyItems {
        300,
        400,
    };

    EXPECT_THROW(
        destination.removeFrontWithDeadSubqueue(subqueue, tooFewItems),
        std::invalid_argument
    );

    EXPECT_THROW(
        destination.removeFrontWithDeadSubqueue(subqueue, tooManyItems),
        std::invalid_argument
    );

    EXPECT_EQ(100, destination.peek());
    EXPECT_EQ(1u, destination.size());
    EXPECT_EQ(0u, destination.deadSize());
}

TEST(StableQueue_Queue_RemoveFrontWithDeadSubqueueRejectsAliveSubqueue) {
    FifoStableQueue destination;
    FifoStableQueue subqueue;

    destination.push(100);

    subqueue.push(1);
    subqueue.addRefForFront();

    ASSERT_FALSE(subqueue.empty());
    ASSERT_FALSE(subqueue.isDead());
    ASSERT_EQ(1u, subqueue.size());
    ASSERT_EQ(0u, subqueue.deadSize());

    const std::array replacementItems { 300 };

    EXPECT_THROW(
        destination.removeFrontWithDeadSubqueue(subqueue, replacementItems),
        std::invalid_argument
    );

    EXPECT_EQ(100, destination.peek());
    EXPECT_EQ(1u, destination.size());
    EXPECT_EQ(0u, destination.deadSize());
}

TEST(StableQueue_Queue_RemoveFrontWithDeadSubqueueRejectsEmptyDestination) {
    FifoStableQueue destination;
    FifoStableQueue subqueue;

    subqueue.push(1);
    subqueue.addRefForFront();
    subqueue.removeFront();

    ASSERT_TRUE(destination.empty());
    ASSERT_TRUE(subqueue.isDead());

    const std::array replacementItems { 300 };

    EXPECT_THROW(
        destination.removeFrontWithDeadSubqueue(subqueue, replacementItems),
        std::runtime_error
    );
}

TEST(StableQueue_Queue_PruneDeadKeepsOnlyChainNeededByFront) {
    FifoStableQueue q;

    q.push(10);
    q.push(20);
    q.removeFront();
    q.removeFront();

    ASSERT_TRUE(q.isDead());
    ASSERT_EQ(0u, q.size());
    ASSERT_EQ(2u, q.deadSize());

    q.reset();

    q.push(30);
    q.push(40);
    q.removeFront();

    ASSERT_EQ(1u, q.size());
    ASSERT_EQ(1u, q.deadSize());
    ASSERT_EQ(40, q.peek());

    q.pruneDead();

    EXPECT_EQ(1u, q.size());
    EXPECT_EQ(1u, q.deadSize());
    EXPECT_EQ(40, q.peek());

    std::vector<int> chain;
    for (const int item : q) {
        chain.push_back(item);
    }

    EXPECT_EQ((std::vector<int>{40, 30}), chain);
}

TEST(StableQueue_Queue_AutoPrunePreservesFrontChainWhenGrowing) {
    AutoPruneFifoStableQueue q(1, 1, 1);

    q.push(10);
    q.push(20);
    q.removeFront();

    ASSERT_EQ(1u, q.size());
    ASSERT_EQ(1u, q.deadSize());
    ASSERT_EQ(20, q.peek());

    q.push(30);
    q.push(40);

    EXPECT_EQ(3u, q.size());
    EXPECT_EQ(1u, q.deadSize());
    EXPECT_EQ(20, q.peek());

    {
        std::vector<int> chain;
        for (const int item : q) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{20, 10}), chain);
    }

    q.removeFront();

    EXPECT_EQ(30, q.peek());

    {
        std::vector<int> chain;
        for (const int item : q) {
            chain.push_back(item);
        }

        EXPECT_EQ((std::vector<int>{30, 20, 10}), chain);
    }
}

TEST(StableQueue_PriorityQueue_KeepsFrontStableWhenNewItemsDoNotOvertakeFront) {
    PriorityStableQueue q;

    q.push(10);
    q.push(20);
    q.push(30);

    EXPECT_FALSE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(3u, q.size());
    EXPECT_EQ(0u, q.deadSize());
    EXPECT_EQ(10, q.peek());

    std::vector<int> chain;
    for (const int item : q) {
        chain.push_back(item);
    }

    EXPECT_EQ((std::vector<int>{10}), chain);
}

TEST(StableQueue_PriorityQueue_RejectsPushThatWouldMoveFrontNode) {
    PriorityStableQueue q;

    q.push(30);

    EXPECT_THROW(q.push(10), std::invalid_argument);

    EXPECT_FALSE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(2u, q.size());
    EXPECT_EQ(0u, q.deadSize());

    // The attempted push already inserted into the underlying priority queue
    // before the StableQueue swap callback rejected moving the front node.
    // Reset is the supported recovery path after this kind of failed mutation.
    q.reset();

    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(0u, q.size());
    EXPECT_EQ(0u, q.deadSize());
}

TEST(StableQueue_PriorityQueue_SingleElementPriorityQueueCanBecomeDead) {
    PriorityStableQueue q;

    q.push(10);

    ASSERT_EQ(10, q.peek());
    ASSERT_EQ(1u, q.size());
    ASSERT_EQ(0u, q.deadSize());

    q.removeFront();

    EXPECT_TRUE(q.empty());
    EXPECT_TRUE(q.isDead());
    EXPECT_EQ(0u, q.size());
    EXPECT_EQ(1u, q.deadSize());

    EXPECT_THROW(q.push(20), std::invalid_argument);
}

TEST(StableQueue_PriorityQueue_AutoPrunePriorityQueueRejectsFrontMovingPush) {
    AutoPrunePriorityStableQueue q(1, 1, 1);

    q.push(50);

    EXPECT_THROW(q.push(40), std::invalid_argument);

    q.reset();

    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.isDead());
    EXPECT_EQ(0u, q.size());
    EXPECT_EQ(0u, q.deadSize());
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
