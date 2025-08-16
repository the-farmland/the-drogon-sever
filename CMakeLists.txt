cmake_minimum_required(VERSION 3.16)
project(ThePlusTVServer)

set(CMAKE_CXX_STANDARD 17)

# Find Drogon, which is the core framework for our server.
# This single command handles finding Drogon and its dependencies like jsoncpp, OpenSSL, etc.
find_package(Drogon REQUIRED)

# Find nlohmann_json for our business logic.
find_package(nlohmann_json 3.11.2 REQUIRED)

# Find the PostgreSQL client library (libpq).
find_library(LIBPQ_LIBRARY NAMES pq REQUIRED)
find_path(LIBPQ_INCLUDE_DIR NAMES libpq-fe.h REQUIRED)

# Add necessary include directories.
include_directories(
    .
    ${LIBPQ_INCLUDE_DIR}
)

# Define the server executable with all its source files.
# We've added RpcController.h and CorsFilter.h (header-only in this implementation).
add_executable(ThePlusTVServer
    main.cpp
    LocationService.cpp
)

# Link all required libraries to the executable.
# Drogon::Drogon links the main Drogon library and its dependencies.
# We still link libpq and nlohmann_json separately.
target_link_libraries(ThePlusTVServer PRIVATE
    Drogon::Drogon
    nlohmann_json::nlohmann_json
    ${LIBPQ_LIBRARY}
)
