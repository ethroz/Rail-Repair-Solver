#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <string>
#include <stdexcept>

#include "GameLogic.hpp"

static std::atomic_bool done = false;

void sigHandler(int signal) {
    done = true;
}

bool solveLevel(const std::string& levelStr) {
    size_t level = std::stoll(levelStr);
    const std::filesystem::path repoPath = std::filesystem::canonical(
        std::filesystem::path(__FILE__) / ".." / ".."
    );
    const std::filesystem::path levelPath = repoPath / "levels" / std::format("level{}.txt", level);
    const std::string fileContents = readFile(levelPath);
    const auto initialState = stateFromString(fileContents);
    StartList startList = createStartList(initialState);

    const auto solution = search(startList, initialState, done);
    if (solution.empty()) {
        std::cout << "No solution found" << std::endl;
        return false;
    }
    else {
        std::string sequence;
        sequence.reserve(solution.size());
        for (auto move : solution) {
            sequence += toChar(move);
        }

        std::cout << std::format("Solution found: {}", sequence) << std::endl;

        // Save the solution to the file.
        if (std::filesystem::exists(levelPath)) {
            const size_t boardLength = fileContents.rfind('\n') + 1;
            const std::string newFileContents = fileContents.substr(0, boardLength) + sequence;

            std::ofstream file(levelPath, std::ios::out | std::ios::trunc | std::ios::binary);
            file << newFileContents;
        }
        else {
            throw std::runtime_error("File no longer exists. Cannot save solution");
        }
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
