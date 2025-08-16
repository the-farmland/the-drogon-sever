FROM ubuntu:22.04 AS builder

# Install only essential build dependencies
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

# Build and install Drogon with complete doc/examples/test disable
RUN git clone https://github.com/drogonframework/drogon && \
    cd drogon && \
    git checkout v1.8.3 && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_DOC=OFF \
          -DBUILD_DOCS=OFF \
          -DBUILD_EXAMPLES=OFF \
          -DBUILD_TESTING=OFF \
          -DBUILD_ORM=ON \
          -DBUILD_SHARED_LIBS=ON \
          .. > cmake.log 2>&1 && \
    make -j$(nproc) VERBOSE=1 && \
    make install && \
    cd ../.. && \
    rm -rf drogon

WORKDIR /app
COPY . .

# Build our application with verbose output
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release --verbose

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