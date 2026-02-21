# WebGL Farbling - Profile-Based System

## Overview

Brave's WebGL farbling system now uses **10 realistic Apple Silicon GPU profiles** instead of random 8-character strings. This provides better fingerprinting resistance while maintaining plausible deniability.

## Implementation Date

**2026-02-07**: Migrated from random string generation to profile-based farbling system.

---

## Profiles

### GL_VENDOR (Fixed)
All profiles return the same vendor string:
```
Google Inc. (Apple)
```

This matches the format that Chrome/ANGLE returns on macOS with Apple Silicon.

### GL_RENDERER (10 Variants)

The system randomly selects one of these 10 profiles using the master seed:

1. `ANGLE (Apple, ANGLE Metal Renderer: Apple M1 Pro, Unspecified Version)`
2. `ANGLE (Apple, ANGLE Metal Renderer: Apple M1, Unspecified Version)`
3. `ANGLE (Apple, ANGLE Metal Renderer: Apple M2 Pro, Unspecified Version)`
4. `ANGLE (Apple, ANGLE Metal Renderer: Apple M2, Unspecified Version)`
5. `ANGLE (Apple, ANGLE Metal Renderer: Apple M3 Max, Unspecified Version)`
6. `ANGLE (Apple, ANGLE Metal Renderer: Apple M3 Pro, Unspecified Version)`
7. `ANGLE (Apple, ANGLE Metal Renderer: Apple M3, Unspecified Version)`
8. `ANGLE (Apple, ANGLE Metal Renderer: Apple M4 Pro, Unspecified Version)`
9. `ANGLE (Apple, ANGLE Metal Renderer: Apple M4, Unspecified Version)`
10. `ANGLE (Apple, ANGLE Metal Renderer: Apple M5, Unspecified Version)`

---

## How It Works

### 1. **Profile Selection Algorithm**

```cpp
FarblingPRNG prng = MakePseudoRandomGenerator(FarbleKey::kWebGLRenderer);
size_t profile_index = prng() % 10;  // Select one of 10 profiles
```

### 2. **Deterministic Selection**

The PRNG is seeded with:
- **Master seed** (if set via `setFingerprintingSeed()`)
- **eTLD+1 domain** (for cross-site isolation)
- **FarbleKey::kWebGLRenderer** (specific to this farbling context)

### 3. **Token Derivation**

```
Token = HMAC-SHA256(master_seed, eTLD+1)
Profile = profiles[PRNG(Token, FarbleKey::kWebGLRenderer) % 10]
```

---

## Key Features

### ✅ **Deterministic**
Same master seed + same domain → same profile every time

### ✅ **Per-Context Isolation**
- Same seed, different domains → different profiles (eTLD+1 salting)
- Different contexts can have independent fingerprints

### ✅ **Realistic Profiles**
All profiles are real Apple Silicon GPUs that exist in the wild:
- M1, M1 Pro (2020-2021)
- M2, M2 Pro (2022-2023)
- M3, M3 Pro, M3 Max (2023-2024)
- M4, M4 Pro (2024-2025)
- M5 (future-proofing)

### ✅ **No Hardcoded Validation**
Chromium/ANGLE doesn't validate GPU strings against any list—they trust OS/driver values

---

## Master Seed Integration

### JavaScript API

```javascript
// Set master seed for this context
window.setFingerprintingSeed(12345);

// Query WebGL
const canvas = document.createElement('canvas');
const gl = canvas.getContext('webgl');
const ext = gl.getExtension('WEBGL_debug_renderer_info');

console.log(gl.getParameter(ext.UNMASKED_VENDOR_WEBGL));
// → "Google Inc. (Apple)"

console.log(gl.getParameter(ext.UNMASKED_RENDERER_WEBGL));
// → "ANGLE (Apple, ANGLE Metal Renderer: Apple M2 Pro, Unspecified Version)"
```

### Consistency Test

```javascript
// Test 1: Set seed 12345
window.setFingerprintingSeed(12345);
// Reload page → Profile X

// Test 2: Set same seed again
window.setFingerprintingSeed(12345);
// Reload page → Profile X (same as before)

// Test 3: Different seed
window.setFingerprintingSeed(67890);
// Reload page → Profile Y (different from X)
```

---

## Implementation Files

### 1. **FarbleKey Enum**
**File:** `src/brave/third_party/blink/renderer/core/farbling/brave_session_cache.h`

```cpp
enum FarbleKey : uint64_t {
  // ... existing keys
  kWebGLVendor,      // NEW
  kWebGLRenderer,    // NEW
  kKeyCount
};
```

