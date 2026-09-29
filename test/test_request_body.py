"""Ensure split HTTP config bodies cannot overrun or silently omit bytes."""
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_config_body_chunks(tmp_path):
    executable = tmp_path / "request_body.exe"
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Iinclude", "test/support/request_body_regression.cpp",
         "-o", str(executable)],
        cwd=ROOT, check=True,
    )
    subprocess.run([str(executable)], check=True, timeout=10)
