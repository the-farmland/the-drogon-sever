cmake_minimum_required(VERSION 3.14)
project(drogon_locations)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Drogon REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Use official PostgreSQL CMake module
find_package(PostgreSQL REQUIRED)

# nlohmann_json
find_package(nlohmann_json 3.11.2 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_executable(drogon_locations main.cpp)

target_include_directories(drogon_locations PRIVATE ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(drogon_locations PRIVATE
    Drogon::Drogon
    OpenSSL::SSL OpenSSL::Crypto
    Threads::Threads
    nlohmann_json::nlohmann_json
    ${PostgreSQL_LIBRARIES}
)
