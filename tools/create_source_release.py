#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Komorebi P. and Polyphase Float contributors
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Create deterministic Polyphase Float source archives and checksums."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import os
import re
import subprocess
import sys
import tarfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path

DEFAULT_EPOCH = int(datetime(2026, 8, 31, tzinfo=timezone.utc).timestamp())
EXCLUDED_PARTS = {
    ".git",
    "build",
    "bin",
    "lib",
    "stage",
    "dist",
    "__pycache__",
    ".cache",
    ".idea",
    ".vscode",
}


def version_from_cmake(root: Path) -> str:
    text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(
        r"project\s*\(\s*PolyphaseFloat.*?\bVERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
        text,
        re.S,
    )
    if not match:
        raise RuntimeError("unable to parse project version")
    return match.group(1)


def included_files(root: Path, output_dir: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if output_dir == path or output_dir in path.parents:
            continue
        if any(part in EXCLUDED_PARTS or part.startswith("build-") for part in relative.parts):
            continue
        if path.name.endswith("~") or path.suffix in {".tmp", ".pyc", ".o", ".a", ".so"}:
            continue
        if "results" in relative.parts and relative.name.startswith("raw_local_"):
            continue
        files.append(path)
    return sorted(files, key=lambda p: p.relative_to(root).as_posix())


def normalized_mode(path: Path) -> int:
    with path.open("rb") as stream:
        executable = stream.read(2) == b"#!"
    return 0o755 if executable else 0o644


def make_tar_gz(root: Path, files: list[Path], output: Path, prefix: str, epoch: int) -> None:
    tar_buffer = io.BytesIO()
    with tarfile.open(fileobj=tar_buffer, mode="w", format=tarfile.PAX_FORMAT) as archive:
        for path in files:
            relative = path.relative_to(root).as_posix()
            info = tarfile.TarInfo(f"{prefix}/{relative}")
            data = path.read_bytes()
            info.size = len(data)
            info.mode = normalized_mode(path)
            info.mtime = epoch
            info.uid = 0
            info.gid = 0
            info.uname = "root"
            info.gname = "root"
            archive.addfile(info, io.BytesIO(data))
    with output.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=epoch, compresslevel=9) as compressed:
            compressed.write(tar_buffer.getvalue())


def make_zip(root: Path, files: list[Path], output: Path, prefix: str, epoch: int) -> None:
    dt = datetime.fromtimestamp(max(epoch, 315532800), tz=timezone.utc)
    zip_time = (dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in files:
            relative = path.relative_to(root).as_posix()
            info = zipfile.ZipInfo(f"{prefix}/{relative}", date_time=zip_time)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.create_system = 3
            info.external_attr = normalized_mode(path) << 16
            archive.writestr(info, path.read_bytes())


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    parser.add_argument("--skip-audit", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    if not args.skip_audit:
        result = subprocess.run([sys.executable, str(root / "tools/release_audit.py"), "--root", str(root)])
        if result.returncode != 0:
            return result.returncode

    version = version_from_cmake(root)
    prefix = f"polyphase-float-{version}"
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", DEFAULT_EPOCH))
    files = included_files(root, output_dir)
    if not files:
        raise SystemExit("no source files selected")

    tar_path = output_dir / f"{prefix}.tar.gz"
    zip_path = output_dir / f"{prefix}.zip"
    make_tar_gz(root, files, tar_path, prefix, epoch)
    make_zip(root, files, zip_path, prefix, epoch)

    checksum_path = output_dir / "SHA256SUMS"
    checksum_path.write_text(
        f"{sha256(tar_path)}  {tar_path.name}\n"
        f"{sha256(zip_path)}  {zip_path.name}\n",
        encoding="utf-8",
    )

    print(f"created {tar_path}")
    print(f"created {zip_path}")
    print(f"created {checksum_path}")
    print(f"files={len(files)} source_date_epoch={epoch}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
