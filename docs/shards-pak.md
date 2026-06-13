# `shards pak` — single-file Shards apps

Turn any Shards script into a standalone, signed executable with one command:

```sh
shards pak main.shs            # -> ./main  (or main.exe on Windows)
./main                         # runs the embedded script — no interpreter, no runtime files
```

No C++ toolchain, no CMake. The produced binary embeds the compiled script and
carries the whole Shards runtime (every module linked into the `shards` binary
you packed with).

## How it works

The `shards` executable *is* the runtime, and `langffi` already knows how to run
a compiled script (`.sho`). So a "pak" compiles no C/C++ — it embeds the compiled
script into a copy of the runtime that is already on disk, using each platform's
**native container mechanism** so the result stays structurally valid and
signable (it does *not* rely on appending bytes after the end of the image, which
breaks code-signing and trips antivirus heuristics).

`shards pak main.shs`:

1. Parses and compiles `main.shs` to its binary `.sho` form **in memory**
   (`"SHRD"` FourCC + ABI version + flexbuffers AST — the exact bytes
   `shards build` would write).
2. Copies the running `shards` executable (`std::env::current_exe()`).
3. Embeds the payload and (re-)signs, per platform:

| Platform | Embedding | Runtime lookup | Signing |
|----------|-----------|----------------|---------|
| **macOS** | inside the `__LINKEDIT` segment (grown to cover it) | payload sits just before `LC_CODE_SIGNATURE.dataoff` | `codesign` (ad-hoc by default) |
| **Windows** | `RT_RCDATA` resource via `UpdateResource` | `FindResource`/`LoadResource` (in-memory) | `signtool` (Authenticode) |
| **Linux** | `.shards_pak` ELF section via `objcopy` (overlay fallback) | section in `/proc/self/exe` (EOF fallback) | n/a |

A fixed 8-byte magic (`SHRDPAK1`) plus a `u64` length form a 16-byte footer on the
payload, so a plain, un-packed `shards` binary is never mistaken for a packed one.

### Why not just append to the end of the file?

Appending bytes after the image (an "overlay") is the simplest trick but the worst
for signing/AV:

- **macOS** rejects it outright — `codesign` fails with *"main executable failed
  strict validation"* when there is data after the Mach-O, and arm64 refuses to
  run a binary whose signature doesn't cover it. The payload **must** live inside
  a segment and before the signature, which is why it goes into `__LINKEDIT`
  (which is grown so `codeLimit` includes it). Verified: the packed binary reports
  `codesign --verify` → *valid on disk*.
- **Windows** Authenticode *can* tolerate an overlay if you sign last, but a large
  appended overlay on an unsigned binary is a classic AV/SmartScreen heuristic. A
  proper `RT_RCDATA` resource keeps the PE conventional and clean.
- **Linux** has no signing/Gatekeeper gate, so an overlay is harmless — but a real
  `.shards_pak` section is tidier, so we use one when `objcopy` is available.

### Startup

Every `shards` binary, on launch (after core init, before argv parsing), asks the
platform extractor whether *this* executable carries a payload (reads its own
`__LINKEDIT`/section/resource). If so it runs the embedded script via the normal
`execute_seq` path, forwarding extra argv to the script as `key:value` defines. A
plain binary has no payload, so the check returns instantly.

## Usage

```
shards pak <file.shs> [-o <output>] [-I <dir>...] [signing options]

  -o, --output <path>        Output executable (default: script name, no extension)
  -I, --include <dir>        Additional include directories for @include / @read

  --sign <identity>          Signing identity.
                               macOS:   codesign -s <identity>   (default: ad-hoc "-")
                               Windows: signtool /n <subject>
  --no-sign                  Do not sign (macOS leaves an invalid signature; testing only)
  --notarize                 macOS: submit for notarization after signing
  --notarize-profile <name>  macOS: xcrun notarytool --keychain-profile <name>
```

Examples:

