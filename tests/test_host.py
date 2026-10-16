"""Host checks for bounded history, UTF-8, stream framing and safe SDK staging."""
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Core(unittest.TestCase):
    def test_tts_provider_stream_and_cancellation(self):
        idf = os.environ.get("IDF_PATH", "")
        cjson = Path(os.environ.get("CJSON_SOURCE_DIR", str(Path(idf) / "components/json/cJSON")))
        if not (cjson / "cJSON.c").is_file():
            self.skipTest("Set CJSON_SOURCE_DIR to cJSON sources, or IDF_PATH to an IDF checkout with components/json/cJSON")
        with tempfile.TemporaryDirectory() as tmp:
            stub = Path(tmp) / "include"
            names = ("lvgl.h", "esp_err.h", "esp_crt_bundle.h", "esp_heap_caps.h", "esp_http_client.h",
                     "esp_timer.h", "freertos/FreeRTOS.h", "freertos/idf_additions.h", "freertos/queue.h",
                     "freertos/semphr.h", "freertos/stream_buffer.h", "freertos/task.h", "mbedtls/base64.h")
            for name in names:
                file = stub / name
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_text('#include "common.h"\n')
            binary = str(Path(tmp) / "tts")
            # macOS deprecates sprintf used by upstream cJSON. Keep strict
            # warnings on our code; relax only this third-party translation unit.
            third_party = str(Path(tmp) / "cjson.o")
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wno-deprecated-declarations", "-fsanitize=address,undefined",
                            "-I" + str(cjson), "-c", str(cjson / "cJSON.c"), "-o", third_party], check=True)
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                            "-fsanitize=address,undefined", "-I" + str(stub), "-I" + str(ROOT / "tests/stubs"),
                            "-I" + str(ROOT / "src"), "-I" + str(cjson),
                            str(ROOT / "tests/test_tts.c"), str(ROOT / "src/sw_core.c"),
                            third_party, "-o", binary], check=True)
            subprocess.run([binary], check=True)

    def test_core_with_sanitizers(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = str(Path(tmp) / "core")
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                            "-I" + str(ROOT / "src"), str(ROOT / "src/sw_core.c"),
                            str(ROOT / "tests/test_core.c"), "-o", binary], check=True)
            subprocess.run([binary], check=True)

    def test_refuses_to_modify_existing_output(self):
        spec = importlib.util.spec_from_file_location("sw_build", ROOT / "build.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "user-directory"
            out.mkdir()
            keep = out / "keep"
            keep.write_text("user work")
            with self.assertRaises(ValueError):
                module.prepare(Path(tmp) / "not-an-sdk", out)
            self.assertEqual(keep.read_text(), "user work")

    def test_rejects_wrong_source_before_copying(self):
        spec = importlib.util.spec_from_file_location("sw_build", ROOT / "build.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "out"
            with self.assertRaises(ValueError):
                module.prepare(tmp, out)
            self.assertFalse(out.exists())


if __name__ == "__main__":
    unittest.main()
