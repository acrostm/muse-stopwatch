<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Muse StopWatch

[![CI](https://github.com/acrostm/muse-stopwatch/actions/workflows/ci.yml/badge.svg)](https://github.com/acrostm/muse-stopwatch/actions/workflows/ci.yml)

A community adaptation of [Muse Gadget SDK](https://github.com/facebookincubator/muse-gadget-sdk) for **M5Stack StopWatch (ESP32-S3, 16 MB flash, 8 MB PSRAM)**. It adds a browser setup portal, Doubao Speech Generation 2.0 playback, named voice selection and preview, four device pages, and cancellable push-to-talk.

This repository contains the adaptation and a patch against one pinned SDK revision, rather than a fork of every upstream device. It does not contain other AI Passport applications, private configuration, build outputs, or upstream avatar assets. It is independent of Meta and M5Stack.

## What you get

- Right blue button: hold **more than 2 seconds**, then release to send. Holds of exactly 2 seconds or less are discarded.
- While holding right: press the left yellow button to cancel. Release right before starting again. Cancellation applies while recording; it cannot recall a submitted message.
- Left button otherwise: cycle avatar → settings → status → recent conversation. The history holds eight entries in RAM.
- Audio stays in PSRAM until release passes the duration check. Allocation failure, capture error, cancellation, or reaching the approximately 15-second recording limit without release discards the recording. No streaming fallback bypasses this check.
- Accepted notes may wait in RAM for connectivity (up to four, bounded retries); a reboot loses them. Delivery retries can duplicate a note if its acknowledgement was lost.
- Wi-Fi setup hotspot with a persistent, per-device eight-digit PIN. Credentials stay on the device; saved secrets are never returned to the browser.
- Doubao named voice search, browser preview, and optional official ListSpeakers refresh. Voice availability depends on your account. Preview makes a billable provider request.
- Per-Wi-Fi SOCKS5/Clash profiles for Muse cloud domains; new networks default to direct. The portal can enable/disable a proxy or edit its address/port for any selected network. Doubao stays direct, and TLS verifies the original destination.

## Screens

The device captures below use **explicit sample data** in the optional USB snapshot build. They show our adaptation pages and contain no saved credentials or conversations. The upstream Jollybot artwork is excluded from this repository and its license.

![Device settings with sample hotspot information](docs/images/settings.png)
![Connection and TTS status using sample data](docs/images/status.png)
![Recent conversation using sample data](docs/images/history.png)

## Connection model

```text
StopWatch microphone → local recording buffer → Wi-Fi → Muse cloud → official Muse app
Muse reply text → StopWatch → Doubao TTS → StopWatch speaker
Phone Bluetooth ↔ StopWatch: discovery and initial pairing
```

**This is not a Bluetooth-only, offline headset.** The official Muse cloud and app remain part of the workflow; Bluetooth normally turns off after pairing. No additional companion app is required. Outdoors, connect StopWatch to your phone's 2.4 GHz hotspot and keep cellular internet available. If your network requires a proxy, that proxy must also be reachable outdoors: a proxy left on a home Mac will not travel with you. Fully offline Muse inference and forwarding audio through the phone over Bluetooth are not implemented.

## First use

1. Obtain a Muse gadget SDK token through [Muse Gadgets](https://gadgets.muse.ai/). Use the official Muse app, enable Devices → Developer mode, and grant Bluetooth access.
2. On StopWatch, open Settings → Open Wi-Fi setup. Connect a phone or computer to the displayed Muse-Setup hotspot using the eight-digit PIN shown on the device. The PIN is persisted across boots.
3. Open **http://192.168.4.1/**. Enter a 2.4 GHz Wi-Fi SSID/password and your Muse SDK token. Blank saved-secret fields retain their existing value. Clearing saved credentials is not yet exposed in the portal.
4. Configure Doubao Speech Generation 2.0 using an API Key, or the older App ID + Access Token pair. Choose a named voice, audition it, then save. The device needs working internet for preview; preview audio plays in the browser.
5. Optional voice-list refresh needs **Volcengine account AK/SK**, different from the TTS API Key. These query credentials are not persisted. A listed voice does not guarantee your account can synthesize it.
6. Add the device from the official Muse app and confirm pairing with the blue talk button. If already paired, do not repeatedly scan: check the device Status page for the cloud link instead.
7. Hold blue, speak for more than two seconds, and release. Press yellow while holding blue to discard and retry.

Select the Wi-Fi name in the portal before editing its proxy switch, LAN IPv4, and SOCKS5/mixed port. Existing Wi-Fi passwords, SDK tokens, and TTS keys are retained when left blank. A blank password for an already saved Wi-Fi edits its proxy without forcing a network switch. New networks default to direct; reconnecting to an existing SSID restores that network's profile. Use distinct SSIDs for your home network and phone hotspot. Disabling a proxy retains its address for re-enabling; disabling and clearing the address removes the profile. Up to eight proxy profiles are stored; full tables reject new proxy entries without evicting existing settings. Upgrades migrate the old global proxy to the previously configured SSID; old firmware does not understand these profiles. The device Status page shows Direct or SOCKS5. Changing the active route alone restarts the device so cloud sockets use the new route; editing an inactive network does not interrupt the current link. When Wi-Fi credentials also change, the SDK saves them before reconnecting, rather than being interrupted by a proxy-triggered restart. The proxy must allow LAN clients without authentication. This adapter deliberately accepts only private LAN IPv4 proxy addresses; restrict access on the proxy host. The setup portal is HTTP on the device hotspot, not a remotely exposed management service.

## Build from source

Requirements: Git, Python 3.11+, a C compiler, Node.js for JavaScript syntax checks, and **ESP-IDF v6.0.1** activated for ESP32-S3. Linux host tests also need OpenSSL development headers. This is not the ESP32-C3 AI Passport firmware or its ESP-IDF 5.5.3 profile.

```bash
python3 build.py build --out /tmp/muse-stopwatch-sdk
python3 package.py create /tmp/muse-stopwatch-sdk/esp32/build-stopwatch --archive-root ./firmware
python3 package.py verify ./firmware/<application-sha256>
```

The output directory must not exist. `build.py` fetches upstream revision `fa073045a5572a8e3367fae142c1b7a7c554f222`, verifies every patched upstream file, copies it, applies `upstream.patch`, and injects `src/`. Your SDK checkout is never edited. For an existing exact checkout, pass `--sdk-source /path/to/muse-gadget-sdk`. Dependencies are pinned in `dependencies.lock`.

For prepare-only: `python3 build.py prepare --out /tmp/muse-stopwatch-sdk`, then invoke `idf.py` in the prepared project with the same defaults listed in `build.py`. Never embed tokens in build profiles.

### Flashing

Our partition layout is **NVS 0x9000–0xdfff, PHY 0xe000–0xefff, application 0x10000–end of 16 MB**. An application-only update preserves these data regions only if the existing bootloader and partition table match. Check your current layout before using this update path. First installation from another layout needs the matching generated bootloader and partition table; do not assume arbitrary stock firmware has this layout.

For an existing compatible installation, enter download mode and use the verified bundle's application:

```bash
python -m esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX --before usb-reset --after hard-reset write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x10000 firmware/<application-sha256>/muse-gadget.bin
```

On Linux replace the serial port with the detected `/dev/ttyACM*` port. Do not write an application binary at 0x0, and do not erase the whole chip to perform an update. `package.py` checks the target, flash size, protected ranges, partition-table integrity, and artifact hashes; retain the matching local ELF for debugging, without publishing private logs or configuration.

## Development and validation

```bash
# Host tests do not require the ESP-IDF toolchain.
python3 tools/fetch_test_deps.py /tmp/muse-stopwatch-cjson
export CJSON_SOURCE_DIR=/tmp/muse-stopwatch-cjson
CC=clang python3 tools/validate.py
# Validate a completed firmware build and its actual LVGL integration:
python3 tools/validate.py --firmware /tmp/muse-stopwatch-sdk/esp32/build-stopwatch
python3 tools/test_ui.py /tmp/muse-stopwatch-sdk/esp32/managed_components/lvgl__lvgl
```

CI runs host tests on Linux and macOS with warnings as errors and ASan/UBSan for the executable C harnesses; no skipped tests pass the gate. It also checks JavaScript syntax, local documentation links, bilingual document pairing, and patch/source alignment. A separate ESP-IDF 6.0.1 job builds and verifies firmware and runs real headless LVGL startup, rendering, swipe, and four-page cycling tests on every pull request. Version tags run both gates before publishing a **source-only** release with checksums. Firmware binaries and the default avatar are not redistributed; review upstream asset and dependency licenses before sharing local builds.

Pure state/protocol logic lives in `sw_core`, `sw_proxy_core`, and `sw_sign`; ESP-IDF workers own network/audio operations. Button callbacks only update state or queue work. LVGL access stays in its task. See [contributing](CONTRIBUTING.md), [security](SECURITY.md), and [architecture / test handoff](docs/DEVELOPMENT.md).

## Limitations and licensing

This is early community firmware. A successful build is not a hardware acceptance test. Test speaker output, reconnects, battery use, and your phone's hotspot on the actual StopWatch. The firmware stores tokens in ordinary NVS; flash encryption and secure boot are not enabled by this development profile. Physical access can expose configuration.

Our source and upstream modifications use [Apache-2.0](LICENSE). See [NOTICE](NOTICE) for upstream provenance, dependency licensing, and the **Jollybot avatar exclusion**. Muse services, SDK tokens, and Doubao access remain subject to their providers' terms.
