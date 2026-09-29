"""USB-only C3 HTTP/WebSocket checks; never arm or enable live outputs.

Run with --url http://<board-ip> --report-dir reports/<run>.
Optional --expected-ssid verifies the configured and connected station name.
Optional --exercise-storage creates and removes uniquely named test records,
restoring the original active-profile name. Credentials are loaded locally and
are never written to the evidence report. Close driving browser tabs first.
"""

from __future__ import annotations

import argparse
from collections import deque
import hashlib
import json
import math
import queue
import re
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import websocket


ROOT = Path(__file__).resolve().parents[1]
SECRET_KEYS = {"token", "sessionToken", "adminPin", "apPassword", "staPassword", "pin"}


def local_defaults() -> dict[str, str]:
    values = {"ANTCORE_DEFAULT_ADMIN_PIN": "antcore"}
    source = ROOT / "include" / "antcore_local_secrets.h"
    if source.exists():
        for name, literal in re.findall(
            r'^\s*#define\s+(ANTCORE_DEFAULT_\w+)\s+("(?:\\.|[^"\\])*")',
            source.read_text(encoding="utf-8"), re.MULTILINE,
        ):
            values[name] = json.loads(literal)
    return values


def safe_state(status: dict[str, Any]) -> bool:
    motors = status.get("motors", [])
    return (
        status.get("armed") is False
        and status.get("liveOutputEnabled") is False
        and status.get("weapon", {}).get("armed") is False
        and len(motors) == 3
        and all(all(m.get(k) == 0 for k in ("target", "output", "pwmA", "pwmB")) for m in motors)
    )


class WsPeer:
    """Continuously drain a live peer; preserve failures and fresh frame times."""

    def __init__(self, url: str):
        self.socket = websocket.create_connection(url, timeout=5, http_no_proxy=["*"], enable_multithread=True)
        self.socket.settimeout(1)
        self.stop = threading.Event()
        self.messages: queue.Queue = queue.Queue(maxsize=256)
        self.statuses: deque = deque(maxlen=128)
        self.errors: list[str] = []
        self.lock = threading.Lock()
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.reader.start()

    def _read(self) -> None:
        while not self.stop.is_set():
            try:
                raw = self.socket.recv()
                if not raw:
                    raise RuntimeError("WebSocket closed before the check completed")
                frame = json.loads(raw)
                received = time.monotonic()
                if frame.get("type") == "status":
                    if not safe_state(frame):
                        raise RuntimeError("WebSocket telemetry is not disarmed with zero motor outputs")
                    with self.lock:
                        self.statuses.append((received, frame))
                self.messages.put_nowait((received, frame))
            except websocket.WebSocketTimeoutException:
                continue
            except Exception as exc:
                if not self.stop.is_set():
                    with self.lock:
                        self.errors.append(f"{type(exc).__name__}: {exc}")
                return

    def send(self, frame: dict[str, Any]) -> None:
        self.socket.send(json.dumps(frame))

    def wait(self, predicate, timeout: float = 4, after: float = 0) -> dict[str, Any]:
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            with self.lock:
                if self.errors:
                    raise RuntimeError(self.errors[0])
            try:
                received, frame = self.messages.get(timeout=min(0.1, max(0.001, until - time.monotonic())))
            except queue.Empty:
                continue
            if received >= after and predicate(frame):
                return frame
        raise RuntimeError("Expected fresh WebSocket response was not received")

    def samples(self, after: float) -> list[dict[str, Any]]:
        with self.lock:
            return [frame for received, frame in self.statuses if received >= after]

    def close(self) -> None:
        self.stop.set()
        self.socket.close(timeout=1)
        self.reader.join(timeout=1.5)