```sh
shards pak game.shs                                   # ad-hoc signed, runs locally
shards pak game.shs --sign "Developer ID Application: Acme (TEAMID)" \
                    --notarize --notarize-profile acme   # distributable on macOS
shards pak game.shs --sign "Acme Code Signing"        # Windows Authenticode
./game arg1:hello arg2:world                           # extra args -> script defines
```

## Resources

What ends up inside the single binary is whatever is in the **compiled AST**:

- **Embedded (packs):** `@include("other.shs")` and
  `@read("logo.png" Bytes: true)` are resolved at *parse time* and baked into the
  `.sho` as constants. Use `@read` to embed images, data files, etc. (Verified: a
  file embedded via `@read` is readable from a packed binary run in a directory
  where the original file does not exist.)
- **Not embedded:** anything loaded at **runtime by path** — `FS.Read`,
  `LoadImage`, audio/glTF/shader loaders — reads from disk at run time. The path
  string is in the binary; the file is not. Pack such assets via `@read`, or ship
  them alongside the executable.

```clojure
; baked into the binary:
@define(logo @read("logo.png" Bytes: true))
; runtime path-load — needs logo.png on disk next to the app:
"logo.png" | LoadImage
```

(There is no embedded virtual filesystem in shards-core; only Formabble has one.)

## Limitations & notes

- **Size**: the packed binary is a full copy of `shards` (all linked modules), so
  it is as large as `shards`. To shrink it, build a `shards` with only the modules
  you need (`SHARDS_WITH_*` options) and pak with that binary.
- **Same platform/ABI**: the embedded `.sho` is tied to `SHARDS_CURRENT_ABI`; a
  packed binary refuses a payload from a different ABI. You pak on the platform you
  target (no cross-pak).
- **macOS universal binaries**: not supported — pak a thin (single-arch) `shards`.
- **Reputation ≠ validity**: ad-hoc signing makes the binary *valid* (it runs
  locally without the arm64 "killed: invalid signature"), but Gatekeeper /
  SmartScreen clear downloads based on a real signing identity (+ notarization on
  macOS). For non-flagged distribution, sign with a Developer ID / Authenticode
  cert via `--sign` (and `--notarize` on macOS).
- **The runtime is fixed at pak time** — you cannot add native (C++/Rust)
  extensions this way. For a customizable native app, bundle scripts at build time
  with CMake (`shards_build` + `target_bundle_files` + `target_generate_bundle_manifest`).

## Implementation

- `shards/lang/src/pak.rs` — platform embedders/extractors (`embed_payload`,
  `load_self_payload`) and `SignOpts`.
- `shards/lang/src/cli.rs` — the `pak` subcommand, the startup self-check in
  `process_args`, and shared helpers `read_program` / `serialize_sho` /
  `deserialize_sho` (also used by `build` / `load`).

### Testing

`shards/tests/pak.sh` is a self-contained smoke test (runnable locally and in CI;
Windows via Git Bash). It packs a script that embeds a resource via `@read`, runs
the produced binary **from a clean directory** with neither the source nor the
resource present, and asserts the marker output, the embedded resource byte count,
that extra argv does not break it, and — on macOS — that `codesign --verify`
passes:

```sh
shards/tests/pak.sh build/shards     # or build/Release/shards
```

It runs in CI on all three platforms as a step in each build's test job
(`build-linux.yml`, `build-macos.yml`, `build-windows.yml`), so the Linux ELF
section and Windows PE-resource round-trips are exercised on real runners.

### Validation status

- **macOS (arm64):** fully validated on-device — `codesign --verify` valid, runs,
  self-contained, `@read` resources embed correctly; covered by CI.
- **Linux / Windows:** implemented and **type-checked** via cross-compilation
  (`cargo check --target x86_64-unknown-linux-gnu` / `x86_64-pc-windows-msvc`), and
  exercised by the CI smoke test above — run the CI to confirm the round-trip on
  those runners.
