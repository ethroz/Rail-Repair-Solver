cmake_minimum_required(VERSION 3.20)
project(rail_repair_solver LANGUAGES CXX)

if(MSVC)
    add_compile_options(/W3 /permissive-)
    add_compile_definitions(NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(absl REQUIRED)

add_executable(rail_repair_solver src/main.cpp)
target_include_directories(rail_repair_solver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)
target_link_libraries(rail_repair_solver PRIVATE
    abseil::abseil
)

add_executable(rail_repair_solver_tests test/src/Tests.cpp)
target_include_directories(rail_repair_solver_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/test/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
)
target_link_libraries(rail_repair_solver_tests PRIVATE
    abseil::abseil
)

enable_testing()
add_test(NAME rail_repair_solver_tests COMMAND $<TARGET_FILE:rail_repair_solver_tests>)
