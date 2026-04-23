cmake_minimum_required(VERSION 3.20)

project(RailRepairSolver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W3 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(rail_repair_solver
    src/main.cpp
)

target_include_directories(rail_repair_solver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(rail_repair_solver_tests
    test/src/Tests.cpp
)

target_include_directories(rail_repair_solver_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src
)

enable_testing()
add_test(NAME rail_repair_solver_tests COMMAND $<TARGET_FILE:rail_repair_solver_tests>)
