#include <filesystem>
#include <regex>
#include <string>
#include <string_view>

#include "Test.hpp"

#include "Components.hpp"
#include "CoordSystem.hpp"
#include "GameLogic.hpp"

TEST(Tracks_NW) {
    const Cell c = MOVABLE_NW;
    {
        const auto res = trackToDirection(c, UP);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, RIGHT);
        EXPECT_EQ(UP, res);
    }
    {
        const auto res = trackToDirection(c, DOWN);
        EXPECT_EQ(LEFT, res);
    }
    {
        const auto res = trackToDirection(c, LEFT);
        EXPECT_EQ(NONE, res);
    }
}

TEST(Tracks_NE) {
    const Cell c = MOVABLE_NE;
    {
        const auto res = trackToDirection(c, UP);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, RIGHT);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, DOWN);
        EXPECT_EQ(RIGHT, res);
    }
    {
        const auto res = trackToDirection(c, LEFT);
        EXPECT_EQ(UP, res);
    }
}

TEST(Tracks_SE) {
    const Cell c = MOVABLE_SE;
    {
        const auto res = trackToDirection(c, UP);
        EXPECT_EQ(RIGHT, res);
    }
    {
        const auto res = trackToDirection(c, RIGHT);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, DOWN);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, LEFT);
        EXPECT_EQ(DOWN, res);
    }
}

TEST(Tracks_SW) {
    const Cell c = MOVABLE_SW;
    {
        const auto res = trackToDirection(c, UP);
        EXPECT_EQ(LEFT, res);
    }
    {
        const auto res = trackToDirection(c, RIGHT);
        EXPECT_EQ(DOWN, res);
    }
    {
        const auto res = trackToDirection(c, DOWN);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, LEFT);
        EXPECT_EQ(NONE, res);
    }
}

TEST(Tracks_H) {
    const Cell c = MOVABLE_H;
    {
        const auto res = trackToDirection(c, UP);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, RIGHT);
        EXPECT_EQ(RIGHT, res);
    }
    {
        const auto res = trackToDirection(c, DOWN);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, LEFT);
        EXPECT_EQ(LEFT, res);
    }
}

TEST(Tracks_V) {
    const Cell c = MOVABLE_V;
    {
        const auto res = trackToDirection(c, UP);
        EXPECT_EQ(UP, res);
    }
    {
        const auto res = trackToDirection(c, RIGHT);
        EXPECT_EQ(NONE, res);
    }
    {
        const auto res = trackToDirection(c, DOWN);
        EXPECT_EQ(DOWN, res);
    }
    {
        const auto res = trackToDirection(c, LEFT);
        EXPECT_EQ(NONE, res);
    }
}

TEST(DirToChar) {
    EXPECT_EQ('u', toChar(UP));
    EXPECT_EQ('r', toChar(RIGHT));
    EXPECT_EQ('d', toChar(DOWN));
    EXPECT_EQ('l', toChar(LEFT));
}

TEST(Position) {
    {
        const Position actual(UP);
        const Position expected(0, -1);
        EXPECT_EQ(expected, actual);
    }

    {
        const Position actual(RIGHT);
        const Position expected(1, 0);
        EXPECT_EQ(expected, actual);
    }

    {
        const Position actual(DOWN);
        const Position expected(0, 1);
        EXPECT_EQ(expected, actual);
    }

    {
        const Position actual(LEFT);
        const Position expected(-1, 0);
        EXPECT_EQ(expected, actual);
    }

    {
        const Position actual(NONE);
        const Position expected(0, 0);
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

    const auto state = stateFromString(level);

    const auto startList = createStartList(state);
    const auto result = simulateTrain(startList, state.grid, 0);
    EXPECT_TRUE(result);
}

TEST(SolutionSearch) {
    constexpr std::string_view level =
        "########\n"
        "1HHHHL@#\n"
        "#DHHLV1#\n"
        "#VDLVVDH\n"
        "#VVRUVV#\n"
        "#VRHHUV#\n"
        "#RHHHHU#\n"
        "########\n";

    const auto state = stateFromString(level);

    const auto startList = createStartList(state);
    const auto solution = search(startList, state);
    ASSERT_EQ(1, solution.size());
    EXPECT_EQ(DOWN, solution[0]);
}
