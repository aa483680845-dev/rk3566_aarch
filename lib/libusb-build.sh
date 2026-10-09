#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir="$root/libusb"
arch=${1:?Usage: bash lib/libusb-build.sh x86_64|aarch64}
jobs=${LIBUSB_BUILD_JOBS:-2}
case "$arch" in
  x86_64)
    cc=/usr/bin/gcc
    host=()
    ;;
  aarch64)
    toolchain_root=${ARM_TOOLCHAIN_ROOT:-/home/huang/toolchains/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu}
    cc="$toolchain_root/bin/aarch64-none-linux-gnu-gcc"
    host=(--host=aarch64-none-linux-gnu)
    ;;
  *) echo "Unsupported architecture: $arch" >&2; exit 1 ;;
esac
[[ -f "$source_dir/configure.ac" ]] || { echo "Initialize lib/libusb submodule first" >&2; exit 1; }
[[ -x "$cc" ]] || { echo "Compiler not found: $cc" >&2; exit 1; }
if [[ ! -x "$source_dir/configure" ]]; then
  NOCONFIGURE=1 "$source_dir/autogen.sh"
fi
build_dir="$root/libusb/build-$arch"
prefix="$root/artifacts/libusb/stage-$arch/usr"
mkdir -p "$build_dir" "$prefix"
(
  cd "$build_dir"
  CC="$cc" "$source_dir/configure" --prefix=/usr --disable-udev "${host[@]}"
  make -j"$jobs"
  make DESTDIR="${prefix%/usr}" install
)
python3 "$root/pinocchio-deps/relocate-sdk.py" "$prefix"
printf 'Installed %s: %s\n' "$arch" "$prefix"