### 2. **Profile Selection Methods**
**File:** `src/brave/third_party/blink/renderer/core/farbling/brave_session_cache.h`

```cpp
blink::String GetFarbledWebGLVendor();
blink::String GetFarbledWebGLRenderer();
```

### 3. **Profile Implementation**
**File:** `src/brave/third_party/blink/renderer/core/farbling/brave_session_cache.cc`

```cpp
blink::String BraveSessionCache::GetFarbledWebGLVendor() {
  return "Google Inc. (Apple)";
}

blink::String BraveSessionCache::GetFarbledWebGLRenderer() {
  static constexpr const char* kWebGLRendererProfiles[] = {
    // ... 10 profiles
  };
  FarblingPRNG prng = MakePseudoRandomGenerator(FarbleKey::kWebGLRenderer);
  return blink::String(kWebGLRendererProfiles[prng() % 10]);
}
```

### 4. **WebGL Hook**
**File:** `src/brave/chromium_src/third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.cc`

```cpp
#define BRAVE_WEBGL_GET_PARAMETER_UNMASKED_RENDERER \
  if (ExtensionEnabled(kWebGLDebugRendererInfoName) && \
      !AllowFingerprintingForHost(Host())) \
    return WebGLAny(script_state, \
        brave::BraveSessionCache::From( \
            *(Host()->GetTopExecutionContext())) \
            .GetFarbledWebGLRenderer());
```

---

## Statistics

### Profile Distribution

With a uniform random distribution across seeds:
- **Each profile: ~10% probability**
- **Total combinations: 10 profiles**

This is significantly fewer than the previous system (62^8 = 218 trillion combinations), but:
1. ✅ More realistic (all are real GPUs)
2. ✅ Harder to detect as fake
3. ✅ Still provides meaningful anonymity set

### Comparison to Previous System

| Aspect | Old System (Random Strings) | New System (Profiles) |
|--------|---------------------------|----------------------|
| **Vendor** | Random 8-char string | `"Google Inc. (Apple)"` |
| **Renderer** | Random 8-char string | 10 realistic profiles |
| **Combinations** | 62^8 = 218 trillion | 10 |
| **Detectability** | Easy (obviously fake) | Hard (real GPUs) |
| **Plausibility** | Low | High |

---

## Why This Approach?

### 1. **No Validation in Chromium**
Research shows Chromium/ANGLE **never validates** GPU strings against hardcoded lists:
- Vendor/renderer come from Metal API at runtime (`MTLDevice.name`)
- Browser trusts whatever the OS/driver returns
- No allowlist/blocklist of valid GPU names

### 2. **Realistic Distribution**
Using real Apple Silicon GPUs means:
- Fingerprints blend into real user population
- Harder for sites to detect farbling
- More plausible deniability

### 3. **Future-Proof**
Includes M4 and M5 (unreleased as of 2024) to ensure longevity

---

## Testing

See `WEBGL_FARBLING_TEST.html` for interactive tests.

### Manual Test

```bash
# Open Brave with shields enabled
brave --enable-features=BraveShields

# Open test page
open WEBGL_FARBLING_TEST.html

# Check console
# Should show one of 10 Apple Silicon profiles
```

### Test with Master Seed

```javascript
// In console
window.setFingerprintingSeed(12345);
location.reload();

// Check GL_RENDERER - should be deterministic
const canvas = document.createElement('canvas');
const gl = canvas.getContext('webgl');
const ext = gl.getExtension('WEBGL_debug_renderer_info');
console.log(gl.getParameter(ext.UNMASKED_RENDERER_WEBGL));
```

---

## Cross-Site Isolation

The system uses **eTLD+1 normalization** to ensure subdomains get the same profile:

```
example.com      → Token_A → Profile 3
www.example.com  → Token_A → Profile 3 (same)
api.example.com  → Token_A → Profile 3 (same)
different.org    → Token_B → Profile 7 (different)
```

This prevents cross-site tracking while maintaining consistent fingerprints within the same site.

---

## References

- [ANGLE Metal Renderer Implementation](src/third_party/angle/src/libANGLE/renderer/metal/DisplayMtl.mm)
- [Chromium GPU Info Collector](src/gpu/config/gpu_info_collector.cc)
- [ANGLE SystemInfo](src/third_party/angle/src/gpu_info_util/SystemInfo.h)
- [Previous Analysis](STEALTH.md) - Original random string approach

---

## Git History

- **2026-02-07**: Implemented profile-based WebGL farbling with 10 Apple Silicon variants
- **Previous**: Used random 8-character alphanumeric strings
