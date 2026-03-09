# syntax=docker/dockerfile:1
FROM ubuntu:22.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Basic validation of architecture; allow amd64(x86_64) only
ARG TARGETARCH
RUN if [ "$TARGETARCH" != "amd64" ]; then \
        echo "Unsupported TARGETARCH: \"$TARGETARCH\"" >&2 && exit 1; \
    fi

# Install build essentials and useful tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential curl git wget python3 python3-pip cmake ninja-build unzip \
    bzip2 make libusb-1.0-0 udev usbutils sudo dos2unix xxd iproute2 net-tools \
    clang-format doxygen && \
    apt-get clean

# Install SonarCloud build-wrapper
RUN curl -L -o /tmp/build-wrapper.zip https://sonarcloud.io/static/cpp/build-wrapper-linux-x86.zip && \
    unzip /tmp/build-wrapper.zip -d /opt/sonar-build-wrapper && \
    rm /tmp/build-wrapper.zip && \
    ln -s /opt/sonar-build-wrapper/build-wrapper-linux-x86/build-wrapper-linux-x86-64 /usr/local/bin/

# Install nrfutil and tools
RUN curl -L "https://files.nordicsemi.com/artifactory/swtools/external/nrfutil/executables/x86_64-unknown-linux-gnu/nrfutil" \
    -H 'user-agent: Docker-Build' --output /tmp/nrfutil && \
    mv /tmp/nrfutil /usr/local/bin/ && chmod +x /usr/local/bin/nrfutil && \
    nrfutil install device && nrfutil install nrf5sdk-tools

# Install Nordic CLI tools
ARG NORDIC_COMMAND_LINE_TOOLS_VERSION="10-24-0/nrf-command-line-tools-10.24.0"
RUN NCLT_BASE=https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x && \
    NCLT_URL="${NCLT_BASE}/${NORDIC_COMMAND_LINE_TOOLS_VERSION}_linux-amd64.tar.gz" && \
    mkdir -p tmp && cd tmp && \
    wget -qO - "$NCLT_URL" | tar --no-same-owner -xz && \
    mkdir -p /opt/SEGGER && \
    tar xzf JLink_*.tgz -C /opt/SEGGER && mv /opt/SEGGER/JLink* /opt/SEGGER/JLink && \
    cp -r ./nrf-command-line-tools /opt && \
    ln -s /opt/nrf-command-line-tools/bin/nrfjprog /usr/local/bin/nrfjprog && \
    ln -s /opt/nrf-command-line-tools/bin/mergehex /usr/local/bin/mergehex && \
    cd .. && rm -rf tmp

# Install ARM toolchain
RUN TOOLCHAIN_URL="https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2" && \
    mkdir -p /tmp && \
    curl -SL --retry 3 --retry-delay 5 "$TOOLCHAIN_URL" -o /tmp/toolchain.tar.bz2 && \
    tar xvjf /tmp/toolchain.tar.bz2 -C /opt/ && \
    rm /tmp/toolchain.tar.bz2 && \
    cd /usr/lib/x86_64-linux-gnu && \
    if [ -f libncurses.so.6 ]; then ln -sf libncurses.so.6 libncurses.so.5; fi && \
    if [ -f libtinfo.so.6 ]; then ln -sf libtinfo.so.6 libtinfo.so.5; fi

# Install nRF5 SDK
RUN curl -L https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/sdks/nrf5/binaries/nrf5_sdk_17.1.0_ddde560.zip \
    -H 'user-agent: Docker-Build' --output /tmp/nrf5_sdk.zip && \
    unzip /tmp/nrf5_sdk.zip -d /opt && rm /tmp/nrf5_sdk.zip && \
    echo "GNU_INSTALL_ROOT ?= /opt/gcc-arm-none-eabi-10.3-2021.10/bin/" > /opt/nRF5_SDK_17.1.0_ddde560/components/toolchain/gcc/Makefile.posix && \
    echo "GNU_VERSION ?= 10.3.1" >> /opt/nRF5_SDK_17.1.0_ddde560/components/toolchain/gcc/Makefile.posix && \
    echo "GNU_PREFIX ?= arm-none-eabi" >> /opt/nRF5_SDK_17.1.0_ddde560/components/toolchain/gcc/Makefile.posix

ENV SDK_ROOT=/opt/nRF5_SDK_17.1.0_ddde560

# Build micro-ecc
RUN cd ${SDK_ROOT}/external/micro-ecc && chmod +x build_all.sh && dos2unix build_all.sh && ./build_all.sh

# Install GoogleTest
RUN cd /tmp && git clone https://github.com/google/googletest.git -b v1.16.0 && \
    cd googletest && mkdir build && cd build && \
    cmake .. -DBUILD_GMOCK=OFF && make && sudo make install && \
    rm -rf /tmp/googletest

# Set up environment variables
RUN echo 'export LC_ALL=C.UTF-8' >> /etc/profile.d/env.sh && \
    echo 'export LANG=C.UTF-8' >> /etc/profile.d/env.sh && \
    echo 'export PRIVATE_KEY_PATH=/workspace/private_key.pem' >> /etc/profile.d/env.sh

# Install dependencies for VS Code
RUN apt-get update && apt-get install -y \
    libx11-xcb1 libxkbfile1 libsecret-1-0 libasound2 libnss3 libxss1 libgtk-3-0 libstdc++6 \
    && apt-get clean

WORKDIR /workspace

CMD ["/bin/bash"]