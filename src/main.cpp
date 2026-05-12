#include <atomic>
#include <csignal>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#include "GameLogic.hpp"

static std::atomic_bool done = false;

static const std::filesystem::path repoPath = std::filesystem::canonical(
    std::filesystem::path(__FILE__).parent_path() / ".."
);

void sigHandler([[maybe_unused]] int signal) {
    done = true;
    std::cout << std::endl;
}

void printStats(std::ostream& out) {
    out << "Total iterations: " << stats.iterations << std::endl;
    out << "visited size: " << stats.visited << std::endl;
}

void writeSolutionFile(
    size_t level,
    std::chrono::steady_clock::duration runtime,
    const std::string& solution
) {
    const std::filesystem::path solutionsDir = repoPath / "solutions";
    std::filesystem::create_directories(solutionsDir);

    const auto runtimeSeconds = std::chrono::duration<double>(runtime).count();
    std::ofstream file(solutionsDir / std::format("level{}.txt", level), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to create solution output file");
    }

    file << std::format("Runtime: {:.9f} s", runtimeSeconds) << std::endl;
    printStats(file);
    file << std::format("Solution: {}", solution.empty() ? "No solution found" : solution) << std::endl;
}

void printStateStats(const Grid& grid, const State& state) {
    struct Stats {
        size_t count = 0;
        std::string text;
    };
    Stats used{};
    Stats unused{};
    for (uint8_t i = 0; i < grid.objectCount; ++i) {
        Stats& stats = (state.objectPositions[i] == INVALID_POS) ? unused : used;
        if (stats.count > 0) {
            stats.text += ", ";
        }
        stats.text += char(state.objects[i]);
        ++stats.count;
    }
    std::cout << used.count << " used blocks: [" << used.text << ']' << std::endl;
    if (unused.count > 0) {
        std::cout << unused.count << " unused blocks: [" << unused.text << ']' << std::endl;
    }
}

bool solveLevel(const std::string& levelStr, bool saveResult, bool findGoals) {
    size_t level = std::stoll(levelStr);
    const std::filesystem::path levelPath = repoPath / "levels" / std::format("level{}.txt", level);
    const std::string fileContents = readFile(levelPath);
    const auto [grid, state] = stateFromString(fileContents);
    StartList startList = createStartList(grid);

    if (findGoals) {
        const auto startTime = std::chrono::steady_clock::now();
        const auto endList = findEndStates(grid, startList, state);
        const auto runtime = std::chrono::steady_clock::now() - startTime;
        std::cout << "Ran in " << runtime << std::endl;
        std::cout << "\nStart state:" << std::endl;
        std::cout << grid.toString(state) << std::endl;
        for (const auto [index, _] : startList) {
            const auto& endStates = endList.at(index);
            std::cout << "Found " << endStates.size() << " end states for lever " << int(index) << std::endl;
            for (const auto& endState : endStates) {
                std::cout << grid.toString(endState);
                printStateStats(grid, endState);
                std::cout << std::endl;
            }
        }
        if (endList.size() != startList.size()) {
            std::cout << "Could not find an end state for every lever" << std::endl;
            return false;
        }
        return true;
    }
    else {
        const auto startTime = std::chrono::steady_clock::now();
        const auto solution = search(grid, startList, state, done);
        const auto runtime = std::chrono::steady_clock::now() - startTime;
        std::cout << std::endl;
        printStats(std::cout);
        if (done) {
            return false;
        }
        std::string solutionStr;
        if (solution.empty()) {
            std::cout << "No solution found" << std::endl;
        }
        else {
            solutionStr.reserve(solution.size());
            for (auto move : solution) {
                solutionStr += char(move);
            }
            std::cout << std::format("Solution found: {}", solutionStr) << std::endl;
        }
    
        if (saveResult) {
            writeSolutionFile(level, runtime, solutionStr);
        }
    
        return !solution.empty();
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sigHandler);

    std::string levelStr;
    bool saveResult = true;
    bool findGoals = false;
    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);
        if (arg == "--no-save") {
            saveResult = false;
        }
        else if (arg == "--find-goals") {
            findGoals = true;
        }
        else if (std::isdigit(arg[0])) {
            levelStr = arg;
        }
        else {
            std::cerr << "Unsupported command line argument: " << arg << std::endl;
            return 1;
        }
    }

    if (levelStr.empty()) {
        while (!done) {
            std::cout << "Choose a level: ";

            std::string line;
            if (!std::getline(std::cin, line)) {
                std::cout << std::endl;
                break;
            }

            std::istringstream iss(line);
            if (!(iss >> levelStr)) {
                std::cout << std::endl;
                break;
            }

            try {
                solveLevel(levelStr, saveResult, findGoals);
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
            }

            levelStr.clear();
        }
        return 0;
    }
    else {
        try {
            return solveLevel(levelStr, saveResult, findGoals) ? 0 : 1;
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    }
}
