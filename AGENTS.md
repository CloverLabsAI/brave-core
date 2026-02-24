# AI Agent System Prompt for Brave Core Development

This document contains the complete system prompt and context required for AI agents (Claude, ChatGPT, etc.) to effectively modify, build, and ship changes to the Brave browser codebase.

---

## Project Context

You are working on **Brave Core**, a custom fork of the Brave browser with enhanced stealth, fingerprinting controls, and automation-friendly APIs. The codebase lives at:

```
brave-browser/                 # Main repository
└── src/brave/                # brave-core fork (CloverLabs custom)
```

All commands must be run from `src/brave/` since brave-core is a separate repo.

---

## How Brave Patching and Overriding Works

Brave doesn't fork Chromium directly. Instead, `brave-core` sits alongside Chromium source and uses **three mechanisms** to modify behavior:

### 1. `chromium_src` Overrides

Files in `src/brave/chromium_src/` **mirror** the Chromium source tree structure. At build time, these files take precedence over the original Chromium files through `#include` redirection.

**Example:**
```
src/brave/chromium_src/third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc
```
This file gets compiled **instead of**:
```
src/third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc
```

**Typical pattern:**
```cpp
// In src/brave/chromium_src/path/to/file.cc
#define SOME_FUNCTION SOME_FUNCTION_ChromiumImpl
#include "src/path/to/original/file.cc"
#undef SOME_FUNCTION

// Now implement Brave's version
void SOME_FUNCTION() {
  // Brave's custom implementation
  SOME_FUNCTION_ChromiumImpl();  // Can call original if needed
}
```

This avoids maintaining a full fork of each file.

### 2. Patch Files

For changes that **can't** be done via `chromium_src` (modifying headers, adding virtual methods, changing build files), use `.patch` files stored in:

```
src/brave/patches/                 # Chromium patches
src/brave/patches/third_party/     # Third-party dependency patches
```

**Patches are applied during `npm run init` or `npm run sync`.**

### 3. Mojo Interfaces & New Files

New features (like fingerprinting override APIs) add entirely new files in `src/brave/` and connect to the browser via **Mojo IPC interfaces**. These don't override anything - they're additive.

**Example structure:**
```
src/brave/browser/brave_fingerprinting/
├── brave_fingerprinting_service.h
├── brave_fingerprinting_service.cc
└── brave_fingerprinting_service.mojom
```

---

## Build System

### Prerequisites

```bash
# Clone repos
git clone https://github.com/brave/brave-browser.git
git clone https://github.com/CloverLabsAI/brave-core.git brave-browser/src/brave
cd brave-browser/src/brave
npm install
npm run init
```

### Build Commands

**All commands run from `src/brave/`:**

```bash
# Apply patches after modifying any .patch files
npm run apply_patches

# Component build (default - fastest for development)
npm run build

# Static build (self-contained binary)
npm run build -- Static

# Debug build (Component with is_debug=true)
npm run build -- Debug
```

### Build Configurations

| Build Type | Command | Description |
|-----------|---------|-------------|
| **Component** | `npm run build` | Dependencies are separate shared libraries. Fastest incremental builds. Best for development. |
| **Static** | `npm run build -- Static` | Everything packed into one binary. Slower to build but starts faster. Good for testing/distribution. |
| **Debug** | `npm run build -- Debug` | Component build with `is_debug=true`. Full debug symbols and assertions. |
| **Release** | ❌ Cannot build | Requires official Brave infrastructure and internal-only build flags. |

### Platform-Specific Builds

```bash
# Android
npm run build -- --target_os=android --target_arch=arm

# iOS
npm run ios_bootstrap -- --open_xcodeproj
```

---

## Testing Changes

After making modifications:

1. **Apply patches** (if you modified any `.patch` files):
   ```bash
   npm run apply_patches
   ```

2. **Build**:
   ```bash
   npm run build
   ```

3. **Run Brave**:
   ```bash
   # macOS
   ../out/Component/Brave\ Browser.app/Contents/MacOS/Brave\ Browser

   # Linux
   ../out/Component/brave

   # Windows
   ..\out\Component\brave.exe
   ```

4. **Test your changes** - Open DevTools console and test APIs, inspect network behavior, etc.

---

## Common Development Patterns

### Adding a New Feature

1. **Determine the approach:**
   - Can it be done via `chromium_src` override? → Use that
   - Need to modify a header or build file? → Use a `.patch`
   - Entirely new functionality? → Add new files + Mojo interface

2. **For `chromium_src` overrides:**
   - Mirror the Chromium source tree structure
   - Use `#define` to redirect function calls
   - Include the original file, then implement Brave's version

3. **For patches:**
   - Modify the file directly in Chromium source
   - Generate the patch:
     ```bash
     npm run update_patches
     ```
   - The patch will be created in `src/brave/patches/`

4. **For new files:**
   - Add them to `src/brave/` (not in `chromium_src`)
   - If adding Mojo interfaces, update `BUILD.gn` files
   - Connect to browser process via Mojo IPC

### Modifying Existing Fingerprinting Code

**Example: Adding a new farbling key**

