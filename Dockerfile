# Stage 1: The Builder
# This stage compiles the application.
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts during package installation.
ENV DEBIAN_FRONTEND=noninteractive

# --- Add Drogon PPA and Install Dependencies ---
# This is the robust, manual method for adding a PPA in Docker.
RUN apt-get update && \
    # 1. Install prerequisite tools: gnupg for key management, curl to download files.
    apt-get install -y gnupg curl ca-certificates && \
    # 2. Download the Drogon PPA's GPG key, de-armor it, and save it to the trusted keys directory.
    curl -sSL 'http://keyserver.ubuntu.com/pks/lookup?op=get&search=0x742618416D396255D9352441D3E5567384484179' | gpg --dearmor -o /usr/share/keyrings/drogon.gpg && \
    # 3. Add the Drogon PPA to the APT sources list, pointing to the key we just saved.
    #    'jammy' is the codename for Ubuntu 22.04.
    echo "deb [signed-by=/usr/share/keyrings/drogon.gpg] http://ppa.launchpadcontent.net/drogon/drogon/ubuntu jammy main" > /etc/apt/sources.list.d/drogon.list && \
    # 4. Update package lists again to include packages from the new PPA.
    apt-get update && \
    # 5. Now, install all build dependencies.
    apt-get install -y \
        build-essential \
        cmake \
        git \
        libdrogon-dev \
        libpq-dev \
        nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory and copy the project source code.
WORKDIR /app
COPY . .

# Configure the project with CMake and compile it in Release mode.
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release -- -j$(nproc)

# ---

# Stage 2: The Final Runtime Image
# This stage creates the small, final image for deployment.
FROM ubuntu:22.04

# Prevent interactive prompts.
ENV DEBIAN_FRONTEND=noninteractive

# --- Add Drogon PPA and Install Runtime Dependencies ---
# We must repeat the PPA addition process here for the new stage.
RUN apt-get update && \
    apt-get install -y gnupg curl ca-certificates && \
    curl -sSL 'http://keyserver.ubuntu.com/pks/lookup?op=get&search=0x742618416D396255D9352441D3E5567384484179' | gpg --dearmor -o /usr/share/keyrings/drogon.gpg && \
    echo "deb [signed-by=/usr/share/keyrings/drogon.gpg] http://ppa.launchpadcontent.net/drogon/drogon/ubuntu jammy main" > /etc/apt/sources.list.d/drogon.list && \
    apt-get update && \
    # Now install only the necessary runtime libraries.
    apt-get install -y \
        libpq5 \
        libdrogon1.9 \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory.
WORKDIR /app

# Copy ONLY the compiled application binary from the builder stage.
COPY --from=builder /app/build/ThePlusTVServer .

# Expose the port the server will listen on.
EXPOSE 8080

# Define the command to run when the container starts.
CMD ["./ThePlusTVServer"]