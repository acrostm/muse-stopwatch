<p align="right"><a href="SECURITY.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Security

Do not post secrets or exploit details in a public issue. Use this repository's GitHub private vulnerability reporting to contact the maintainer; if it is unavailable, first open a minimal issue requesting a private channel without technical exploit details.

Only the latest main branch is maintained. Reports should identify the revision, hardware, prerequisites, impact, and a minimal reproduction with dummy credentials. There is no guaranteed response SLA.

The portal is plain HTTP restricted to the setup hotspot subnet. The eight-digit hotspot PIN is access control, not protection against physical extraction. This development profile uses ordinary NVS without flash encryption or secure boot. API keys and device pairing data must be treated as recoverable by someone with physical access. The optional proxy is unauthenticated LAN SOCKS5; use a trusted LAN and restrict access at its host. TLS certificate validation is retained for provider endpoints.

Saved secrets are not returned by status APIs. ListSpeakers account AK/SK is used only for a query, and preview audio is bounded and expires. These safeguards do not make raw RAM, NVS, screenshots or debug logs safe to publish. Check all attachments manually and rotate any accidentally exposed credential.
