FROM ubuntu:22.04 AS builder

# Install all required dependencies including Drogon
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        cmake \
        git \
        libboost-system-dev \
        libboost-thread-dev \
        libboost-random-dev \
        libwebsocketpp-dev \
        nlohmann-json3-dev \
        libpq-dev \
        libssl-dev \
        wget \
        gnupg \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Drogon
RUN wget https://github.com/drogonframework/drogon/releases/download/v1.9.1/drogon-v1.9.1-ubuntu22.04-amd64.deb && \
    apt-get install -y ./drogon-v1.9.1-ubuntu22.04-amd64.deb && \
    rm drogon-v1.9.1-ubuntu22.04-amd64.deb

WORKDIR /app
COPY . .

# Configure and build the project
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
        libbrotli1 \
        libzstd1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/ThePlusTVServer .

# Set default CORS allowed origins
ENV ALLOWED_ORIGINS="https://the-super-sweet-two.vercel.app,http://localhost:3000"
EXPOSE 8080
CMD ["./ThePlusTVServer"]