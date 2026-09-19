#!/usr/bin/env python3
"""Format project sources, or check formatting without changing files."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
LOCAL_VENV_BIN = ROOT / ".venv" / ("Scripts" if os.name == "nt" else "bin")
WORKSPACE_VENV_BIN = (
    ROOT.parent / "murin-ros2" / ".venv" / ("Scripts" if os.name == "nt" else "bin")
)
EXCLUDED = {
    ".git",
    ".venv",
    "venv",
    "__pycache__",
    ".pytest_cache",
    "node_modules",
    "managed_components",
}
CPP_SUFFIXES = {".c", ".h", ".cc", ".cpp", ".cxx", ".hh", ".hpp", ".hxx"}


def source_files(root=ROOT):
    groups = {"cpp": [], "python": [], "cmake": []}
    for directory, dirs, files in os.walk(root):
        dirs[:] = sorted(
            d for d in dirs if d not in EXCLUDED and not d.startswith("build")
        )
        for name in sorted(files):
            path = Path(directory) / name
            relative = path.relative_to(root)
            first = relative.parts[0]
            if first in {"main", "tests"} and path.suffix in CPP_SUFFIXES:
                groups["cpp"].append(path)
            if first in {"tests", "tools", "utils", "scripts"} and path.suffix == ".py":
                groups["python"].append(path)
            if name == "CMakeLists.txt" or path.suffix == ".cmake":
                groups["cmake"].append(path)
    return groups


def commands(groups, check=False):
    for path in groups["cpp"]:
        yield [
            "clang-format",
            "--style=file",
            *(["--dry-run", "--Werror"] if check else ["-i"]),
            str(path),
        ]
    for path in groups["python"]:
        yield ["ruff", "format", *(["--check"] if check else []), str(path)]
    for path in groups["cmake"]:
        yield ["cmake-format", "--check" if check else "-i", str(path)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    venv_bin = next(
        (path for path in (LOCAL_VENV_BIN, WORKSPACE_VENV_BIN) if path.is_dir()), None
    )
    if venv_bin:
        os.environ["PATH"] = os.pathsep.join(
            [str(venv_bin), os.environ.get("PATH", "")]
        )
    planned = list(commands(source_files(), args.check))
    missing = sorted({cmd[0] for cmd in planned if not shutil.which(cmd[0])})
    if missing:
        print("Missing formatting tools: " + ", ".join(missing), file=sys.stderr)
        return 1
    failed = False
    for cmd in planned:
        result = subprocess.run(cmd, cwd=ROOT)
        failed |= result.returncode != 0
    return int(failed)


if __name__ == "__main__":
    sys.exit(main())
