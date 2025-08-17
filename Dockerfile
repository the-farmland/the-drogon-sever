# Use Ubuntu as base
FROM ubuntu:22.04 AS build

# Install dependencies
RUN apt-get update && apt-get install -y \
    git cmake g++ make openssl libssl-dev zlib1g-dev uuid-dev wget curl \
    && rm -rf /var/lib/apt/lists/*

# Build and install Drogon
RUN git clone https://github.com/drogonframework/drogon.git /tmp/drogon \
    && cd /tmp/drogon \
    && git submodule update --init \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
    && make -j$(nproc) && make install \
    && ldconfig

# Build your app
WORKDIR /app
COPY . .
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

# Final lightweight image
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    libssl-dev zlib1g uuid-runtime \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/lib /usr/local/lib
COPY --from=build /usr/local/include /usr/local/include
COPY --from=build /usr/local/bin/drogon_ctl /usr/local/bin/
COPY --from=build /app/build/drogon_hello /usr/local/bin/drogon_hello

# Set LD_LIBRARY_PATH
ENV LD_LIBRARY_PATH=/usr/local/lib

EXPOSE 8080
CMD ["drogon_hello"]
