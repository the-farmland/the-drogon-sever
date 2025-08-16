# Stage 1: The Builder
# This stage installs all development tools and libraries, compiles the application,
# but will be discarded later to keep the final image small.
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install all necessary build dependencies.
# The 'libdrogon-dev' package automatically pulls in the correct versions of
# its own dependencies (like libjsoncpp-dev) for Ubuntu 22.04.
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        cmake \
        git \
        libdrogon-dev \
        libpq-dev \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory and copy the entire project into the container.
WORKDIR /app
COPY . .

# Create a build directory, run cmake to configure the project, and then build it.
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release -- -j$(nproc)

# ---

# Stage 2: The Final Runtime Image
# This stage starts from a fresh, clean base image to ensure it's minimal.
FROM ubuntu:22.04

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install only the runtime shared libraries that our compiled application needs.
# The 'libdrogon1.9' package automatically pulls in the correct runtime dependencies
# (like libjsoncpp25), avoiding the error you encountered.
RUN apt-get update && \
    apt-get install -y \
        libpq5 \
        libdrogon1.9 \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory.
WORKDIR /app

# Copy ONLY the compiled application binary from the builder stage.
COPY --from=builder /app/build/ThePlusTVServer .

# Expose the port the Drogon server is configured to listen on.
EXPOSE 8080

# Define the command to run when the container starts.
CMD ["./ThePlusTVServer"]