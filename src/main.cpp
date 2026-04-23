#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <string>

#include "GameLogic.hpp"

static std::atomic_bool done = false;

void sigHandler(int signal) {
    done = true;
}

/*
    Fun fact, this fails on level 6.
*/

int main() {
    std::signal(SIGINT, sigHandler);

    while (!done) {
        try {
            std::cout << "Choose a level: ";
            std::string levelStr;
            std::cin >> levelStr;
            if (levelStr.empty()) {
                std::cout << std::endl;
                break;
            }
            size_t level = std::stoll(levelStr);
            const std::filesystem::path repoPath = std::filesystem::canonical(
                std::filesystem::path(__FILE__) / ".." / ".."
            );
            const std::filesystem::path levelPath = repoPath / "levels" / std::format("level{}.txt", level);
            const std::string fileContents = readFile(levelPath);
            const auto initialState = stateFromString(fileContents);
            StartList startList = createStartList(initialState);

            const auto solution = search(startList, initialState);
            if (solution.empty()) {
                std::cout << "No solution found" << std::endl;
            }
            else {
                std::string sequence;
                sequence.reserve(solution.size());
                for (auto move : solution) {
                    sequence += toChar(move);
                }

                std::cout << std::format("Solution found: {}", sequence) << std::endl;

                // Save the solution to the file.
                if (fileContents.ends_with('\n')) {
                    if (!std::filesystem::exists(levelPath)) {
                        throw std::runtime_error("File no longer exists. Cannot save solution");
                    }

                    std::fstream file(levelPath, std::ios::out | std::ios::app);
                    file << sequence;
                }
            }
        }
        catch (const std::invalid_argument& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    return 0;
}
