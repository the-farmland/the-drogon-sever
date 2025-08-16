FROM ubuntu:22.04 AS builder

# Install all required dependencies including graphviz for documentation
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
        zlib1g-dev \
        uuid-dev \
        libjsoncpp-dev \
        libbrotli-dev \
        libzstd-dev \
        doxygen \
        graphviz \
        dia \
    && rm -rf /var/lib/apt/lists/*

# Build and install Drogon from source with all optional features disabled
RUN git clone https://github.com/drogonframework/drogon && \
    cd drogon && \
    git checkout v2.0.0 && \  # Using newer version
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_DOC=OFF \
          -DBUILD_EXAMPLES=OFF \
          -DBUILD_TESTING=OFF \
          -DBUILD_SHARED_LIBS=ON \
          .. && \
    make -j$(nproc) && \
    make install && \
    cd ../.. && \
    rm -rf drogon

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