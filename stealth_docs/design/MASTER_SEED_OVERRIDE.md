# Master Seed Override - Always Farble When Seed Is Set

## Overview

When `setFingerprintingSeed()` is called, **all farbling systems now activate automatically**, regardless of Brave Shields settings. This ensures consistent, deterministic fingerprinting for per-context control.

## Implementation Date

**2026-02-07**: Added master seed override logic to all farbling systems.

---

## Behavior Change

### Before
Farbling only applied when:
- ✅ Brave Shields enabled
- ✅ Fingerprinting protection set to "Standard" or "Strict"

**Problem:** Even with master seed set, shields could be off → no farbling

### After
Farbling applies when:
- ✅ Master seed is set via `setFingerprintingSeed()` **OR**
- ✅ Brave Shields enabled with fingerprinting protection

**Solution:** Master seed now **overrides** shields settings

---

## Affected Systems

### 1. WebGL Vendor & Renderer
**File:** `webgl_rendering_context_base.cc`

**Logic:**
```cpp
bool AllowFingerprintingForHost(blink::CanvasRenderingContextHost* host) {
  if (!host)
    return true;

  // If master seed is set, always farble (ignore shields settings)
  auto* context = host->GetTopExecutionContext();
  if (context && brave::BraveSessionCache::From(*context).HasMasterSeed())
    return false;  // false = don't allow real fingerprinting = farble!

  return brave::AllowFingerprinting(context,
                                    ContentSettingsType::BRAVE_WEBCOMPAT_WEBGL);
}
```

**Result:**
- Master seed set → Always returns farbled WebGL values
- No master seed → Respects shields settings

---

### 2. Hardware Concurrency
**File:** `navigator_base.cc`

**Logic:**
```cpp
void ApplyBraveHardwareConcurrencyOverride(...) {
  // If master seed is set, always use MAXIMUM farbling level
  BraveFarblingLevel farbling_level = BraveFarblingLevel::OFF;
  if (context && brave::BraveSessionCache::From(*context).HasMasterSeed()) {
    farbling_level = BraveFarblingLevel::MAXIMUM;
  } else {
    farbling_level = brave::GetBraveFarblingLevelFor(...);
  }

  // Apply farbling based on level
  switch (farbling_level) { ... }
}
```

**Result:**
- Master seed set → Always uses MAXIMUM farbling (2-8 cores range)
- No master seed → Respects shields settings

---

### 3. Device Memory
**File:** `navigator_device_memory.cc`

**Logic:**
```cpp
float FarbleDeviceMemory(blink::ExecutionContext* context) {
  // If master seed is set, always use MAXIMUM farbling level
  BraveFarblingLevel farbling_level = BraveFarblingLevel::OFF;
  if (context && BraveSessionCache::From(*context).HasMasterSeed()) {
    farbling_level = BraveFarblingLevel::MAXIMUM;
  } else {
    farbling_level = brave::GetBraveFarblingLevelFor(...);
  }

  // Apply farbling based on level
  if (farbling_level == BraveFarblingLevel::OFF)
    return true_value;
  // ... farble the value
}
```

**Result:**
- Master seed set → Always uses MAXIMUM farbling (0.25-8 GB range)
- No master seed → Respects shields settings

---

## JavaScript API

```javascript
// Set master seed
window.setFingerprintingSeed(12345);

// Now ALL farbling is active, even if shields are OFF:
console.log(navigator.deviceMemory);        // → Farbled (e.g., 4 GB)
console.log(navigator.hardwareConcurrency); // → Farbled (e.g., 6 cores)

// WebGL
const canvas = document.createElement('canvas');
const gl = canvas.getContext('webgl');
const ext = gl.getExtension('WEBGL_debug_renderer_info');
console.log(gl.getParameter(ext.UNMASKED_VENDOR_WEBGL));
// → "Google Inc. (Apple)"
console.log(gl.getParameter(ext.UNMASKED_RENDERER_WEBGL));
// → "ANGLE (Apple, ANGLE Metal Renderer: Apple M2 Pro, ...)"
```

---

## Test Cases

### Test 1: Master Seed with Shields OFF

```javascript
// Turn shields OFF for site
// brave://settings/shields → Disable for this site

// Set master seed
window.setFingerprintingSeed(12345);

// Reload page

// Expected: All values are FARBLED despite shields being off
console.log(navigator.deviceMemory);        // → 4 GB (not real value)
console.log(navigator.hardwareConcurrency); // → 6 cores (not real value)
// WebGL → Farbled profiles
```

### Test 2: Master Seed with Shields ON

```javascript
// Shields ON (default)
window.setFingerprintingSeed(12345);

// Reload page

// Expected: All values are FARBLED (same as Test 1)
console.log(navigator.deviceMemory);        // → 4 GB
console.log(navigator.hardwareConcurrency); // → 6 cores
```

### Test 3: No Master Seed, Shields OFF

