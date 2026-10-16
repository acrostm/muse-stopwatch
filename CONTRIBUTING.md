<p align="right"><a href="CONTRIBUTING.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Contributing

Small, reviewable pull requests are welcome. Open an issue for changes to pairing, transport, partition layout, or recording semantics before substantial implementation. Keep the project specific to M5Stack StopWatch; changes for other boards usually belong upstream.

1. Fork this repository and create a topic branch.
2. Activate ESP-IDF 6.0.1. Prepare a clean pinned SDK with `build.py`; never edit a shared SDK checkout.
3. Put reusable, testable state and protocol code in the pure C modules and add meaningful host regression tests. Keep audio and networking out of button callbacks; serialize LVGL access in its task.
4. Run `tools/fetch_test_deps.py` and `tools/validate.py` as documented in the README, build firmware, and validate its bundle. Include device results separately: a build is not a hardware test.
5. Update both language versions of affected documents. Submit a PR explaining the trigger, resulting behavior, compatibility, tests, and unverified hardware cases.

To update the upstream patch, start from the pinned upstream commit, apply the current patch to a separate SDK copy, edit that copy, regenerate a unified patch, and update SHA-256 entries in `upstream-hashes.json` **from the pristine upstream files**. Add every touched upstream file to the hash map. Test preparation against an unmodified checkout. Do not silently advance the pinned revision or dependency lock.

Never include SDK/API tokens, Wi-Fi passwords, hotspot PINs, private conversation text, serial dumps, NVS images, compiled binaries, or upstream avatar artwork. Use the optional snapshot documentation mode for sample screenshots. Preserve upstream copyright notices and explain third-party licensing changes. Be respectful, assume good intent, and focus review on reproducible evidence.

CI gates must pass before merge; hardware behavior still needs explicit acceptance. Versioned source releases are built by the tag workflow after the gates pass. The VERSION file must match the v-prefixed tag. Release maintainers must not attach upstream-avatar firmware without resolving redistribution rights.
