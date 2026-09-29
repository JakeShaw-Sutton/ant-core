"""Measure complete dashboard status delivery without sending control commands.

Intervals measure telemetry cadence, not physical controller-to-actuator latency.
The Xbox input age is time since the most recent input callback and can be large
when the controller is idle.
"""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import statistics
import time

import websocket

from antcore_live_check import safe_state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    parser.add_argument("--seconds", type=float, default=18)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    sock = websocket.create_connection(args.url.rstrip("/").replace("http://", "ws://", 1) + "/ws",
                                       timeout=8, http_no_proxy=["*"])
    sock.settimeout(3)
    start = time.monotonic()
    samples, errors = [], []
    try:
        while time.monotonic() - start < args.seconds:
            try:
                message = json.loads(sock.recv())
                if message.get("type") != "status":
                    continue
                controller = message.get("controller", {})
                raw = controller.get("xboxRaw", {})
                samples.append({"receivedMs": round((time.monotonic() - start) * 1000),
                                "uptimeMs": message["uptimeMs"], "safe": safe_state(message),
                                "connected": controller.get("xboxConnected"),
                                "source": controller.get("source"), "inputAgeMs": raw.get("ageMs"),
                                "freeHeap": message["system"]["freeHeap"],
                                "minFreeHeap": message["system"]["minFreeHeap"],
                                "webClients": controller.get("webClients"),
                                "axes": raw.get("axes"), "buttons": raw.get("buttons")})
            except websocket.WebSocketTimeoutException:
                errors.append("receive timeout")
            except Exception as exc:
                errors.append(f"{type(exc).__name__}: {exc}")
                break
    finally:
        sock.close()
    intervals = [b["receivedMs"] - a["receivedMs"] for a, b in zip(samples, samples[1:])]
    generations = [b["uptimeMs"] - a["uptimeMs"] for a, b in zip(samples, samples[1:])]
    passed = len(samples) >= 3 and not errors and all(s["safe"] for s in samples) and all(d > 0 for d in generations)
    summary = {"passed": passed, "frames": len(samples),
               "medianIntervalMs": statistics.median(intervals) if intervals else None,
               "p95IntervalMs": sorted(intervals)[min(len(intervals)-1, int(len(intervals)*.95))] if intervals else None,
               "maxIntervalMs": max(intervals) if intervals else None,
               "medianGenerationGapMs": statistics.median(generations) if generations else None,
               "connected": any(s["connected"] for s in samples), "errors": errors}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({"recordedUtc": datetime.now(timezone.utc).isoformat(),
                                      "scope": "Read-only complete status cadence; not physical input latency",
                                      "summary": summary, "samples": samples}, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
