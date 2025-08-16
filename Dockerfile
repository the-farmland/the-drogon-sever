# Stage 1: The Builder
# This stage compiles the application.
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts during package installation.
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install dependencies.
# 1. Install 'software-properties-common' which provides the 'add-apt-repository' command.
# 2. Add the official Drogon PPA repository.
# 3. Update the package lists AGAIN to fetch the contents of the new PPA.
# 4. Install all the necessary build tools and development libraries.
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:drogon/drogon && \
    apt-get update && \
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
# The -- -j$(nproc) flag tells 'make' to use all available CPU cores for a faster build.
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --config Release -- -j$(nproc)

# ---

# Stage 2: The Final Runtime Image
# This stage creates the small, final image for deployment.
FROM ubuntu:22.04

# Prevent interactive prompts.
ENV DEBIAN_FRONTEND=noninteractive

# Install only the runtime libraries.
# We must add the Drogon PPA here as well, so apt knows where to find 'libdrogon1.9'.
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:drogon/drogon && \
    apt-get update && \
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