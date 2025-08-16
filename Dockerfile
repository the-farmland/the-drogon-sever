# Stage 1: The Builder
# This stage will first build Drogon from source, then build our application.
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies required to build BOTH Drogon and your application.
# Drogon itself needs git, cmake, g++, zlib, openssl, jsoncpp, and uuid.
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        cmake \
        git \
        zlib1g-dev \
        libjsoncpp-dev \
        uuid-dev \
        libssl-dev \
        libpq-dev \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# --- Build Drogon from source ---
WORKDIR /opt
RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git submodule update --init && \
    mkdir build && \
    cd build && \
    # Configure cmake for Drogon. BUILD_CTL=OFF and BUILD_EXAMPLES=OFF
    # are optimizations as we don't need them in the container.
    cmake .. -DBUILD_CTL=OFF -DBUILD_EXAMPLES=OFF && \
    make -j$(nproc) && \
    make install

# --- Build Your Application ---
WORKDIR /app
COPY . .

# Create a build directory, run cmake, and build your project.
# It will automatically find the Drogon that was just installed into the system.
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release -- -j$(nproc)

# ---

# Stage 2: The Final Runtime Image
# This stage is identical to the one in Solution 1, but we must manually
# install Drogon's runtime dependencies since we didn't use a package manager.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies for your application AND for Drogon.
RUN apt-get update && \
    apt-get install -y \
        libpq5 \
        libjsoncpp1 \
        libuuid1 \
        libssl3 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy your compiled application from the builder stage.
COPY --from=builder /app/build/ThePlusTVServer .

# IMPORTANT: Copy the Drogon shared library we built from source in the builder stage.
COPY --from=builder /usr/local/lib/libdrogon.so /usr/local/lib/

# Update the dynamic linker's cache to find the newly added library.
RUN ldconfig

EXPOSE 8080
CMD ["./ThePlusTVServer"]