class Check:
    def __init__(self, url: str, report_dir: Path):
        self.url = url.rstrip("/")
        parsed = urllib.parse.urlparse(self.url)
        if parsed.scheme not in ("http", "https") or not parsed.hostname or parsed.username or parsed.query:
            raise ValueError("Use a plain HTTP(S) board URL without credentials or query parameters")
        self.report_dir = report_dir
        self.defaults = local_defaults()
        self.token = ""
        self.secrets = {v for k, v in self.defaults.items() if v and ("PASSWORD" in k or "PIN" in k)}
        self.http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        self.report: dict[str, Any] = {
            "startedUtc": datetime.now(timezone.utc).isoformat(), "url": self.url,
            "scope": "C3 USB-only; no arming, live output tests, resets, or OTA",
            "checks": [], "requests": [], "telemetry": [],
        }

    def record(self, name: str, passed: bool, evidence: Any = None) -> bool:
        self.report["checks"].append({"name": name, "passed": bool(passed), "evidence": evidence})
        return bool(passed)

    def require(self, name: str, passed: bool, evidence: Any = None) -> None:
        if not self.record(name, passed, evidence):
            raise RuntimeError(name)

    def redact(self, value: Any) -> Any:
        if isinstance(value, bytes):
            return self.redact(value.decode("utf-8", errors="replace"))
        if isinstance(value, dict):
            return {k: ("<redacted>" if k in SECRET_KEYS and v else self.redact(v)) for k, v in value.items()}
        if isinstance(value, list):
            return [self.redact(v) for v in value]
        if isinstance(value, str):
            for secret in sorted(self.secrets, key=len, reverse=True):
                value = value.replace(secret, "<redacted>")
        return value

    def request(self, path: str, method: str = "GET", body: Any = None, auth: bool = True) -> tuple[int, Any]:
        headers = {}
        if auth and self.token:
            headers["X-AntCore-Token"] = self.token
        data = None if body is None else json.dumps(body).encode()
        if data is not None:
            headers["Content-Type"] = "application/json"
        request = urllib.request.Request(self.url + path, data=data, headers=headers, method=method)
        started = time.monotonic()
        try:
            response = self.http.open(request, timeout=10)
        except urllib.error.HTTPError as exc:
            response = exc
        with response:
            raw = response.read()
            content_type = response.headers.get("Content-Type", "")
            result = json.loads(raw) if "application/json" in content_type else raw
            code = response.code
        self.report["requests"].append({"method": method, "path": path, "status": code,
                                       "bytes": len(raw), "elapsedMs": round((time.monotonic() - started) * 1000)})
        return code, result

    def get_json(self, path: str) -> dict[str, Any]:
        code, body = self.request(path)
        self.require(f"GET {path}", code == 200 and isinstance(body, dict), {"status": code})
        return body

    def wait_status(self, predicate, timeout: float = 30) -> dict[str, Any]:
        until = time.monotonic() + timeout
        last: dict[str, Any] = {}
        while time.monotonic() < until:
            try:
                code, last = self.request("/api/status")
                if code == 200 and isinstance(last, dict) and predicate(last):
                    return last
            except (OSError, ValueError):
                pass
            time.sleep(0.25)
        raise RuntimeError("Timed out waiting for expected board state")

    def snapshot(self, status: dict[str, Any]) -> None:
        self.report["telemetry"].append({k: status.get(k) for k in (
            "uptimeMs", "board", "firmwareVersion", "filesystemVersion", "armed", "liveOutputEnabled",
            "system", "wifi", "battery", "imu", "camera", "motors", "servos", "safety",
        )})

    def run(self, exercise_storage: bool, expected_ssid: str | None = None) -> None:
        initial = self.get_json("/api/status")
        self.require("Initially disarmed with all motor outputs zero", safe_state(initial))
        self.require("No existing browser driver or Xbox controller", not initial.get("controller", {}).get("webDriverLocked")
                     and not initial.get("controller", {}).get("xboxConnected"))
        self.snapshot(initial)
        self.record("C3 board", initial.get("board") == "XIAO ESP32-C3", initial.get("board"))
        firmware_header = (ROOT / "include" / "antcore_firmware_config.h").read_text(encoding="utf-8")
        for key, symbol in (("firmwareVersion", "FIRMWARE_VERSION"), ("filesystemVersion", "WEB_ASSET_VERSION")):
            match = re.search(r"\b" + symbol + r'\s*=\s*"([^"]+)"', firmware_header)
            self.record(f"{key} matches source", bool(match) and initial.get(key) == match.group(1), initial.get(key))
        self.record("Station connected", initial.get("wifi", {}).get("sta", {}).get("connected") is True)
        self.record("Unauthenticated status redacts station SSID", initial.get("wifi", {}).get("sta", {}).get("ssid") == "")
        self.record("Battery sensing unsupported without false low-voltage fault",
                    initial.get("battery", {}).get("supported") is False and initial["battery"].get("enabled") is False
                    and initial["battery"].get("critical") is False)
        self.record("Camera unsupported", initial.get("camera", {}).get("supported") is False
                    and initial["camera"].get("ready") is False)
        self.record("Missing IMU handled", initial.get("imu", {}).get("present") is False)
        self.record("Two servos at configured failsafe", len(initial.get("servos", [])) == 2
                    and all(s.get("override") in ("failsafe", "detached") and
                            (not s.get("attached") or s.get("us") == s.get("failsafeUs")) for s in initial.get("servos", [])))
        for route, filename in (("/", "index.html"), ("/index.html", "index.html"), ("/app.js", "app.js"), ("/app.css", "app.css")):
            code, body = self.request(route)
            expected = hashlib.sha256((ROOT / "data" / filename).read_bytes()).hexdigest()
            actual = hashlib.sha256(body).hexdigest() if isinstance(body, bytes) else None
            self.record(f"Asset {route} matches local source", code == 200 and actual == expected,
                        {"sha256": actual, "expectedSha256": expected})
        config = self.get_json("/api/config")
        self.record("Station enabled in config", config.get("wifi", {}).get("staEnabled") is True)
        if expected_ssid is not None:
            self.record("Configured station matches expected SSID", config.get("wifi", {}).get("staSsid") == expected_ssid)
        exported = self.get_json("/api/config/export")
        validation = self.get_json("/api/config/validate")
        self.record("Config validates", validation.get("valid") is True, validation)
        self.record("Export matches public config", config == exported)
        self.record("Secrets redacted from config and export", all(
            item.get("apPassword") == "" and item.get("security", {}).get("adminPin") == ""
            and item.get("wifi", {}).get("staPassword") == "" for item in (config, exported)))
        self.get_json("/api/profiles")
        self.get_json("/api/packs")
        code, body = self.request("/api/logs", auth=False)
        self.require("Protected logs reject unauthenticated access", code == 401, {"status": code})
        code, body = self.request("/api/auth/login", "POST", {"pin": self.defaults["ANTCORE_DEFAULT_ADMIN_PIN"]}, auth=False)
        self.require("Admin login", code == 200 and isinstance(body, dict) and bool(body.get("token")), {"status": code})
        self.token = body["token"]
        self.secrets.add(self.token)
        authenticated_status = self.get_json("/api/status")
        self.record("Authenticated status confirms station connection",
                    authenticated_status.get("wifi", {}).get("sta", {}).get("connected") is True)
        if expected_ssid is not None:
            self.record("Connected station matches expected SSID",
                        authenticated_status.get("wifi", {}).get("sta", {}).get("ssid") == expected_ssid)
        self.snapshot(authenticated_status)
        logs = self.get_json("/api/logs")
        self.record("Log storage available", logs.get("blackboxAvailable") is True,
                    {"lines": logs.get("logs"), "blackboxBytes": logs.get("blackboxBytes")})
        code, blackbox = self.request("/api/blackbox")
        self.record("Blackbox export", code == 200 and isinstance(blackbox, bytes)
                    and (not logs.get("blackboxBytes") or len(blackbox) > 0),
                    {"status": code, "bytes": len(blackbox)})
        for route in ("/stream", "/snapshot.jpg"):
            code, _ = self.request(route)
            self.record(f"Unsupported camera {route} fails cleanly", code == 503, {"status": code})
        self.websocket_checks()
        if exercise_storage:
            # A real dashboard keeps a telemetry socket open while saving.
            # Config saves deliberately restart STA, so reconnect afterwards
            # instead of treating the expected Wi-Fi disconnect as a failure.
            ws_url = self.url.replace("https://", "wss://", 1).replace("http://", "ws://", 1) + "/ws"
            observer = WsPeer(ws_url)
            try:
                observer.wait(lambda frame: frame.get("type") == "status")
                self.storage_checks(config)
                self.record("Storage checks started with a live telemetry observer", True)
            finally:
                observer.close()
            observer = WsPeer(ws_url)
            try:
                first = observer.wait(lambda frame: frame.get("type") == "status")
                observer.wait(lambda frame: frame.get("type") == "status"
                              and frame.get("uptimeMs", 0) > first["uptimeMs"])
                self.record("Live telemetry reconnects after configuration saves", True)
            finally:
                observer.close()
        stable = self.wait_status(lambda s: s.get("system", {}).get("bootStable") is True, 45)
        self.snapshot(stable)
        start_uptime = stable["uptimeMs"]
        heap_samples = []
        for _ in range(6):
            time.sleep(1)
            status = self.get_json("/api/status")
            heap_samples.append({"uptimeMs": status.get("uptimeMs"), "freeHeap": status.get("system", {}).get("freeHeap"),
                                 "minFreeHeap": status.get("system", {}).get("minFreeHeap")})
            self.require("Sample remains disarmed with zero motor outputs", safe_state(status))
        self.record("Uptime advances without reboot", all(b["uptimeMs"] > a["uptimeMs"]
                    for a, b in zip([{"uptimeMs": start_uptime}] + heap_samples, heap_samples)), heap_samples)
        self.record("Heap available throughout sample", all((s["freeHeap"] or 0) > 0 for s in heap_samples))
        self.snapshot(status)

    def websocket_checks(self) -> None:
        url = self.url.replace("https://", "wss://", 1).replace("http://", "ws://", 1) + "/ws"
        a = b = None
        heartbeat = None
        heartbeat_stop = threading.Event()
        sender_errors: list[str] = []
        sent = [0]
        try:
            a = WsPeer(url)
            status = a.wait(lambda f: f.get("type") == "status")
            self.record("WebSocket telemetry received", status.get("type") == "status")
            ident = "bench-" + uuid.uuid4().hex[:10]
            a.send({"type": "claimControl", "token": self.token, "clientId": ident})
            claim = a.wait(lambda f: f.get("type") == "controlClaim")
            self.require("WebSocket driver claim", claim.get("ok") is True and claim.get("webDriverId") == ident, claim)
            neutral = {"type": "control", "token": self.token, "clientId": ident,
                       "leftX": 0, "leftY": 0, "rightX": 0, "rightY": 0, "leftTrigger": 0, "rightTrigger": 0, "buttons": {}}

            def send_neutral() -> None:
                # Maintain freshness during the second connection/handshake;
                # receiving status must never determine the input cadence.
                while not heartbeat_stop.is_set():
                    try:
                        a.send(neutral)
                        sent[0] += 1
                    except Exception as exc:
                        sender_errors.append(f"{type(exc).__name__}: {exc}")
                        return
                    heartbeat_stop.wait(0.05)

            heartbeat = threading.Thread(target=send_neutral, daemon=True)
            heartbeat.start()
            b = WsPeer(url)
            b.wait(lambda f: f.get("type") == "status")
            b.send({"type": "claimControl", "token": self.token, "clientId": ident + "-other"})
            conflict = b.wait(lambda f: f.get("type") == "controlClaim")
            self.record("Second driver cannot steal control", conflict.get("ok") is False
                        and conflict.get("reason") == "driver locked" and conflict.get("webDriverId") == ident, conflict)
            sample_start = time.monotonic()
            # Require fresh broadcast status, not the initial on-connect frame
            # or stale frames received before neutral transmission began.
            a.wait(lambda f: f.get("type") == "status" and f.get("controller", {}).get("source") == "web"
                   and f.get("controller", {}).get("webDriverId") == ident, after=sample_start)
            until = sample_start + 2.5
            while time.monotonic() < until:
                time.sleep(min(0.1, max(0, until - time.monotonic())))
            for label, peer in (("owner", a), ("observer", b)):
                statuses = peer.samples(sample_start)
                uptimes = [s.get("uptimeMs", 0) for s in statuses]
                self.record(f"Repeated fresh telemetry reaches {label}", len(statuses) >= 3
                            and all(new > old for old, new in zip(uptimes, uptimes[1:])),
                            {"frames": len(statuses), "uptimeMs": uptimes, "readerErrors": peer.errors})
            self.record("Neutral web input recognized", any(s.get("controller", {}).get("source") == "web"
                        for s in a.samples(sample_start)), {"neutralPackets": sent[0], "senderErrors": sender_errors})
            self.require("Neutral sender and both readers remain healthy", not sender_errors and not a.errors and not b.errors)
            heartbeat_stop.set()
            heartbeat.join(timeout=1.5)
            release_start = time.monotonic()
            last_uptime = a.samples(0)[-1]["uptimeMs"]
            a.send({"type": "releaseControl", "token": self.token})
            released = a.wait(lambda f: f.get("type") == "status" and f.get("uptimeMs", 0) > last_uptime
                              and not f.get("controller", {}).get("webDriverLocked")
                              and f.get("controller", {}).get("source") == "none", after=release_start)
            self.record("Web driver released in fresh telemetry", released["controller"].get("source") == "none")
            # A closing observer must not stop fresh broadcasts to the owner.
            b.close()
            b = None
            after_close = time.monotonic()
            last_uptime = released["uptimeMs"]
            a.wait(lambda f: f.get("type") == "status" and f.get("uptimeMs", 0) > last_uptime, after=after_close)
            self.record("Healthy peer still receives telemetry after observer disconnect", True)
        finally:
            heartbeat_stop.set()
            if heartbeat is not None:
                heartbeat.join(timeout=1.5)
            for label, peer in (("owner", a), ("observer", b)):
                if peer is None:
                    continue
                self.report.setdefault("websocketEvidence", {})[label] = {
                    "statusFrames": len(peer.samples(0)), "readerErrors": peer.errors,
                    "neutralPackets": sent[0], "senderErrors": sender_errors,
                }
                try:
                    peer.close()
                except Exception as exc:
                    self.record(f"Close {label} WebSocket", False, str(exc))
        self.require("Outputs remain safe after WebSocket disconnect", safe_state(self.wait_status(
            lambda s: not s.get("controller", {}).get("webDriverLocked"))))

    def storage_checks(self, original_config: dict[str, Any]) -> None:
        profiles_before = self.get_json("/api/profiles")
        packs_before = self.get_json("/api/packs")
        self.require("Free storage slots for temporary records", len(profiles_before.get("profiles", [])) < 8
                     and len(packs_before.get("packs", [])) < 10)
        name = "BenchCheck-" + uuid.uuid4().hex[:8]
        pack_id = name.lower()
        # Mark operations before sending: cleanup is needed even if a response is lost.
        profile_attempted = pack_attempted = False
        try:
            code, body = self.request("/api/config", "PUT", original_config)
            self.require("Full dashboard configuration saves", code == 200 and body.get("ok") is True, body)
            time.sleep(1)
            self.wait_status(lambda s: s.get("wifi", {}).get("sta", {}).get("connected") is True)
            profile_attempted = True
            code, body = self.request("/api/profiles/save", "POST", {"name": name})
            self.require("Temporary profile saves", code == 200 and body.get("ok") is True, body)
            profiles = self.get_json("/api/profiles")
            self.record("Saved profile listed", any(p.get("name") == name for p in profiles.get("profiles", [])))
            code, body = self.request("/api/profiles/load", "POST", {"name": name})
            self.require("Temporary profile loads", code == 200 and body.get("ok") is True, body)
            time.sleep(1)
            self.wait_status(lambda s: s.get("activeProfile") == name
                             and s.get("wifi", {}).get("sta", {}).get("connected") is True)
            pack_attempted = True
            pack = {"id": pack_id, "name": name, "chargeVoltage": 8.4, "weak": False, "cycles": 0,
                    "notes": "Temporary USB dashboard check"}
            code, body = self.request("/api/packs/save", "POST", pack)
            self.require("Temporary pack saves", code == 200 and body.get("ok") is True, body)
            packs = self.get_json("/api/packs")
            saved_pack = next((p for p in packs.get("packs", []) if p.get("id") == pack_id), None)
            # Firmware stores volts as float32; compare its numeric value, not
            # the binary representation of Python's float64 literal.
            pack_matches = (isinstance(saved_pack, dict) and set(saved_pack) == set(pack)
                            and all(saved_pack.get(k) == v for k, v in pack.items() if k != "chargeVoltage")
                            and math.isclose(saved_pack["chargeVoltage"], pack["chargeVoltage"], abs_tol=1e-5))
            self.record("Saved pack round-trips", pack_matches, {"expected": pack, "actual": saved_pack})
        finally:
            if pack_attempted:
                try:
                    code, body = self.request("/api/packs/delete", "POST", {"id": pack_id})
                    self.record("Temporary pack cleaned up", code in (200, 404), {"status": code})
                except Exception as exc:
                    self.record("Temporary pack cleaned up", False, str(exc))
            if profile_attempted:
                # Saving a profile also changes activeProfile; a partial config update
                # restores only that field and retains every stored secret/setting.
                try:
                    code, body = self.request("/api/config", "PUT", {"activeProfile": original_config["activeProfile"]})
                    self.record("Original active profile restore accepted", code == 200 and body.get("ok") is True)
                    # Config saves schedule a station restart 500 ms later.
                    time.sleep(1)
                    self.wait_status(lambda s: s.get("wifi", {}).get("sta", {}).get("connected") is True)
                except Exception as exc:
                    self.record("Original active profile restore accepted", False, str(exc))
                try:
                    code, body = self.request("/api/profiles/delete", "POST", {"name": name})
                    self.record("Temporary profile cleaned up", code in (200, 404), {"status": code})
                except Exception as exc:
                    self.record("Temporary profile cleaned up", False, str(exc))
        # Config cache refreshes with telemetry, so wait for its profile name first.
        self.wait_status(lambda s: s.get("activeProfile") == original_config["activeProfile"])
        self.record("Original complete public config preserved", self.get_json("/api/config") == original_config)
        after_profiles = self.get_json("/api/profiles")
        self.record("Original profile list preserved", sorted(after_profiles.get("profiles", []), key=lambda p: p["name"])
                    == sorted(profiles_before.get("profiles", []), key=lambda p: p["name"]))
        self.record("Original pack list preserved", self.get_json("/api/packs") == packs_before)

    def finish(self) -> Path:
        if self.token:
            try:
                code, _ = self.request("/api/auth/logout", "POST")
                self.record("Test session logged out", code == 200)
            except Exception as exc:
                self.record("Test session logged out", False, str(exc))
        self.report["finishedUtc"] = datetime.now(timezone.utc).isoformat()
        self.report["passed"] = all(c["passed"] for c in self.report["checks"])
        self.report_dir.mkdir(parents=True, exist_ok=True)
        output = self.report_dir / "antcore-live-check.json"
        output.write_text(json.dumps(self.redact(self.report), indent=2) + "\n", encoding="utf-8")
        return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    parser.add_argument("--report-dir", type=Path, required=True)
    parser.add_argument("--expected-ssid", help="Check the configured and connected station SSID (optional)")
    parser.add_argument("--exercise-storage", action="store_true")
    args = parser.parse_args()
    check = Check(args.url, args.report_dir)
    try:
        check.run(args.exercise_storage, args.expected_ssid)
    except Exception as exc:
        check.record("Run completed", False, f"{type(exc).__name__}: {exc}")
    output = check.finish()
    failed = [item["name"] for item in check.report["checks"] if not item["passed"]]
    print(json.dumps({"passed": not failed, "checks": len(check.report["checks"]), "failed": failed,
                      "report": str(output.resolve())}, indent=2))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
