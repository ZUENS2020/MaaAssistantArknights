#!/usr/bin/env python3
"""Load the full resource tree through libMaaCore (AsstLoadResource).

Missing templates, broken task JSON, or other loader errors fail the process
the same way maa-cli does: AsstLoadResource returns false.

Usage:
    python3 tools/check_resource_load.py --lib-dir install
    python3 tools/check_resource_load.py --lib-dir install --resource .
"""

from __future__ import annotations

import argparse
import ctypes
import os
import platform
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CLIENTS = ("Official", "YoStarJP", "YoStarEN", "YoStarKR", "txwy")

LIB_NAMES = {
    "windows": "MaaCore.dll",
    "darwin": "libMaaCore.dylib",
    "linux": "libMaaCore.so",
}


def find_lib_dir(explicit: Path | None) -> Path:
    if explicit is not None:
        return explicit
    candidates = [
        REPO_ROOT / "install",
        REPO_ROOT / "build" / "bin" / "RelWithDebInfo",
        REPO_ROOT / "build" / "bin" / "Release",
        REPO_ROOT / "build" / "bin" / "Debug",
    ]
    system = platform.system().lower()
    libname = LIB_NAMES.get(system, "libMaaCore.so")
    for cand in candidates:
        if (cand / libname).is_file():
            return cand
    raise FileNotFoundError(
        "libMaaCore not found; pass --lib-dir (tried: "
        + ", ".join(str(p) for p in candidates)
        + ")"
    )


def load_library(lib_dir: Path):
    system = platform.system().lower()
    libname = LIB_NAMES.get(system, "libMaaCore.so")
    lib_path = lib_dir / libname
    if not lib_path.is_file():
        raise FileNotFoundError(lib_path)

    env_key = {
        "windows": "PATH",
        "darwin": "DYLD_LIBRARY_PATH",
        "linux": "LD_LIBRARY_PATH",
    }.get(system, "LD_LIBRARY_PATH")
    os.environ[env_key] = str(lib_dir) + os.pathsep + os.environ.get(env_key, "")

    loader = ctypes.WinDLL if system == "windows" else ctypes.CDLL
    return loader(str(lib_path))


def declare(lib) -> None:
    c = ctypes.c_char_p
    lib.AsstSetUserDir.argtypes, lib.AsstSetUserDir.restype = [c], ctypes.c_bool
    lib.AsstLoadResource.argtypes, lib.AsstLoadResource.restype = [c], ctypes.c_bool
    lib.AsstGetVersion.argtypes, lib.AsstGetVersion.restype = [], ctypes.c_char_p


def print_log_errors(user_dir: Path) -> None:
    log_path = user_dir / "debug" / "asst.log"
    if not log_path.is_file():
        # Logger may also write asst.log at the user-dir root.
        alt = user_dir / "asst.log"
        log_path = alt if alt.is_file() else log_path
    if not log_path.is_file():
        print(f"(no asst.log under {user_dir})", file=sys.stderr)
        return
    print(f"--- {log_path} (errors) ---", file=sys.stderr)
    for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if (
            "[ERR]" in line
            or "Templ load failed" in line
            or "load resource" in line.lower()
        ):
            print(line, file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lib-dir", type=Path, default=None)
    parser.add_argument("--resource", type=Path, default=REPO_ROOT)
    parser.add_argument(
        "--clients",
        nargs="*",
        default=list(CLIENTS),
        help="Official then incremental global overlays (same as tools/SmokeTesting)",
    )
    args = parser.parse_args()

    lib_dir = find_lib_dir(args.lib_dir)
    resource = args.resource.resolve()
    if not (resource / "resource").is_dir():
        print(f"resource/ not found under {resource}", file=sys.stderr)
        return 2

    lib = load_library(lib_dir)
    declare(lib)

    with tempfile.TemporaryDirectory(prefix="maa-resource-load-") as tmp:
        user_dir = Path(tmp)
        if not lib.AsstSetUserDir(str(user_dir).encode("utf-8")):
            print("AsstSetUserDir failed", file=sys.stderr)
            return 1

        version = lib.AsstGetVersion()
        version_s = version.decode("utf-8") if version else ""
        print(f"AsstGetVersion: {version_s}")
        print(f"lib: {lib_dir}")
        print(f"resource: {resource}")

        if not lib.AsstLoadResource(str(resource).encode("utf-8")):
            print("AsstLoadResource failed: Official", file=sys.stderr)
            print_log_errors(user_dir)
            return 1
        print("AsstLoadResource: Official OK")

        for client in args.clients:
            if client in ("", "Official"):
                continue
            overseas = resource / "resource" / "global" / client
            if not overseas.is_dir():
                print(f"skip missing global client {client}")
                continue
            if not lib.AsstLoadResource(str(overseas).encode("utf-8")):
                print(f"AsstLoadResource failed: {client}", file=sys.stderr)
                print_log_errors(user_dir)
                return 1
            print(f"AsstLoadResource: {client} OK")

    return 0


if __name__ == "__main__":
    sys.exit(main())
