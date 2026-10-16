import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("sw_package", ROOT / "package.py")
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def table(nvs_offset=0x9000):
    entries = [("nvs", 1, 2, nvs_offset, 0x5000), ("phy_init", 1, 1, 0xe000, 0x1000),
               ("factory", 0, 0, 0x10000, 0xff0000)]
    data = b"".join(struct.pack("<HBBII16sI", 0x50aa, kind, sub, start, size, name.encode(), 0)
                    for name, kind, sub, start, size in entries)
    return data + b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(data).digest() + b"\xff" * 32


class Package(unittest.TestCase):
    def test_preserves_layout_and_detects_table_corruption(self):
        self.assertEqual(package.partition_table(table())[0]["offset"], 0x9000)
        with self.assertRaises(ValueError):
            package.partition_table(table(0xa000))
        corrupt = bytearray(table()); corrupt[12] ^= 1
        with self.assertRaises(ValueError):
            package.partition_table(corrupt)

    def test_archive_verification_binds_app_and_elf(self):
        with tempfile.TemporaryDirectory() as tmp:
            build = Path(tmp) / "build"; build.mkdir()
            (build / "config").mkdir()
            config = {"IDF_TARGET": "esp32s3", "ESPTOOLPY_FLASHSIZE": "16MB",
                      "MUSE_BOARD_M5STACK_STOPWATCH": True, "PARTITION_TABLE_OFFSET": 0x8000}
            (build / "config/sdkconfig.json").write_text(json.dumps(config))
            (build / "flasher_args.json").write_text(json.dumps({"flash_files": {
                "0x0": "bootloader.bin", "0x8000": "partition-table.bin", "0x10000": "muse-gadget.bin"}}))
            elf = b"test fixture ELF identity"
            (build / "muse-gadget.elf").write_bytes(elf)
            app = bytearray(256); app[0] = 0xe9; struct.pack_into("<H", app, 12, 9)
            struct.pack_into("<I", app, 32, 0xabcd5432); app[176:208] = hashlib.sha256(elf).digest()
            (build / "muse-gadget.bin").write_bytes(app)
            (build / "bootloader.bin").write_bytes(app[:24])
            (build / "partition-table.bin").write_bytes(table())
            (build / "muse-gadget.map").write_text("test map")
            (build / "project_description.json").write_text("{}")
            out = package.create(build, Path(tmp) / "archives")
            package.verify(out)
            (out / "muse-gadget.bin").write_bytes(b"corrupted")
            with self.assertRaises(ValueError):
                package.verify(out)
            (build / "muse-gadget.elf").write_bytes(b"different ELF")
            with self.assertRaises(ValueError):
                package.create(build, Path(tmp) / "other-archives")


if __name__ == "__main__":
    unittest.main()
