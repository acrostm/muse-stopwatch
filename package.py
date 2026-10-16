#!/usr/bin/env python3
"""Validate a StopWatch build and retain an immutable segmented-flash bundle."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

FLASH_BYTES = 16 * 1024 * 1024
PROTECTED = (0x9000, 0x10000)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def partition_table(raw):
    entries = []
    for offset in range(0, len(raw), 32):
        record = raw[offset:offset + 32]
        if record[:2] == b"\xeb\xeb":
            if hashlib.md5(raw[:offset]).digest() != record[16:32]:
                raise ValueError("Partition-table MD5 mismatch")
            break
        if record[:2] == b"\xff\xff":
            raise ValueError("Partition table is missing its MD5 record")
        if len(record) != 32 or record[:2] != b"\xaa\x50":
            raise ValueError("Invalid partition-table record")
        _, kind, sub, start, size, name, flags = struct.unpack("<HBBII16sI", record)
        entries.append(dict(name=name.rstrip(b"\0").decode(), type=kind, subtype=sub,
                            offset=start, size=size, flags=flags))
    else:
        raise ValueError("Partition-table MD5 not found")
    expected = [("nvs", 1, 2, 0x9000, 0x5000), ("phy_init", 1, 1, 0xe000, 0x1000),
                ("factory", 0, 0, 0x10000, 0xff0000)]
    actual = [(e["name"], e["type"], e["subtype"], e["offset"], e["size"]) for e in entries]
    if actual != expected:
        raise ValueError("Unexpected layout; refusing a flash that could move existing NVS")
    return entries


def create(build, archive_root):
    build, archive_root = Path(build).resolve(), Path(archive_root).resolve()
    config = json.loads((build / "config/sdkconfig.json").read_text())
    if config.get("IDF_TARGET") != "esp32s3" or config.get("ESPTOOLPY_FLASHSIZE") != "16MB":
        raise ValueError("Expected ESP32-S3 with 16 MB flash")
    if not config.get("MUSE_BOARD_M5STACK_STOPWATCH") or config.get("PARTITION_TABLE_OFFSET") != 0x8000:
        raise ValueError("Wrong board or partition-table offset")
    if config.get("SECURE_BOOT") or config.get("FLASH_ENCRYPTION_ENABLED") or config.get("HOMEHUB_OTA_ENABLED"):
        raise ValueError("Unexpected Secure Boot, encryption or OTA configuration")
    flash = json.loads((build / "flasher_args.json").read_text())
    images = []
    for offset, name in sorted(flash["flash_files"].items(), key=lambda item: int(item[0], 0)):
        start = int(offset, 0)
        source = (build / name).resolve()
        if not source.is_relative_to(build):
            raise ValueError("Flash image is outside the build")
        data = source.read_bytes()
        end = start + len(data)
        # esptool erases whole sectors, so validate the rounded write range too.
        erase_end = (end + 4095) & ~4095
        if not data or start % 4096 or erase_end > FLASH_BYTES or (start < PROTECTED[1] and erase_end > PROTECTED[0]):
            raise ValueError(f"Image overlaps protected data or device bounds: {name}")
        if images and start < images[-1]["erase_end"]:
            raise ValueError("Overlapping image erase ranges")
        if start in (0, 0x10000) and (len(data) < 24 or data[0] != 0xe9 or struct.unpack_from("<H", data, 12)[0] != 9):
            raise ValueError("Not an ESP32-S3 application/bootloader image")
        images.append(dict(offset=start, erase_end=erase_end, source=source, data=data,
                           file=Path(name).name, bytes=len(data), sha256=digest(data)))
    if [i["offset"] for i in images] != [0, 0x8000, 0x10000]:
        raise ValueError("Expected only bootloader, table and factory app flash images")
    partitions = partition_table(images[1]["data"])
    if images[2]["bytes"] > partitions[-1]["size"]:
        raise ValueError("Application exceeds the factory partition")
    elf = (build / "muse-gadget.elf").read_bytes()
    app = images[2]["data"]
    if len(app) < 208 or struct.unpack_from("<I", app, 32)[0] != 0xabcd5432 or app[176:208] != hashlib.sha256(elf).digest():
        raise ValueError("Application descriptor does not match the build ELF")
    target = archive_root / images[2]["sha256"]
    if target.exists():
        verify(target)
        return target
    target.mkdir(parents=True)
    manifest = {"target": "esp32s3", "board": "M5Stack StopWatch", "flash_bytes": FLASH_BYTES,
                "upstream_revision": "fa073045a5572a8e3367fae142c1b7a7c554f222",
                "partitions": partitions, "files": []}
    for i in images:
        (target / i["file"]).write_bytes(i["data"])
        manifest["files"].append({k: i[k] for k in ("file", "bytes", "sha256", "offset")})
    for name in ("muse-gadget.elf", "muse-gadget.map", "config/sdkconfig.json", "project_description.json"):
        data = (build / name).read_bytes()
        basename = Path(name).name
        (target / basename).write_bytes(data)
        manifest["files"].append(dict(file=basename, bytes=len(data), sha256=digest(data)))
    manifest["source_sha256"] = {
        str(p.relative_to(Path(__file__).parent)): digest(p.read_bytes())
        for p in sorted(Path(__file__).parent.rglob("*"))
        if p.is_file() and not set(p.relative_to(Path(__file__).parent).parts).intersection({".git", "__pycache__", "build", "out", "firmware"}) and p.suffix != ".pyc"
    }
    (target / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    verify(target)
    return target


def verify(directory):
    directory = Path(directory).resolve()
    manifest = json.loads((directory / "manifest.json").read_text())
    for file in manifest["files"]:
        path = (directory / file["file"]).resolve()
        if path.parent != directory:
            raise ValueError("Invalid bundle path")
        data = path.read_bytes()
        if len(data) != file["bytes"] or digest(data) != file["sha256"]:
            raise ValueError("Bundle hash mismatch: " + file["file"])
    partition_table((directory / "partition-table.bin").read_bytes())
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("create", "verify"))
    parser.add_argument("directory", type=Path)
    parser.add_argument("--archive-root", type=Path)
    args = parser.parse_args()
    if args.action == "create":
        if not args.archive_root:
            parser.error("--archive-root is required")
        print(create(args.directory, args.archive_root))
    else:
        verify(args.directory)
        print("Bundle verification: PASS")
