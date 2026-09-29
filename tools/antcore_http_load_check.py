"""Read-only concurrent dashboard transfers with live WebSocket telemetry."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import threading
import time
import urllib.request

import websocket

from antcore_live_check import ROOT, safe_state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    base = args.url.rstrip("/")
    results, samples, errors = [], [], []
    stop = threading.Event()
    sock = websocket.create_connection(base.replace("http://", "ws://", 1) + "/ws",
                                       timeout=10, http_no_proxy=["*"])
    sock.settimeout(1)

    def reader():
        while not stop.is_set():
            try:
                frame = json.loads(sock.recv())
                if frame.get("type") == "status":
                    samples.append({"uptimeMs": frame["uptimeMs"], "safe": safe_state(frame),
                                    "freeHeap": frame["system"]["freeHeap"],
                                    "maxAllocHeap": frame["system"]["maxAllocHeap"],
                                    "webClients": frame.get("controller", {}).get("webClients"),
                                    "xboxConnected": frame.get("controller", {}).get("xboxConnected")})
                elif frame.get("ok") is False:
                    errors.append(frame.get("message", "telemetry error"))
            except websocket.WebSocketTimeoutException:
                continue
            except Exception as exc:
                if not stop.is_set():
                    errors.append(f"{type(exc).__name__}: {exc}")
                return

    def request(path):
        start = time.monotonic()
        item = {"path": path}
        try:
            opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
            with opener.open(base + path, timeout=10) as response:
                raw = response.read()
                item.update(status=response.status, bytes=len(raw))
            if path == "/api/status":
                item["passed"] = safe_state(json.loads(raw))
            else:
                source = ROOT / "data" / ("index.html" if path == "/" else path[1:])
                item["sha256"] = hashlib.sha256(raw).hexdigest()
                item["passed"] = item["sha256"] == hashlib.sha256(source.read_bytes()).hexdigest()
        except Exception as exc:
            item.update(passed=False, error=f"{type(exc).__name__}: {exc}")
        item["elapsedMs"] = round((time.monotonic() - start) * 1000)
        return item

    worker = threading.Thread(target=reader, daemon=True)
    worker.start()
    try:
        for round_number in range(3):
            with ThreadPoolExecutor(max_workers=4) as pool:
                batch = list(pool.map(request, ("/", "/app.css", "/app.js", "/api/status")))
            results.append({"round": round_number + 1, "requests": batch})
            if not all(r["passed"] for r in batch) or errors:
                break
        # Require the live feed to keep advancing after the transfer burst.
        tail_start = len(samples)
        stop.wait(5)
        tail = samples[tail_start:]
    finally:
        stop.set()
        worker.join(timeout=2)
        sock.close()
    passed = (len(results) == 3 and all(r["passed"] for batch in results for r in batch["requests"])
              and not errors and len(tail) >= 3 and all(s["safe"] for s in samples)
              and all(b["uptimeMs"] > a["uptimeMs"] for a, b in zip(tail, tail[1:])))
    report = {"recordedUtc": datetime.now(timezone.utc).isoformat(), "passed": passed,
              "scope": "Read-only HTTP transfers and WebSocket observer; no control packets",
              "rounds": results, "samples": samples, "errors": errors}
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": passed, "rounds": len(results), "statusFrames": len(samples),
                      "errors": errors, "report": str(args.report)}))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
