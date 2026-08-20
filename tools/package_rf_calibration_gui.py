#!/usr/bin/env python3
"""Package the TK8710 RF calibration GUI as a single Windows executable."""

import argparse
import os
import shutil
import subprocess
import sys
from datetime import datetime
from importlib import metadata
from pathlib import Path

from packaging.version import Version


APP_NAME = "TK8710_RF_Test_GUI"
MIN_PYINSTALLER_VERSION = Version("6.22.0")
REQUIRED_RUNTIME_FILES = (
    Path("build_jtool") / "Test8710RFTest.exe",
    Path("build_jtool") / "jtool.dll",
)


def require_supported_pyinstaller() -> None:
    try:
        installed_version = Version(metadata.version("PyInstaller"))
    except metadata.PackageNotFoundError as exc:
        raise RuntimeError(
            "PyInstaller is not installed. Run: "
            "python -m pip install --user 'pyinstaller>=6.22.0'"
        ) from exc

    if installed_version < MIN_PYINSTALLER_VERSION:
        raise RuntimeError(
            f"PyInstaller {installed_version} is too old; {MIN_PYINSTALLER_VERSION} or newer is required.\n"
            "Upgrade it with:\n"
            "  python -m pip install --user --upgrade 'pyinstaller>=6.22.0'"
        )


def run(command, cwd: Path) -> None:
    print("+", " ".join(str(part) for part in command))
    subprocess.run(command, cwd=str(cwd), check=True)


def archive_dist_extras(repo_root: Path, target_exe: Path) -> None:
    dist_dir = target_exe.parent
    if not dist_dir.exists():
        return
    archive_root = repo_root / "build_pyinstaller" / "old_dist_artifacts"
    archive_root.mkdir(parents=True, exist_ok=True)
    target_resolved = target_exe.resolve()

    for item in dist_dir.iterdir():
        if item.resolve() == target_resolved:
            continue
        archive_target = archive_root / item.name
        if archive_target.exists():
            stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            archive_target = archive_root / f"{item.name}.{stamp}"
        shutil.move(str(item), str(archive_target))
        print(f"moved stale dist entry {item} -> {archive_target}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build a single-file Windows executable for rf_calibration_gui.py."
    )
    parser.add_argument("--name", default=APP_NAME, help="PyInstaller application name")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    gui_script = repo_root / "tools" / "rf_calibration_gui.py"
    pyinstaller_work = repo_root / "build_pyinstaller"
    dist_exe = repo_root / "dist" / f"{args.name}.exe"

    require_supported_pyinstaller()

    missing = [str(repo_root / item) for item in REQUIRED_RUNTIME_FILES if not (repo_root / item).exists()]
    if missing:
        raise FileNotFoundError(
            "Missing runtime files. Build the JTOOL target first:\n"
            "  mingw32-make -f cmake/Makefile.jtool build_jtool/Test8710RFTest.exe copy_dll\n"
            "Missing:\n  "
            + "\n  ".join(missing)
        )

    command = [
        sys.executable,
        "-m",
        "PyInstaller",
        "--noconfirm",
        "--clean",
        "--onefile",
        "--windowed",
        "--name",
        args.name,
        "--distpath",
        str(repo_root / "dist"),
        "--workpath",
        str(pyinstaller_work),
        "--specpath",
        str(pyinstaller_work),
    ]
    for item in REQUIRED_RUNTIME_FILES:
        command.extend(["--add-binary", f"{repo_root / item}{os.pathsep}build_jtool"])
    command.append(str(gui_script))
    run(command, repo_root)
    archive_dist_extras(repo_root, dist_exe)

    print(f"Packaged application: {dist_exe}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
