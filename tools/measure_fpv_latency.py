#!/usr/bin/env python3
"""Measure Ant Core FPV HTTP responsiveness over the robot AP."""

from __future__ import annotations

import argparse
import json
import time
import urllib.request
from dataclasses import dataclass


@dataclass
class Timing:
    ms: float
    bytes_read: int


def fetch(url: str, timeout: float = 4.0) -> tuple[bytes, float]:
    start = time.perf_counter()
    with urllib.request.urlopen(url, timeout=timeout) as response:
        body = response.read()
    return body, (time.perf_counter() - start) * 1000.0


def measure_snapshots(base: str, count: int) -> list[Timing]:
    timings: list[Timing] = []
    for _ in range(count):
        body, ms = fetch(f"{base}:81/snapshot.jpg")
        timings.append(Timing(ms=ms, bytes_read=len(body)))
    return timings


def measure_stream(base: str, target_frames: int, timeout_s: float) -> dict[str, float]:
    url = f"{base}:81/stream"
    start = time.perf_counter()
    first_byte_at: float | None = None
    frames = 0
    total = 0
    previous = b""
    with urllib.request.urlopen(url, timeout=timeout_s) as response:
        while frames < target_frames and (time.perf_counter() - start) < timeout_s:
            chunk = response.read(4096)
            if not chunk:
                break
            if first_byte_at is None:
                first_byte_at = time.perf_counter()
            total += len(chunk)
            data = previous + chunk
            frames += data.count(b"--frame")
            previous = data[-16:]
    elapsed = time.perf_counter() - start
    active_elapsed = max(0.001, elapsed)
    return {
        "frames": frames,
        "bytes": total,
        "elapsed_s": elapsed,
        "first_byte_ms": 0.0 if first_byte_at is None else (first_byte_at - start) * 1000.0,
        "approx_fps": frames / active_elapsed,
        "kbytes_per_s": (total / 1024.0) / active_elapsed,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.4.1")
    parser.add_argument("--snapshots", type=int, default=5)
    parser.add_argument("--frames", type=int, default=80)
    parser.add_argument("--timeout", type=float, default=8.0)
    args = parser.parse_args()

    base = f"http://{args.host}"
    status_before, status_ms = fetch(f"{base}/api/status")
    snapshot_timings = measure_snapshots(base, args.snapshots)
    stream = measure_stream(base, args.frames, args.timeout)
    time.sleep(0.25)
    status_after, status_after_ms = fetch(f"{base}/api/status")

    result = {
        "status_ms": status_ms,
        "status_after_ms": status_after_ms,
        "camera_before": json.loads(status_before.decode("utf-8")).get("camera", {}),
        "snapshots": {
            "count": len(snapshot_timings),
            "avg_ms": sum(t.ms for t in snapshot_timings) / max(1, len(snapshot_timings)),
            "min_ms": min((t.ms for t in snapshot_timings), default=0.0),
            "max_ms": max((t.ms for t in snapshot_timings), default=0.0),
            "avg_bytes": sum(t.bytes_read for t in snapshot_timings) / max(1, len(snapshot_timings)),
        },
        "stream": stream,
        "camera_after": json.loads(status_after.decode("utf-8")).get("camera", {}),
    }
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
