# Subsonic / stream-bridge integration tests

Run from the repository root on a Linux host:

```sh
make -C sonix-player/tests test
make -C sonix-player/tests sanitize
```

These require only a C compiler, Make and Python 3 (plus standard pthread/dl/math libraries). They **do not require LVGL, ALSA, a MIPS toolchain, a music server, a real account, or downloaded audio**.

`run_subsonic_tests.py` starts a temporary threaded localhost HTTP server plus three HTTPS servers carrying the certificates in `tls_fixtures.py`, runs the C binary with a private temporary config directory, verifies its requests, then shuts down. All account values are synthetic. Build products live in ignored `.build/`; response/PCM fixtures are generated in memory rather than added to Git, and the three test certificates are pasted into that Python file rather than downloaded.

## Coverage

The C binary links the **real** HTTP/TLS helpers, REST client, MD5, JSON, configuration, playlist, RAM producer/reader and two-slot manager. `support.c` replaces only unrelated platform scheduling/signal helpers, translations and local-library indexes; it does not replace networking, buffers or decoding.

Checks include:

- correct salted MD5 authentication with encoded UTF-8/special characters, fresh salts, and no password/token query in diagnostic output;
- TLS against real self-signed certificates: `strict` (the default) refuses one, `relaxed` accepts it, a certificate naming another server and an expired one are refused either way, a connection opened under the old mode survives the setting changing under it, and the mode round-trips through `device_config.ini`;
- server URL/port rejection, exact opaque/numeric IDs (including >64-bit IDs), bounded JSON/token parsing, and malformed/oversized/auth-failed responses;
- music folders → indexes → directory and artists → artist → album, and 200-entry pagination;
- optional atomic 0600 credential persistence/round-trip, clearing remembered credentials, reporting a failed Forget instead of false success, rejecting symlinks/truncated identities/embedded NUL;
- fixed RAM capacity, millions of bytes across ring wrap, 50% refill hysteresis, protected initial header read/rewind vs a racing producer, retained-header rewind vs overwritten history, and 512 KiB pre-cache promotion;
- chunk extensions/trailers, transfer framing never reaching the reader, truncation/empty bodies/error pages, and retry after timeouts inside a chunk size, CRLF and trailer;
- cancellation during HTTP header setup, a stalled body, and reader starvation, followed by generation-safe reuse;
- nonmutating playlist next-track look-ahead across repeat/shuffle/ordered modes;
- exclusive decoder leases, both handoff/teardown orders, failed pre-cache retry suppression, repeat-one starting at byte zero, account-change invalidation with an outstanding lease, metadata lookup and unsupported raw codecs.

## Real codec integration

Install the normal host development libraries (Debian/Ubuntu example):

```sh
sudo apt-get install build-essential python3 pkg-config libopusfile-dev libwavpack-dev
make -C sonix-player/tests codecs
make -C sonix-player/tests sanitize-codecs
```

The codec suite links the actual player decoder dispatcher and bundled `dr_mp3`/`dr_flac`, plus **real libopusfile/libopus/libogg**. Other player codecs are also compiled/linked normally, not replaced with fake decoders. `PKG_CONFIG`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS` and `BUILD` can be overridden for a custom dependency installation.

`media_fixtures.py` creates deterministic silent MP3 frames, lossless FLAC verbatim frames with real CRCs, and Ogg Opus silence packets with valid page CRCs and preskip/end trimming. No ffmpeg/encoder command or binary media fixture is needed. Tests check exact PCM frame counts (including MP3 Xing/LAME delay/padding and Opus preskip/end trim), buffered-source identity, seek/source-span exclusions, codec mismatch rejection and canceled-source opens. Large ID3/FLAC headers and >1 MiB FLAC transfers exercise forward header skipping and ring wrap without a file cache.

A no-ID3 MP3 fixture is intentional: the bundled `dr_mp3` discards its first ten bytes when given a NULL seek callback, losing the first audio frame. The production bridge supplies **limited retained-header seeks** and refuses an unknown-length whole-stream count; this test detects both regressions.

Both sanitizer targets use AddressSanitizer and UndefinedBehaviorSanitizer. At `-O1`, GCC reports pre-existing intentionally bounded/truncating MP4 tag copies; `sanitize-codecs` leaves that warning category nonfatal, while other warnings remain fatal. The core suite keeps all warnings fatal.

## Limits

The fixture proves transport, byte/sample counts and ownership behavior on the host. Scheduling helpers are stubs and there is **no ALSA/device loop** here, so these tests do not certify real-time scheduling, gapless analog output, Wi-Fi performance, target CPU use or battery life. The TLS cases run against the OpenSSL the host has (the player's own client code, `dlopen`ed exactly as on the device, sees the host's libssl 1.1/3.x); with no loadable OpenSSL they print `SKIP TLS` and the rest of the suite still runs. Real-network trust, redirects and certificate cancellation, plus UI interaction, should also be checked on hardware. See [the V1 checklist](../../docs/subsonic.md#validation).
