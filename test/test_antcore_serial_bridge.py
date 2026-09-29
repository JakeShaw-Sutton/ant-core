import json
import unittest

from tools.antcore_serial_bridge import extract_prefixed_json


class AntCoreSerialBridgeTests(unittest.TestCase):
    def test_extract_status_json(self):
        payload = {"armed": False, "battery": {"packVolts": 7.4}}
        line = "STATUS " + json.dumps(payload)
        self.assertEqual(extract_prefixed_json(line, "STATUS"), payload)

    def test_ignores_other_prefix(self):
        self.assertIsNone(extract_prefixed_json("LOG 123 INFO boot", "STATUS"))

    def test_invalid_json_raises(self):
        with self.assertRaises(json.JSONDecodeError):
            extract_prefixed_json("CONFIG {not-json}", "CONFIG")


if __name__ == "__main__":
    unittest.main()
