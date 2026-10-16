<p align="right"><a href="DEVELOPMENT.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Architecture and hardware acceptance

`build.py` is an isolated source preparer and ESP-IDF driver. `upstream-hashes.json` binds the files modified by `upstream.patch` to the pinned SDK. `package.py` verifies the flash layout and binds the application, ELF, configuration, partition table and source fingerprints into a local content-addressed archive.

`sw_core` implements bounded UTF-8 history, line framing, persistent PIN validation, idle-pairing decisions and physical PTT duration/cancellation. A short FreeRTOS critical section serializes the PTT guard across the input and voice tasks. Audio is held in PSRAM; only an actual release longer than two seconds admits the note to upstream transmission. The release timestamp precedes queue delivery, so wake handling, pre-roll and capture tail cannot inflate the press duration. Overflow/error paths discard instead of streaming.

`sw_runtime` owns configuration/history synchronization. `sw_portal` serves the hotspot UI and queues network/storage operations. `sw_voice` implements voice discovery and bounded preview; `sw_tts` handles provider stream framing, PCM and generation-based cancellation. `sw_proxy_core` builds bounded SOCKS5 requests; `sw_proxy` supplies domain-scoped DNS/connect wrappers. `sw_pages` owns LVGL widgets in the UI task. Upstream continues to own the board, microphone, codecs, avatar, Muse pairing and cloud session.

Host harnesses exercise real C implementations with ASan/UBSan, including exhaustive duration boundaries, UTF-8/history bounds, fragmented provider streams, cancellation, signing against a Python HMAC oracle, fragmented SOCKS5 sockets and artifact tamper detection. SDK integration and board peripherals need the firmware job and hardware checks; host stubs cannot prove those.

## Hardware checklist

- Record a sub-second tap, a roughly two-second hold, and a deliberate longer note. Only the last should reach Muse; use serial timing for an exact boundary test.
- Hold blue for more than two seconds, press yellow while still holding blue, then release blue. No note should arrive. Repeat with a fresh recording and verify normal delivery.
- Hold to the recording limit without release: no note should arrive. Check no stuck listening state after release.
- Check low-memory failure without a streaming fallback, offline accepted-note buffering, reconnection and bounded queue capacity.
- Verify CJK glyphs, all pages, TTS playback, voice selection/preview, saved configuration across reboot, cloud recovery, hotspot timeout, battery sleep/wake and the intended phone hotspot.
- Report Build, Host tests, Device tests and Unverified separately. Never substitute old binaries after a failed build.

## USB screenshots

For documentation only, enable `CONFIG_LV_USE_SNAPSHOT=y` in the prepared build's sdkconfig and rebuild. This exposes the upstream USB snapshot command and `>sw.docs=1`, `>sw.docs=2`, `>sw.docs=3` for sample settings/status/history pages. Sample mode alters only presentation, never stored network or pairing configuration. It stays active until reboot. Capture using `tools/capture.py`; that tool uses a raw USB descriptor without toggling DTR/RTS. Example:

```bash
python3 tools/capture.py /dev/cu.usbmodemXXXX --page 2 --out docs/images/status.png
```

Production defaults leave snapshot support disabled. Do not capture the real settings or conversation pages for public documentation. Captures in this repository are generated from sample mode and exclude upstream avatar art.
