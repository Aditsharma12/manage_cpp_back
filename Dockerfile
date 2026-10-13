# Multi-stage Docker build for Drogon C++ + PostgreSQL backend
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    openssl \
    libssl-dev \
    pkg-config \
    libpq-dev \
    postgresql-server-dev-all \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Drogon framework with PostgreSQL ORM enabled
WORKDIR /tmp
RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git checkout v1.9.10 && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DBUILD_TESTING=OFF \
      -DBUILD_EXAMPLES=OFF \
      -DBUILD_POSTGRESQL=ON \
      -DBUILD_MYSQL=OFF \
      -DBUILD_SQLITE=OFF \
      -DBUILD_REDIS=OFF && \
    cmake --build . --target install -j$(nproc)

# Build our application
WORKDIR /app
COPY . .
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

# Runtime container
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libjsoncpp25 \
    libssl3 \
    zlib1g \
    libpq5 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /usr/local/lib/libdrogon* /usr/local/lib/
COPY --from=builder /usr/local/lib/libtrantor* /usr/local/lib/
RUN ldconfig

COPY --from=builder /app/build/backend_fast /app/backend_fast
COPY --from=builder /app/config.json /app/config.json

EXPOSE 5000

CMD ["./backend_fast", "config.json"]
