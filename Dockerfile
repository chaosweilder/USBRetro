FROM docker.io/debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH

# Install build essentials. We deliberately skip Debian's gcc-arm-none-eabi
# package (GCC 12.2.1) because PIO-USB's tightened handshake/turnaround paths
# are sensitive to compiler codegen — GCC 12 vs 15 produces different cycle
# counts in __not_in_flash_func code, breaking specific 2.4 GHz dongles and
# wireless pads. Pin to ARM GNU Toolchain 15.2.rel1 (GCC 15.2) to match the
# brew-installed local toolchain.
RUN apt update && \
    apt install -y --no-install-recommends \
      build-essential \
      cmake \
      git \
      curl \
      xz-utils \
      ca-certificates \
      python3 \
      python3-pip \
      vim && \
    apt autoremove -y && \
    apt clean && \
    rm -rf /var/lib/apt/lists/*

ARG ARM_TOOLCHAIN_VERSION=15.2.rel1
ARG ARM_TOOLCHAIN_SHA256_X86_64=597893282ac8c6ab1a4073977f2362990184599643b4c5ee34870a8215783a16
ARG ARM_TOOLCHAIN_SHA256_AARCH64=d061559d814b205ed30c5b7c577c03317ec447ca51cd5a159d26b12a5bbeb20c

RUN set -eux; \
    case "${TARGETARCH:-$(dpkg --print-architecture)}" in \
      amd64) ARCH=x86_64; SHA256="${ARM_TOOLCHAIN_SHA256_X86_64}" ;; \
      arm64) ARCH=aarch64; SHA256="${ARM_TOOLCHAIN_SHA256_AARCH64}" ;; \
      *) echo "Unsupported arch: ${TARGETARCH}" >&2; exit 1 ;; \
    esac; \
    TARBALL="arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-${ARCH}-arm-none-eabi.tar.xz"; \
    URL="https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_TOOLCHAIN_VERSION}/binrel/${TARBALL}"; \
    curl -fsSL -o "/tmp/${TARBALL}" "${URL}"; \
    echo "${SHA256}  /tmp/${TARBALL}" | sha256sum -c -; \
    mkdir -p /opt/arm-gnu-toolchain; \
    tar -xJf "/tmp/${TARBALL}" -C /opt/arm-gnu-toolchain --strip-components=1; \
    rm "/tmp/${TARBALL}"; \
    /opt/arm-gnu-toolchain/bin/arm-none-eabi-gcc --version

ENV PATH="/opt/arm-gnu-toolchain/bin:${PATH}"

# Build environment only - code is mounted at runtime
WORKDIR /workspace

CMD ["/bin/bash"]
