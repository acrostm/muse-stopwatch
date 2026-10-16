<p align="right"><a href="AGENTS.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Agent instructions

This is the standalone Muse Gadget SDK adaptation for M5Stack StopWatch,
ESP32-S3 with 16 MB flash and 8 MB PSRAM, using ESP-IDF 6.0.1.
Read README.md and docs/DEVELOPMENT.md before editing. Start with
`git status --short --branch` and preserve unrelated user work.

- Keep this repository limited to the StopWatch adaptation. Prepare an isolated
  SDK with build.py; never modify a caller's checkout or vendor the entire SDK.
- Pin upstream revisions and dependencies. Cover each patched upstream file
  with its pristine SHA-256 in upstream-hashes.json and verify clean preparation.
- Audio must remain local until a real release longer than two seconds. Short,
  cancelled, failed or unreleased recordings must never enter upload or retry
  queues. Never fall back to streaming when allocation fails.
- Keep callbacks non-blocking, network/audio in workers, and LVGL in its task.
  Keep testable state and protocols independent from ESP-IDF and add regression
  tests for behavior changes. Do not disable failing tests or accept skips.
- Run tools/validate.py with CJSON_SOURCE_DIR configured, then build and verify
  firmware. Separate Build, Host tests, Device tests and Unverified in delivery.
  A build does not prove hardware behavior.
- Flash only with user authorization. Use the verified bundle and compatible
  layout. Application updates at 0x10000 preserve NVS only with the expected
  bootloader/partition layout. Never erase the whole chip without explicit consent.
- Do not commit secrets, logs, NVS images, firmware/ELF/MAP files, upstream avatar
  art, or other projects. Use sample-only documentation screenshots. Preserve
  upstream license notices and consult NOTICE before redistributing binaries.
- Maintain English .md documents with paired .zh_CN.md translations and reciprocal
  links. Explain public behavior, compatibility and validation in PR descriptions.
- Git writes and publication require user authorization; CI source releases are
  triggered deliberately by a version tag after all gates pass.
