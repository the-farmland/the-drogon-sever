cmake_minimum_required(VERSION 3.14)
project(drogon_hello)

set(CMAKE_CXX_STANDARD 17)

find_package(Drogon REQUIRED)

add_executable(drogon_hello main.cpp)

target_link_libraries(drogon_hello PRIVATE Drogon::Drogon)
