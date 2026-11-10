# ZionOS isolated build + test environment.
# Base matches the ZionOS target release (Debian 13 "trixie") so that
# live-build, debootstrap and custom .deb builds use the same toolchain
# and library ABI as the image being produced.
FROM debian:trixie

ENV DEBIAN_FRONTEND=noninteractive LANG=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
      # image build
      live-build debootstrap xorriso squashfs-tools mtools dosfstools \
      grub-pc-bin grub-efi-amd64-bin grub-efi-ia32-bin grub-common grub2-common \
      isolinux syslinux-common syslinux-utils cpio gzip xz-utils zstd \
      ca-certificates curl wget gnupg debian-archive-keyring apt-utils rsync git \
      sudo procps kmod file bc jq \
      # packaging
      build-essential debhelper devscripts dpkg-dev fakeroot lintian reprepro \
      cmake ninja-build pkgconf \
      # Qt 6 toolchain for ZionOS applications
      qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools \
      qt6-l10n-tools qt6-svg-dev qtkeychain-qt6-dev libgl1-mesa-dev \
      # static analysis / sanitizers
      cppcheck clang clang-tidy \
      # VM tests
      qemu-system-x86 qemu-utils ovmf socat python3 python3-pil \
      tesseract-ocr tesseract-ocr-eng tesseract-ocr-heb \
      # artwork
      librsvg2-bin imagemagick fonts-dejavu-core fonts-noto-core \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
