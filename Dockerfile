# OpenFIDO Development Container
# Multi-stage build for optimized development environment

# Stage 1: Base image with build tools
FROM ubuntu:22.04 AS base

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install essential development tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    unzip \
    python3 \
    python3-pip \
    libmbedtls-dev \
    clang \
    clang-format \
    clang-tidy \
    cppcheck \
    doxygen \
    graphviz \
    lcov \
    gdb \
    && rm -rf /var/lib/apt/lists/*

# Install PlatformIO
RUN pip3 install --no-cache-dir platformio

# Stage 2: Development environment
FROM base AS development

# Set working directory
WORKDIR /workspace

# Copy project files
COPY . .

# Create build directories
RUN mkdir -p build build_coverage build_benchmark

# Configure CMake
RUN cd build && cmake ..

# Expose common ports (for debugging, web server, etc.)
EXPOSE 8080 3000

# Default command
CMD ["/bin/bash"]

# Stage 3: Builder image (for CI/CD)
FROM base AS builder

WORKDIR /workspace
COPY . .

# Build the project
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Run tests
RUN cd build && ctest --output-on-failure || true

# Labels
LABEL org.opencontainers.image.title="OpenFIDO Development Environment"
LABEL org.opencontainers.image.description="Complete development environment for OpenFIDO FIDO2 security key"
LABEL org.opencontainers.image.vendor="OpenFIDO"
LABEL org.opencontainers.image.licenses="MIT"