1. **Find the farbling implementation** (usually in `chromium_src/third_party/blink/...`)
2. **Update the token derivation** in the master seed system
3. **Rebuild and test**:
   ```bash
   npm run build
   ```
4. **Test the API**:
   ```javascript
   window.setFingerprintingSeed(123456789);
   // Test that your new farbling uses the seed correctly
   ```

---

## Sync and Update

```bash
# Update to latest Chromium & Brave (safe, only re-applies changed patches)
npm run sync

# Force update everything to a known-good state
npm run sync -- --force

# Update to a specific brave-core ref
npm run sync brave_core_ref
```

---

## Key File Locations

### Fingerprinting/Stealth Code
```
src/brave/chromium_src/third_party/blink/renderer/modules/webgl/
src/brave/chromium_src/third_party/blink/renderer/core/dom/
src/brave/chromium_src/content/browser/renderer_host/
```

### Per-Context Override APIs
```
src/brave/browser/brave_fingerprinting/
src/brave/renderer/brave_fingerprinting/
```

### Patches
```
src/brave/patches/
```

### Documentation
```
src/brave/stealth_docs/           # All stealth/fingerprinting docs
src/brave/stealth_docs/design/    # Architecture and design docs
```

---

## Agent Guidelines

When asked to modify Brave:

1. ✅ **DO:**
   - Ask clarifying questions about the desired behavior
   - Check existing stealth docs for related implementations
   - Use the appropriate patching mechanism (chromium_src, .patch, or new files)
   - Test changes by building and running Brave
   - Update relevant documentation in `stealth_docs/`
   - Explain what you changed and why

2. ❌ **DON'T:**
   - Make assumptions about how Brave's farbling works - check the docs first
   - Modify Chromium source directly without understanding the patch/override system
   - Skip building and testing your changes
   - Break existing per-context APIs or master seed behavior
   - Forget to update documentation when adding new features

---

## Documentation

All custom stealth/fingerprinting documentation lives in `src/brave/stealth_docs/`:

### Stealth Modules
- [HEADLESS.md](HEADLESS.md) - Headless detection and mitigation
- [PER_CONTEXT.md](PER_CONTEXT.md) - Per-context fingerprinting override APIs
- [SCREEN.md](SCREEN.md) - Screen and display fingerprinting
- [WEBDRIVER.md](WEBDRIVER.md) - WebDriver detection and stealth
- [WEBRTC.md](WEBRTC.md) - WebRTC IP leak protection

### Design Documents
- [design/MASTER_SEED_OVERRIDE.md](design/MASTER_SEED_OVERRIDE.md) - Master seed architecture
- [design/WEBGL_FARBLING_PROFILES.md](design/WEBGL_FARBLING_PROFILES.md) - WebGL profile-based farbling
- [design/WEBRTC_PER_CONTEXT_COMPLETE.md](design/WEBRTC_PER_CONTEXT_COMPLETE.md) - WebRTC IP override
- [design/FARBLING_COMBINATIONS.md](design/FARBLING_COMBINATIONS.md) - All farbling key combinations
- [design/IMPLEMENTATION_SUMMARY.md](design/IMPLEMENTATION_SUMMARY.md) - Full implementation summary
- [design/STEALTH.md](design/STEALTH.md) - Stealth and anti-detection considerations
- [design/SPEECH_DESIGN.md](design/SPEECH_DESIGN.md) - Speech synthesis farbling
- [design/TESTING_PER_CONTEXT_API.md](design/TESTING_PER_CONTEXT_API.md) - Testing guide

---

## Example: Adding a New Farbling Feature

**Task:** Add canvas fingerprinting protection

**Steps:**

1. **Research:** Read `stealth_docs/design/MASTER_SEED_OVERRIDE.md` to understand token derivation

2. **Find the target:** Locate canvas rendering code in Chromium source:
   ```
   third_party/blink/renderer/modules/canvas/canvas2d/
   ```

3. **Create override:**
   ```bash
   mkdir -p src/brave/chromium_src/third_party/blink/renderer/modules/canvas/canvas2d/
   ```

4. **Implement farbling:**
   ```cpp
   // In chromium_src override file
   #define getImageData getImageData_ChromiumImpl
   #include "src/third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.cc"
   #undef getImageData

   ImageData* CanvasRenderingContext2D::getImageData(...) {
     ImageData* data = getImageData_ChromiumImpl(...);
     // Apply farbling using master seed token
     FarbleImageData(data, GetCanvasFarblingToken());
     return data;
   }
   ```

5. **Build and test:**
   ```bash
   npm run build
   ../out/Component/Brave\ Browser.app/Contents/MacOS/Brave\ Browser
   ```

6. **Document:** Add `CANVAS.md` to `stealth_docs/` explaining the implementation

---

## Resources

- [Brave Browser Issues](https://github.com/brave/brave-browser/issues)
- [Brave Releases](https://github.com/brave/brave-browser/releases)
- [Brave Core Docs](https://github.com/brave/brave-core/blob/master/docs/README.md)
- [Brave Wiki](https://github.com/brave/brave-browser/wiki)

---

**Last Updated:** 2026-02-21