```javascript
// Shields OFF, no master seed set

// Expected: All values are REAL (no farbling)
console.log(navigator.deviceMemory);        // → Real device memory
console.log(navigator.hardwareConcurrency); // → Real core count
// WebGL → Real GPU info
```

### Test 4: No Master Seed, Shields ON

```javascript
// Shields ON (default), no master seed

// Expected: All values are FARBLED (normal shields behavior)
console.log(navigator.deviceMemory);        // → Farbled based on shields
console.log(navigator.hardwareConcurrency); // → Farbled based on shields
```

---

## Priority Matrix

| Master Seed | Shields | Result |
|-------------|---------|--------|
| ✅ Set | ✅ ON | **FARBLE** (master seed) |
| ✅ Set | ❌ OFF | **FARBLE** (master seed overrides) |
| ❌ Not Set | ✅ ON | **FARBLE** (shields) |
| ❌ Not Set | ❌ OFF | **REAL VALUES** (no farbling) |

**Rule:** Master seed always wins. If set, farbling is ALWAYS active.

---

## Implementation Details

### HasMasterSeed() Check

All three systems now check if a master seed is set:

```cpp
if (context && brave::BraveSessionCache::From(*context).HasMasterSeed()) {
  // Force farbling to activate
}
```

**Location:** `BraveSessionCache::HasMasterSeed()`
```cpp
bool HasMasterSeed() const { return has_master_seed_; }
```

This flag is set when `SetMasterFingerprintingSeed()` is called from JavaScript.

---

## Rationale

### Why Override Shields?

1. **Consistency:** Master seed API implies "I want full control over fingerprinting"
2. **Predictability:** Users calling `setFingerprintingSeed()` expect it to work regardless of shields
3. **Per-context control:** Allows fine-grained fingerprint management independent of global settings
4. **Testing:** Makes it easier to test farbling without changing shields settings

### Why MAXIMUM Level?

When master seed is set, we use `BraveFarblingLevel::MAXIMUM`:
- **Device Memory:** Full range 0.25-8 GB (6 values)
- **Hardware Concurrency:** Full range 2-8 cores (7 values)
- **WebGL:** All 10 Apple Silicon profiles

This provides the maximum anonymity set while remaining deterministic with the seed.

---

## Files Modified

1. **webgl_rendering_context_base.cc**
   - Updated `AllowFingerprintingForHost()` to check master seed

2. **navigator_base.cc**
   - Updated `ApplyBraveHardwareConcurrencyOverride()` to check master seed

3. **navigator_device_memory.cc**
   - Updated `FarbleDeviceMemory()` to check master seed

---

## Backwards Compatibility

✅ **No breaking changes:**
- Old behavior (shields-based) still works when no master seed is set
- Master seed API is additive, doesn't break existing functionality
- All existing code paths remain functional

---

## Security Considerations

### Potential Concerns

1. **"Can't websites detect master seed by checking if shields are off but farbling is on?"**
   - Answer: No. Websites cannot detect shields state. They only see farbled values.

2. **"Does this weaken shields protection?"**
   - Answer: No. It strengthens per-context control. Shields still work normally when no seed is set.

3. **"Can this be abused?"**
   - Answer: No. The master seed API is already privileged and requires explicit user action.

---

## Example: Complete Flow

```javascript
// 1. User sets master seed
window.setFingerprintingSeed(42);

// 2. Internal: BraveSessionCache marks has_master_seed_ = true

// 3. Page accesses navigator.hardwareConcurrency
//    → ApplyBraveHardwareConcurrencyOverride() called
//    → Checks HasMasterSeed() → TRUE
//    → Uses MAXIMUM farbling level
//    → Returns farbled value (e.g., 6 cores)

// 4. Page accesses navigator.deviceMemory
//    → FarbleDeviceMemory() called
//    → Checks HasMasterSeed() → TRUE
//    → Uses MAXIMUM farbling level
//    → Returns farbled value (e.g., 4 GB)

// 5. Page queries WebGL
//    → getParameter(UNMASKED_VENDOR_WEBGL) called
//    → AllowFingerprintingForHost() checks HasMasterSeed() → TRUE
//    → Returns false (don't allow real fingerprinting)
//    → GetFarbledWebGLVendor() called
//    → Returns "Google Inc. (Apple)"

// Result: Consistent farbled fingerprint with seed 42
```

---

## Total Combinations (with Master Seed)

When master seed is set, all systems use MAXIMUM farbling:

```
Device Memory: 6 values
Hardware Concurrency: 7 values
WebGL Renderer: 10 profiles
WebGL Vendor: 1 value (fixed)

Total: 6 × 7 × 10 = 420 unique combinations
```

Same as before, but now **guaranteed to activate** regardless of shields.

---

## Conclusion

Master seed now **always activates farbling**, making it a true "per-context fingerprint override" system. This ensures:

✅ Deterministic fingerprints when seed is set
✅ Independent of global shields settings
✅ Consistent behavior across all farbling systems
✅ 420 unique, realistic combinations
✅ Backwards compatible with existing shields behavior
