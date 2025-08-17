# Build stage
FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y \
    git cmake g++ make openssl libssl-dev zlib1g-dev uuid-dev wget curl \
    libjsoncpp-dev libc-ares-dev libpq-dev \
    && rm -rf /var/lib/apt/lists/*

# Drogon build
RUN git clone https://github.com/drogonframework/drogon.git /tmp/drogon \
    && cd /tmp/drogon && git submodule update --init \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
    && make -j$(nproc) && make install && ldconfig

WORKDIR /app
COPY . .
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

# Runtime
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    libssl-dev zlib1g uuid-runtime libc-ares-dev libpq5 libjsoncpp25 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/lib /usr/local/lib
COPY --from=build /usr/local/include /usr/local/include
COPY --from=build /usr/local/bin/drogon_ctl /usr/local/bin/
COPY --from=build /app/build/drogon_locations /usr/local/bin/drogon_locations

ENV LD_LIBRARY_PATH=/usr/local/lib
EXPOSE 8080
CMD ["drogon_locations"]

