cmake_minimum_required(VERSION 3.14)
project(drogon_hello)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optional: add warnings
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Drogon REQUIRED)

add_executable(drogon_hello main.cpp)
target_link_libraries(drogon_hello PRIVATE Drogon::Drogon)
