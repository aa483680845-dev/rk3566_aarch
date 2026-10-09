#!/usr/bin/env bash
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$project_dir/cmake-build-release-aarch"
libusb_dir="$project_dir/../lib/libusb/stage-aarch64/usr/lib"
pinocchio_lib_dir="$project_dir/../lib/pinocchio/stage-aarch64/usr/lib"
output="${1:-$project_dir/u2canfd-rk3566-aarch64-$(date +%Y%m%d).tar.gz}"

for path in "$build_dir/dm_main" "$build_dir/dev_sn" \
            "$build_dir/libgravity_compensator.so" \
            "$project_dir/config/controller.ini" \
            "$project_dir/model/robot_urdf/robot.urdf" \
            "$project_dir/lib/aarch64/libdm_device.so" \
            "$libusb_dir/libusb-1.0.so.0" "$build_dir/CMakeCache.txt"; do
    if [[ ! -f "$path" ]]; then
        printf '缺少文件：%s\n' "$path" >&2
        exit 1
    fi
done

compiler=$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "$build_dir/CMakeCache.txt" | head -n 1)
if [[ ! -x "$compiler" ]] || [[ "$("$compiler" -dumpmachine)" != aarch64-* ]]; then
    printf '当前构建目录未使用 AArch64 交叉编译器：%s\n' "$compiler" >&2
    exit 1
fi
if ! LC_ALL=C readelf -h "$build_dir/dm_main" | grep -q 'Machine:.*AArch64'; then
    printf 'dm_main 不是 AArch64 二进制文件\n' >&2
    exit 1
fi

libstdcxx=$("$compiler" -print-file-name=libstdc++.so.6)
libgcc=$("$compiler" -print-file-name=libgcc_s.so.1)
for path in "$libstdcxx" "$libgcc"; do
    if [[ ! -f "$path" ]]; then
        printf '缺少交叉编译器运行库：%s\n' "$path" >&2
        exit 1
    fi
done

staging_dir=$(mktemp -d)
archive_tmp=$(mktemp "${output}.tmp.XXXXXX")
trap 'rm -rf -- "$staging_dir"; rm -f -- "$archive_tmp"' EXIT
package_dir="$staging_dir/u2canfd-rk3566-aarch64"
mkdir -p "$package_dir/bin" "$package_dir/lib" "$package_dir/config" "$package_dir/model"

cp "$build_dir/dm_main" "$build_dir/dev_sn" "$package_dir/bin/"
cp "$project_dir/config/controller.ini" "$package_dir/config/"
cp -a "$project_dir/model/robot_urdf" "$package_dir/model/"
cp "$build_dir/libgravity_compensator.so" "$package_dir/lib/"
cp -L "$project_dir/lib/aarch64/libdm_device.so" "$package_dir/lib/"
cp -L "$libusb_dir/libusb-1.0.so.0" "$package_dir/lib/"
cp -L "$libstdcxx" "$package_dir/lib/libstdc++.so.6"
cp -L "$libgcc" "$package_dir/lib/libgcc_s.so.1"
for library in "$pinocchio_lib_dir"/*.so*; do
    cp -L "$library" "$package_dir/lib/$(basename "$library")"
done

cat > "$package_dir/run.sh" <<'EOF'
#!/bin/sh
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$APP_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$APP_DIR/bin/dm_main" "$@"
EOF
cat > "$package_dir/dev_sn.sh" <<'EOF'
#!/bin/sh
set -eu
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$APP_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$APP_DIR/bin/dev_sn" "$@"
EOF
chmod +x "$package_dir/run.sh" "$package_dir/dev_sn.sh"
cat > "$package_dir/README.txt" <<'EOF'
在 RK3566 上解压本包，编辑 config/controller.ini，进入 u2canfd-rk3566-aarch64 目录，运行 ./run.sh。
修改配置后重启程序生效。也可运行 ./run.sh --config /path/to/controller.ini。
使用 ./dev_sn.sh 查看设备序列号。目标系统需要兼容打包的运行库。
本包不包含 glibc 和系统动态加载器；请核对设备系统版本与交叉编译工具链的 ABI。
EOF

tar -C "$staging_dir" -czf "$archive_tmp" u2canfd-rk3566-aarch64
tar -tzf "$archive_tmp" >/dev/null
mv -f -- "$archive_tmp" "$output"
printf '已生成：%s\n' "$output"
sha256sum "$output"
