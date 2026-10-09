#!/usr/bin/env bash
set -euo pipefail
deps=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
pinocchio="$deps/../pinocchio"
arch=${1:?Usage: bash build.sh x86_64|aarch64}
jobs=${PINOCCHIO_BUILD_JOBS:-2}
case "$arch" in
  x86_64)
    compiler=/usr/bin/g++-13
    toolset=gcc-native
    cmake_compiler=(-DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER="$compiler")
    boost_arch=x86
    ;;
  aarch64)
    compiler=/home/huang/toolchains/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++
    toolset=gcc-arm
    cmake_compiler=(-DCMAKE_TOOLCHAIN_FILE="$deps/aarch64-toolchain.cmake")
    boost_arch=arm
    ;;
  *) echo "Unsupported architecture: $arch" >&2; exit 1 ;;
esac
prefix=$(realpath -m "$deps/../artifacts/pinocchio/stage-$arch/usr")
archive="$deps/downloads/boost_1_83_0.tar.bz2"
boost_source="$deps/src/boost_1_83_0"
boost_url=https://archives.boost.io/release/1.83.0/source/boost_1_83_0.tar.bz2
boost_sha256=6478edfe2f3305127cffe8caf73ea0176c53769f4bf1585be237eb30798c3b8e
mkdir -p "$deps/downloads" "$deps/src"
if [[ ! -f "$archive" ]] || ! printf '%s  %s\n' "$boost_sha256" "$archive" | sha256sum -c --status; then
  curl --fail --location --retry 3 --output "$archive.part" "$boost_url"
  printf '%s  %s\n' "$boost_sha256" "$archive.part" | sha256sum -c
  mv "$archive.part" "$archive"
fi
if [[ ! -f "$boost_source/boost/version.hpp" ]]; then
  tar -xjf "$archive" -C "$deps/src"
fi
if [[ ! -x "$boost_source/b2" ]]; then
  (cd "$boost_source" && ./bootstrap.sh)
fi
mkdir -p "$prefix/include" "$prefix/share/eigen3" "$deps/build-$arch"
# Eigen is header-only; copy its headers and relocatable CMake package.
cp -a /usr/include/eigen3 "$prefix/include/"
cp -a /usr/share/eigen3/cmake "$prefix/share/eigen3/"
printf 'using gcc : %s : %s ;\n' "${toolset#gcc-}" "$compiler" > "$deps/build-$arch/user-config.jam"
(
  cd "$boost_source"
  ./b2 --user-config="$deps/build-$arch/user-config.jam" \
    --build-dir="$deps/build-$arch/boost" --prefix="$prefix" \
    --with-filesystem --with-serialization toolset="$toolset" \
    target-os=linux architecture="$boost_arch" address-model=64 \
    variant=release link=shared threading=multi cxxstd=17 \
    -j"$jobs" install
)
common=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib
  -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_FIND_ROOT_PATH="$prefix"
  '-DCMAKE_INSTALL_RPATH=$ORIGIN' -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF)
for dependency in console_bridge tinyxml2 urdfdom_headers urdfdom; do
  cmake -S "$deps/src/$dependency" -B "$deps/build-$arch/$dependency" \
    "${common[@]}" "${cmake_compiler[@]}" -DBUILD_TESTING=OFF
  cmake --build "$deps/build-$arch/$dependency" --parallel "$jobs"
  cmake --install "$deps/build-$arch/$dependency"
done
cmake -S "$pinocchio" -B "$pinocchio/build-$arch" \
  "${common[@]}" "${cmake_compiler[@]}" \
  -DBUILD_PYTHON_INTERFACE=OFF -DBUILD_WITH_URDF_SUPPORT=ON \
  -DBUILD_WITH_COLLISION_SUPPORT=OFF -DBUILD_EXAMPLES=OFF \
  -DBUILD_BENCHMARK=OFF -DBUILD_UTILS=OFF -DINSTALL_DOCUMENTATION=OFF
cmake --build "$pinocchio/build-$arch" --parallel "$jobs"
cmake --install "$pinocchio/build-$arch"
python3 "$deps/relocate-sdk.py" "$prefix"
printf 'Installed %s: %s\n' "$arch" "$prefix"
