FROM ubuntu:22.04 AS builder

# Install build dependencies and Drogon
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
        wget \
    && rm -rf /var/lib/apt/lists/*

# Install Drogon from package (alternative approach)
RUN wget https://github.com/drogonframework/drogon/releases/download/v2.0.0/drogon-v2.0.0-ubuntu22.04-amd64.deb -O drogon.deb && \
    apt-get install -y ./drogon.deb && \
    rm drogon.deb

WORKDIR /app
COPY . .

# Build
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

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