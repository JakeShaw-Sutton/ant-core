"""Small architectural/security contracts; behaviour lives in runtime tests and the UI audit.

Avoid coupling tests to README prose, CSS colours, or where an implementation lives.
"""
from pathlib import Path
import re

import pytest

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def firmware():
    return "\n".join(p.read_text(encoding="utf-8") for p in (ROOT / "src").glob("*.cpp"))


def test_entrypoint_delegates_to_services():
    main = read("src/main.cpp")
    assert "setupApplication();" in main and "loopApplication();" in main
    assert len(main.splitlines()) < 30
    assert not re.search(r'#include\s+[<"].*\.cpp', firmware())
    for service in ["robot", "inputs", "config", "network", "web", "ws", "telemetry",
                    "peripherals", "commands", "console", "log", "indicator", "lifecycle"]:
        assert (ROOT / f"include/antcore_app_{service}.h").is_file()
        assert (ROOT / f"src/antcore_app_{service}.cpp").is_file()


def test_public_service_headers_do_not_expose_mutable_state():
    for header in (ROOT / "include").glob("antcore_app_*.h"):
        assert not re.search(r"^extern .*State", header.read_text(), re.MULTILINE)


def test_all_route_groups_are_registered():
    entry = read("src/antcore_app_web.cpp")
    for source in (ROOT / "src").glob("antcore_routes_*.cpp"):
        names = re.findall(r"^void (register\w+Routes)\(\)", source.read_text(), re.MULTILINE)
        assert names and all(f"{name}();" in entry for name in names)


@pytest.mark.parametrize("path", [
    "/api/arm", "/api/weapon/arm", "/api/weapon/disarm", "/api/pit",
    "/api/config/reset", "/api/profiles/save", "/api/profiles/load", "/api/profiles/delete",
    "/api/packs/save", "/api/packs/delete", "/api/test/live-output", "/api/test/motor",
    "/api/test/servo", "/api/ble/forget", "/api/blackbox/clear", "/api/control/release",
])
def test_mutating_http_routes_authenticate_before_execution(path):
    code = firmware()
    start = code.index(f'"{path}"')
    callback = code[start:code.index("return;", start) + len("return;")]
    assert "requireAuth(request)" in callback


def test_ota_authentication_is_present_in_each_upload_phase():
    code = read("src/antcore_routes_ota.cpp")
    assert code.count("tokenIsValid(") == 4  # start and completion, firmware and filesystem
    assert "OtaBeginFirmware" in code and "OtaBeginFilesystem" in code


def test_public_json_is_built_without_secrets():
    code = read("src/antcore_config.cpp")
    for key, field in [("adminPin", "config.security.adminPin"),
                       ("staPassword", "config.wifi.staPassword"), ("apPassword", "config.apPassword")]:
        assert f'["{key}"] = includeSecrets ? {field} : ""' in code
    assert "configToJson(config, false)" in read("src/antcore_app_telemetry.cpp")
    assert "configToJson(doc, false)" in read("src/antcore_app_console.cpp")


def test_snapshot_building_stays_off_network_callbacks():
    for filename in ["src/antcore_app_web.cpp", "src/antcore_app_ws.cpp", "src/antcore_routes_configuration.cpp"]:
        code = read(filename)
        assert "buildStatusJson(" not in code
        assert "cached" in code
    assert "refreshTelemetry();" in read("src/antcore_app_lifecycle.cpp")
    # The application task also lends its scratch pool to config operations;
    # callbacks must never clear it while the loop holds JSON variants.
    for source in list((ROOT / "src").glob("antcore_routes_*.cpp")) + [
        ROOT / "src/antcore_app_web.cpp", ROOT / "src/antcore_app_ws.cpp",
    ]:
        assert "borrowLoopJsonDocument(" not in source.read_text(encoding="utf-8")


def test_network_starts_before_optional_peripherals():
    code = read("src/antcore_app_lifecycle.cpp")
    setup = code[code.index("void setupApplication()") : code.index("void loopApplication()")]
    assert setup.index("initPins();") < setup.index("initWiFiAp();") < setup.index("server.begin();")
    assert "initBle();" not in setup and "initCamera();" not in setup
    assert "serviceOptionalPeripherals(now);" in code
    network = read("src/antcore_app_network.cpp")
    assert "WiFi.setSleep(false)" not in network


def test_every_persistent_json_writer_uses_the_shared_commit_helper():
    for name in ["antcore_app_config", "antcore_profiles", "antcore_packs"]:
        assert "antcore_storage::writeTextAtomic" in read(f"src/{name}.cpp")
    helper = read("src/antcore_file_store.cpp")
    assert "filesystem.remove(path)" not in helper
    assert "written == body.length()" in helper


def test_developer_docs_and_secret_template_are_available():
    for name in ["pinout", "wiring-and-safety", "controller-setup", "ota-and-recovery", "battery-calibration", "architecture"]:
        assert (ROOT / f"docs/{name}.md").is_file()
    assert "include/antcore_local_secrets.h" in read(".gitignore")
    assert "ANTCORE_DEFAULT_STA_ENABLED false" in read("include/antcore_local_secrets.example.h")


def test_no_audio_assets_in_control_ui():
    code = "\n".join(read(f"data/{name}") for name in ["index.html", "app.js", "app.css"])
    assert all(token not in code for token in ["<audio", "new Audio", "AudioContext", ".mp3", ".wav", ".ogg"])
