# Stage 1: Get Drogon from official image
FROM drogonframework/drogon:v2.0.0 AS drogon-builder

# Stage 2: Build our application
FROM ubuntu:22.04 AS builder

# Copy Drogon installation
COPY --from=drogon-builder /usr/local /usr/local

# Install build dependencies
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        cmake \
        git \
        libboost-system-dev \
        libboost-thread-dev \
        libpq-dev \
        libssl-dev \
        zlib1g-dev \
        uuid-dev \
        libjsoncpp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Build
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

# Stage 3: Runtime image
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && \
    apt-get install -y \
        libboost-system1.74.0 \
        libboost-thread1.74.0 \
        libpq5 \
        libssl3 \
        libjsoncpp25 \
        libuuid1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/ThePlusTVServer .

# Configuration
ENV ALLOWED_ORIGINS="https://the-super-sweet-two.vercel.app,http://localhost:3000"
EXPOSE 8080
CMD ["./ThePlusTVServer"]