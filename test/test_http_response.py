"""Check the production HTTP sender against backpressure and partial transport writes."""
import subprocess
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[1]


@pytest.mark.parametrize("target", ["s3", "c3"])
def test_bounded_http_transfer(tmp_path, target):
    executable = tmp_path / "http_response.exe"
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", *(["-DCONFIG_IDF_TARGET_ESP32C3=1"] if target == "c3" else []),
         "-Itest/support/http", "-Itest/support", "-Iinclude",
         "test/support/http_response_regression.cpp", "src/antcore_http_response.cpp", "-o", str(executable)],
        cwd=ROOT, check=True,
    )
    subprocess.run([str(executable)], check=True, timeout=10)
