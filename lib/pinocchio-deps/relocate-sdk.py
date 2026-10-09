#!/usr/bin/env python3
"""Remove build-machine paths from installed SDK metadata."""

from pathlib import Path
import sys


prefix = Path(sys.argv[1]).resolve()
cmake_dir = prefix / "lib" / "cmake"
for config in cmake_dir.glob("boost_*-1.83.0/boost_*-config.cmake"):
    contents = config.read_text()
    start = contents.find("# If the computed and the original directories are symlink-equivalent")
    if start >= 0:
        end = contents.find("get_filename_component(_BOOST_INCLUDEDIR", start)
        if end < 0:
            raise ValueError(f"Unexpected Boost config format: {config}")
        config.write_text(contents[:start] + contents[end:])

for pc in (prefix / "lib" / "pkgconfig").glob("*.pc"):
    lines = pc.read_text().splitlines(keepends=True)
    for index, line in enumerate(lines):
        if line.startswith("prefix="):
            lines[index] = "prefix=${pcfiledir}/../..\n"
        elif line.startswith("libdir=/"):
            lines[index] = "libdir=${prefix}/lib\n"
        elif line.startswith("includedir=/"):
            lines[index] = "includedir=${prefix}/include\n"
    pc.write_text("".join(lines))
