FROM ubuntu:22.04 AS builder

# Install all dependencies
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

# Build and install Drogon from source (using known good version)
RUN git clone https://github.com/drogonframework/drogon && \
    cd drogon && \
    git checkout v1.8.4 && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_DOC=OFF \
          -DBUILD_EXAMPLES=OFF \
          -DBUILD_TESTING=OFF \
          .. && \
    make -j$(nproc) && \
    make install && \
    cd ../.. && \
    rm -rf drogon

WORKDIR /app
COPY . .

# Build our application
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release

FROM ubuntu:22.04

# Runtime dependencies
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

ENV ALLOWED_ORIGINS="https://the-super-sweet-two.vercel.app,http://localhost:3000"
EXPOSE 8080
CMD ["./ThePlusTVServer"]