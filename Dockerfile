# Stage 1: Builder
# Use a base image with essential build tools.
FROM ubuntu:22.04 AS builder

# Install all build dependencies, including Drogon.
# libdrogon-dev pulls in most necessary components.
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        cmake \
        git \
        libpq-dev \
        nlohmann-json3-dev \
        libdrogon-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy source code and build the project.
WORKDIR /app
COPY . .
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

# Stage 2: Final Runtime Image
FROM ubuntu:22.04

# Install only the runtime dependencies needed.
# libdrogon1.9 is the runtime library for Drogon.
RUN apt-get update && \
    apt-get install -y \
        libpq5 \
        libdrogon1.9 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the compiled binary from the builder stage.
COPY --from=builder /app/build/ThePlusTVServer .

# Expose the port the server will listen on.
EXPOSE 8080

# The command to run the server.
CMD ["./ThePlusTVServer"]