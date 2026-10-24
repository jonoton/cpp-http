FROM mcr.microsoft.com/devcontainers/cpp:1-debian-12

# Install minimal additional tools if needed
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get -y install --no-install-recommends \
    cmake \
    build-essential \
    gdb \
    xclip \
    ruby-full \
    zlib1g-dev \
    clangd \
    tcpdump \
    chromium \
    && apt-get clean -y && rm -rf /var/lib/apt/lists/*

# Install Jekyll and Bundler
RUN gem install jekyll bundler

# Install curl with HTTP/3 support
RUN curl -LO https://github.com/stunnel/static-curl/releases/download/8.21.0/curl-linux-x86_64-glibc-8.21.0.tar.xz \
    && tar -xJvf curl-linux-x86_64-glibc-8.21.0.tar.xz \
    && mv curl /usr/local/bin/curl-http3 \
    && rm -f SHA256SUMS curl-linux-x86_64-glibc-8.21.0.tar.xz

# Install Node.js 22 (required for Artillery benchmarks)
RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y nodejs \
    && apt-get clean -y && rm -rf /var/lib/apt/lists/*

