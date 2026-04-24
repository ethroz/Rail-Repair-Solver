#include <atomic>
#include <csignal>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>

#include "GameLogic.hpp"

static std::atomic_bool done = false;

static const std::filesystem::path repoPath = std::filesystem::canonical(
    std::filesystem::path(__FILE__) / ".." / ".."
);

void sigHandler(int signal) {
    done = true;
}

void writeSolutionFile(
    size_t level,
    std::chrono::steady_clock::duration runtime,
    const std::string& solution
) {
    const std::filesystem::path solutionsDir = repoPath / "solutions";
    std::filesystem::create_directories(solutionsDir);

    const auto runtimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(runtime).count();
    std::ofstream file(solutionsDir / std::format("level{}.txt", level), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to create solution output file");
    }

    file << std::format("Runtime: {} ms\n", runtimeMs);
    file << std::format("Solution: {}\n", solution.empty() ? "No solution found" : solution);
}

bool solveLevel(const std::string& levelStr) {
    size_t level = std::stoll(levelStr);
    const std::filesystem::path levelPath = repoPath / "levels" / std::format("level{}.txt", level);
    const std::string fileContents = readFile(levelPath);
    const auto initialState = stateFromString(fileContents);
    StartList startList = createStartList(initialState);

    const auto startTime = std::chrono::steady_clock::now();
    const auto solution = search(startList, initialState, done);
    const auto runtime = std::chrono::steady_clock::now() - startTime;
    if (solution.empty()) {
        writeSolutionFile(level, runtime, "");
        std::cout << "No solution found" << std::endl;
        return false;
    }
    else {
        std::string sequence;
        sequence.reserve(solution.size());
        for (auto move : solution) {
            sequence += toChar(move);
        }

        writeSolutionFile(level, runtime, sequence);
        std::cout << std::format("Solution found: {}", sequence) << std::endl;
    }

    return true;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, sigHandler);

    if (argc == 2) {
        try {
            return solveLevel(argv[1]) ? 0 : 1;
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return 1;
        }
    }
    else if (argc == 1) {
        while (!done) {
            try {
                std::cout << "Choose a level: ";
                std::string levelStr;
                std::cin >> levelStr;
                if (levelStr.empty()) {
                    std::cout << std::endl;
                    break;
                }
                solveLevel(levelStr);
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
            }
        }
        return 0;
    }
    else {
        std::cerr << "Invalid number of arguments. Only 0 or 1 are allowed." << std::endl;
        return 1;
    }

}
