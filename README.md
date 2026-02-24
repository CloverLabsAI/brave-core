# Brave Core (CloverLabs Fork)

Custom fork of [brave-core](https://github.com/brave/brave-core) with per-context fingerprinting controls, stealth enhancements, and automation-friendly APIs for Playwright/Puppeteer integration.

## Custom Features

### Per-Context Fingerprinting Override APIs

Self-destructing JavaScript APIs that set overrides per browser context (per Playwright context). All functions delete themselves from `window` after the first call to prevent detection.

```javascript
// Set a master seed that drives all farbling (canvas, audio, screen, WebGL, etc.)
window.setFingerprintingSeed(123456789);

// Override WebRTC local IP addresses
window.setWebRTCIPv4("192.168.1.42");
window.setWebRTCIPv6("fd00::42");

// Override browser timezone (IANA format)
window.setTimezone("Asia/Kolkata");
```

- Overrides persist across navigations within the same browser context
- Workers (dedicated, shared, service) automatically inherit all overrides
- Each Playwright browser context is isolated (separate renderer process)

### WebGL Profile-Based Farbling

At BALANCED level (default), WebGL vendor/renderer are replaced with realistic Apple Silicon GPU profiles instead of random strings. A deterministic profile is selected per-context based on the farbling seed.

### Consistent Cross-Context Farbling

Device memory, hardware concurrency, WebGL vendor/renderer, and all farbled values are guaranteed to match between the main window and worker scopes (dedicated workers, blob workers, OffscreenCanvas).

## Stealth Docs

All custom stealth/fingerprinting documentation lives in `src/brave/stealth_docs/`:

```
stealth_docs/
├── HEADLESS.md      # Stealth module docs
├── PER_CONTEXT.md
├── SCREEN.md
├── WEBDRIVER.md
├── WEBRTC.md
└── design/          # Architecture and design documents
```

### Stealth Modules

- [HEADLESS.md](stealth_docs/HEADLESS.md) - Headless detection and mitigation
- [PER_CONTEXT.md](stealth_docs/PER_CONTEXT.md) - Per-context fingerprinting override APIs
- [SCREEN.md](stealth_docs/SCREEN.md) - Screen and display fingerprinting
- [WEBDRIVER.md](stealth_docs/WEBDRIVER.md) - WebDriver detection and stealth
- [WEBRTC.md](stealth_docs/WEBRTC.md) - WebRTC IP leak protection

### Design Documents

- [MASTER_SEED_OVERRIDE.md](stealth_docs/design/MASTER_SEED_OVERRIDE.md) - Master seed architecture and per-context token derivation
- [WEBGL_FARBLING_PROFILES.md](stealth_docs/design/WEBGL_FARBLING_PROFILES.md) - WebGL profile-based farbling design (10 Apple Silicon GPU profiles)
- [WEBRTC_PER_CONTEXT_COMPLETE.md](stealth_docs/design/WEBRTC_PER_CONTEXT_COMPLETE.md) - WebRTC IP override implementation
- [FARBLING_COMBINATIONS.md](stealth_docs/design/FARBLING_COMBINATIONS.md) - All farbling key combinations and PRNG behavior
- [IMPLEMENTATION_SUMMARY.md](stealth_docs/design/IMPLEMENTATION_SUMMARY.md) - Full implementation summary across all modified files
- [STEALTH.md](stealth_docs/design/STEALTH.md) - Stealth and anti-detection considerations
- [SPEECH_DESIGN.md](stealth_docs/design/SPEECH_DESIGN.md) - Speech synthesis farbling design
- [TESTING_PER_CONTEXT_API.md](stealth_docs/design/TESTING_PER_CONTEXT_API.md) - Testing guide for per-context APIs

## How Brave Patching and Overriding Works

Brave doesn't fork Chromium directly. Instead, `brave-core` sits alongside the Chromium source and uses two mechanisms to modify behavior:

### 1. `chromium_src` Overrides

Files placed in `src/brave/chromium_src/` mirror the Chromium source tree structure. At build time, Brave's build system makes these files take precedence over the original Chromium files. This works through `#include` redirection:

```
src/brave/chromium_src/third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc
```

This file gets compiled **instead of** the original at:
```
src/third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc
```

Typically, the override file `#include`s the original and then adds macros or redefines behavior around it. This avoids maintaining a full fork of each file.

### 2. Patch Files

For changes that can't be done via `chromium_src` (e.g. modifying a header, adding a virtual method, or changing build files), Brave uses `.patch` files stored in `src/brave/patches/`. These are applied to the Chromium source during `npm run init` or `npm run sync`.

Patches live in:
```
src/brave/patches/                          # Chromium patches
src/brave/patches/third_party/              # Third-party dependency patches
```

### 3. Mojo Interfaces & New Files

New features (like our fingerprinting override APIs) add entirely new files in `src/brave/` and connect to the browser via Mojo IPC interfaces. These don't override anything - they're additive.

## How to Build

### Prerequisites

Clone the repos:

```bash
git clone https://github.com/brave/brave-browser.git
git clone https://github.com/CloverLabsAI/brave-core.git brave-browser/src/brave
cd brave-browser/src/brave
npm install
npm run init
```

> **Note:** All build commands must be run from `src/brave/` since brave-core is a separate repo from brave-browser.

### Build

If you made modifications to any patches:

```bash
npm run apply_patches
```

The default build type is **Component**.

```bash
# Component build (default - fastest for development)
npm run build
```

To do a **Static** build:

```bash
npm run build -- Static
```

To do a **Debug** build (Component with `is_debug=true`):

```bash
npm run build -- Debug
```

> **Note:** The build will take a while. Depending on your processor and memory, it could take a few hours.

### Build Configurations

We only build **Component** and **Static** builds since we can't make a Release build (requires official Brave build infrastructure and internal-only build flags like `brave_stats_updater_url`).

| Build Type | Command | Description |
|-----------|---------|-------------|
| **Component** | `npm run build` | Dependencies are separate shared libraries (`.dylib`/`.so`). Fastest incremental builds. Best for development. |
| **Static** | `npm run build -- Static` | Everything packed into one binary. Slower to build but starts faster and is self-contained. Good for testing/distribution. |
| **Debug** | `npm run build -- Debug` | Component build with `is_debug=true`. Full debug symbols and assertions. |
| **Release** | `npm run build Release` | Requires official Brave builder infra. We cannot build this. |

> Running a release build with `npm run build Release` can be very slow and use a lot of RAM, especially on Linux with the Gold LLVM plugin.

### Platform-Specific Builds

**Android:**
```bash
npm run build -- --target_os=android --target_arch=arm
```

**iOS:**

Use the Xcode project found in `ios/brave-ios/App`. You can open it directly or run:
```bash
npm run ios_bootstrap -- --open_xcodeproj
```

## Update / Sync to Latest Chromium & Brave

```bash
npm run sync -- [--force] [--init] [--create] [brave_core_ref]
```

> It's safer to commit local changes before running this. The script will attempt to stash your local changes in brave-core.

`npm run sync` will (depending on flags):

- Update sub-projects (chromium, brave-core) to latest commit of a git ref (e.g. tag or branch)
- Apply patches
- Update gclient DEPS dependencies
- Run hooks (e.g. `npm install` on child projects)

| Flag | Description |
|------|-------------|
| *(no flags)* | Updates chromium if needed and re-applies patches. If chromium version didn't change, only re-applies changed patches. Updates child dependencies only if any project needed updating. **Use this to stay up to date.** |
| `--force` | Updates both Chromium and brave-core to latest remote commit for the current branch and the Chromium ref in `package.json`. Re-applies all patches. Force updates all child dependencies. **Use this when things are broken and you want a known-good state.** |
| `--init` | Force update both Chromium and brave-core to versions in `package.json` and force update all dependent repos. Same as `npm run init`. |
| `--sync_chromium (true/false)` | Force or skip the chromium version update. Useful to avoid a large rebuild when not ready for a chromium update. A warning will show if the code expects a different version. |
| `-D, --delete_unused_deps` | Delete dependencies removed since last sync. Mimics `gclient sync -D`. |

To checkout a specific brave-core ref and update everything:
```bash
npm run sync brave_core_ref
```

## Resources

- [Brave Browser Issues](https://github.com/brave/brave-browser/issues)
- [Brave Releases](https://github.com/brave/brave-browser/releases)
- [Brave Core Docs](https://github.com/brave/brave-core/blob/master/docs/README.md)
- [Brave Wiki](https://github.com/brave/brave-browser/wiki)
