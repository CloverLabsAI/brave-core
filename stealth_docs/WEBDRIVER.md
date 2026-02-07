# WebDriver Detection Prevention - Brave Stealth Module

## Overview

This module ensures the `navigator.webdriver` property always returns `false` instead of `true`, making automated browser control (Selenium, Puppeteer, Playwright, etc.) undetectable. Brave headless and automated browsing instances appear identical to normal user-controlled browsers.

**Status:** ✅ **FULLY IMPLEMENTED**
**Detection Method:** Uses Chromium's default `false` value (no longer removing property)
**Maintenance:** Low (single-file override, no patches required)

---

## Table of Contents

1. [Detection Vector](#detection-vector)
2. [Implementation](#implementation)
3. [Technical Details](#technical-details)
4. [Testing](#testing)
5. [Architecture](#architecture)
6. [Git History](#git-history)

---

## Detection Vector

### navigator.webdriver Property

**JavaScript API:**
```javascript
// Standard detection method used by anti-bot systems
if (navigator.webdriver === true) {
  console.log("Automated browser detected!");
  // Block access, show CAPTCHA, etc.
}
```

**Chromium Default Behavior:**
- When browser is launched with `--enable-automation` flag (Selenium, Puppeteer, etc.)
- `navigator.webdriver` returns `true`
- Without the flag, it returns `false`
- This is a W3C WebDriver standard requirement
- Intended for legitimate automation testing, but abused for bot detection

**Brave's Approach:**
- `navigator.webdriver` **ALWAYS** returns `false` (via patch override)
- Property exists and is accessible (not `undefined`)
- Even with `--enable-automation` flag, **always** returns `false` (never `true`)
- Patch removes Chromium's automation detection logic entirely
- Most stealthy approach: property behaves like a normal non-automated browser

**Detection Impact:**
- 🔴 **CRITICAL** - Instant detection of automation
- Used by Cloudflare, PerimeterX, DataDome, and other anti-bot services
- Cannot be overridden from JavaScript (property is non-configurable)
- Single most reliable automation detection signal

---

## Implementation

### Files Modified

**1. Patch file (forces webdriver to always return false):**
```
src/brave/patches/third_party-blink-renderer-core-frame-navigator.cc.patch
```

**2. IDL member installer (no longer filters webdriver - allows it to install normally):**
```
src/brave/chromium_src/third_party/blink/renderer/platform/bindings/idl_member_installer.cc
```

### How It Works

Brave uses a **two-part approach** to ensure `navigator.webdriver` always returns `false`:

**Part 1: Patch `navigator.cc` to force `false` return value**

The patch file replaces Chromium's automation detection logic with a hardcoded `return false;`:

```diff
 bool Navigator::webdriver() const {
-  if (RuntimeEnabledFeatures::AutomationControlledEnabled())
-    return true;
-
-  bool automation_enabled = false;
-  probe::ApplyAutomationOverride(GetExecutionContext(), automation_enabled);
-  return automation_enabled;
+  return false;
 }
```

**What this does:**
- Removes the check for `RuntimeEnabledFeatures::AutomationControlledEnabled()`
- Removes the `probe::ApplyAutomationOverride()` call
- **ALWAYS** returns `false`, regardless of flags or settings
- Even with `--enable-automation`, `--headless`, or any automation flags → still `false`

**Part 2: Allow property to install normally (no longer filter it out)**

Previously, Brave was blocking the webdriver property from being installed in JavaScript, making it `undefined`. As of 2026-02-07, we removed that blocking logic from `idl_member_installer.cc`, allowing the property to exist normally.

**Result:**
1. Property exists on `navigator` object (not `undefined`)
2. Property always returns `false` (patch ensures this)
3. Behaves exactly like a normal non-automated browser
4. Cannot be detected via `'webdriver' in navigator` check

**Previous Approach (Deprecated):**
- Used chromium_src override to skip webdriver attribute installation
- Made `navigator.webdriver === undefined`
- Could be detected by checking `'webdriver' in navigator === false`

---

## Technical Details

### Code Implementation

#### Part 1: Patch File (navigator.cc)

**File:** `src/brave/patches/third_party-blink-renderer-core-frame-navigator.cc.patch`

```diff
diff --git a/third_party/blink/renderer/core/frame/navigator.cc b/third_party/blink/renderer/core/frame/navigator.cc
index f8e3bb7712d31..a7f05a4924781 100644
--- a/third_party/blink/renderer/core/frame/navigator.cc
+++ b/third_party/blink/renderer/core/frame/navigator.cc
@@ -105,12 +105,7 @@ bool Navigator::cookieEnabled() const {
 }

 bool Navigator::webdriver() const {
-  if (RuntimeEnabledFeatures::AutomationControlledEnabled())
-    return true;
-
-  bool automation_enabled = false;
-  probe::ApplyAutomationOverride(GetExecutionContext(), automation_enabled);
-  return automation_enabled;
+  return false;
 }
```

**What this patch does:**
- **Removes:** Chromium's logic that checks if automation is enabled
- **Removes:** The `RuntimeEnabledFeatures::AutomationControlledEnabled()` check (set by `--enable-automation`)
- **Removes:** The `probe::ApplyAutomationOverride()` DevTools protocol override
- **Adds:** Hardcoded `return false;` statement
- **Result:** Method **ALWAYS** returns `false`, no matter what flags or settings

#### Part 2: IDL Member Installer (idl_member_installer.cc)

**File:** `src/brave/chromium_src/third_party/blink/renderer/platform/bindings/idl_member_installer.cc`

**Note:** The webdriver-blocking code has been REMOVED as of 2026-02-07. The file now only handles `navigator.connection` filtering.

```cpp
namespace {

bool IsConnectionConfig(const IDLMemberInstaller::AttributeConfig& config) {
  constexpr std::string_view kConnection = "connection";
  return kConnection == config.property_name;
}

}  // namespace
```

**Result:** `navigator.webdriver` is installed normally and calls the patched `Navigator::webdriver()` method which always returns `false`.

#### Template Specialization 1: Template-based Installation

```cpp
template <>
PLATFORM_EXPORT void IDLMemberInstaller::BraveInstallAttributes<
    BraveNavigatorAttributeInstallerTrait>(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Template> instance_template,
    v8::Local<v8::Template> prototype_template,
    v8::Local<v8::Template> interface_template,
    v8::Local<v8::Signature> signature,
    const char* interface_name,
    base::span<const AttributeConfig> configs) {
  const bool connection_attribute_enabled = base::FeatureList::IsEnabled(
      blink::features::kNavigatorConnectionAttribute);
  for (const auto& config : configs) {
    if (!connection_attribute_enabled && IsConnectionConfig(config)) {
      continue;  // Skip connection property if disabled
    }
    // webdriver attribute is NO LONGER filtered - installs normally
    InstallAttribute(isolate, world, instance_template, prototype_template,
                     interface_template, signature, interface_name, config);
  }
}
```

**When This Runs:**
- During V8 context initialization
- Before any JavaScript executes
- When Navigator interface template is being set up

#### Template Specialization 2: Object-based Installation

```cpp
template <>
PLATFORM_EXPORT void IDLMemberInstaller::BraveInstallAttributes<
    BraveNavigatorAttributeInstallerTrait>(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance_object,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Object> interface_object,
    v8::Local<v8::Signature> signature,
    const char* interface_name,
    base::span<const AttributeConfig> configs) {
  const bool connection_attribute_enabled = base::FeatureList::IsEnabled(
      blink::features::kNavigatorConnectionAttribute);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (const auto& config : configs) {
    if (!connection_attribute_enabled && IsConnectionConfig(config)) {
      continue;  // Skip connection property if disabled
    }
    // webdriver attribute is NO LONGER filtered - installs normally
    InstallAttribute(isolate, context, world, instance_object, prototype_object,
                     interface_object, signature, interface_name, config);
  }
}
```

**When This Runs:**
- During runtime attribute installation
- When JavaScript contexts are created dynamically
- Handles cases where Navigator object is accessed after initialization

### Comparison with Other Attributes

**navigator.connection (filtered):**
- Filtered via `IsConnectionConfig()` when feature flag is disabled
- Network Information API (reveals connection type, bandwidth)
- Disabled by default for privacy

**navigator.webdriver (NOT filtered - updated 2026-02-07):**
- No longer filtered in IDL member installer
- Installs normally and returns `false` (Chromium default)
- Property exists and behaves like a normal non-automated browser

---

## Testing

### Detection Test Script

```javascript
async function testWebDriverDetection() {
  console.log('=== WebDriver Detection Test ===');

  // Test 1: navigator.webdriver property value
  console.log('navigator.webdriver:', navigator.webdriver);
  console.log('Expected: false');
  console.log('Result:', navigator.webdriver === false ? '✅ PASS' : '❌ FAIL');

  // Test 2: Type check
  console.log('typeof navigator.webdriver:', typeof navigator.webdriver);
  console.log('Expected: "boolean"');
  console.log('Result:', typeof navigator.webdriver === 'boolean' ? '✅ PASS' : '❌ FAIL');

  // Test 3: 'webdriver' in navigator check
  console.log("'webdriver' in navigator:", 'webdriver' in navigator);
  console.log('Expected: true');
  console.log('Result:', ('webdriver' in navigator) ? '✅ PASS' : '❌ FAIL');

  // Test 4: Property descriptor check
  const descriptor = Object.getOwnPropertyDescriptor(Navigator.prototype, 'webdriver');
  console.log('Property descriptor exists:', descriptor !== undefined);
  console.log('Expected: true (property should exist)');
  console.log('Result:', descriptor !== undefined ? '✅ PASS' : '❌ FAIL');

  // Test 5: Not true (the key test)
  console.log('navigator.webdriver !== true:', navigator.webdriver !== true);
  console.log('Expected: true');
  console.log('Result:', navigator.webdriver !== true ? '✅ PASS' : '❌ FAIL');

  return {
    propertyValue: navigator.webdriver,
    propertyExists: 'webdriver' in navigator,
    typeOf: typeof navigator.webdriver,
    descriptor: descriptor
  };
}

// Run test
testWebDriverDetection().then(results => {
  if (results.propertyValue === false &&
      results.propertyExists === true &&
      results.typeOf === 'boolean' &&
      results.descriptor !== undefined) {
    console.log('\n✅ ALL TESTS PASSED - WebDriver returns false (undetectable)');
  } else {
    console.log('\n❌ TESTS FAILED - WebDriver detection still possible');
  }
});
```

### Expected Results

**Brave with fix (2026-02-07):**
```
navigator.webdriver: false
Expected: false
Result: ✅ PASS

typeof navigator.webdriver: "boolean"
Expected: "boolean"
Result: ✅ PASS

'webdriver' in navigator: true
Expected: true
Result: ✅ PASS

Property descriptor exists: true
Expected: true (property should exist)
Result: ✅ PASS

navigator.webdriver !== true: true
Expected: true
Result: ✅ PASS

✅ ALL TESTS PASSED - WebDriver returns false (undetectable)
```

**Standard Chromium with --enable-automation:**
```
navigator.webdriver: true
Expected: false
Result: ❌ FAIL

navigator.webdriver !== true: false
Expected: true
Result: ❌ FAIL
```

**Standard Chromium WITHOUT --enable-automation:**
```
navigator.webdriver: false
Expected: false
Result: ✅ PASS

(Identical to Brave - this is the goal!)
```

### Real-World Detection Examples

#### Cloudflare Bot Detection
```javascript
// Cloudflare checks for webdriver property
if (navigator.webdriver) {
  // Trigger challenge or block
}
```

#### PerimeterX / HUMAN Security
```javascript
// Multi-layered detection
const signals = {
  webdriver: navigator.webdriver,
  automation: window.navigator.webdriver === true,
  // ... other signals
};
```

#### DataDome
```javascript
// Part of fingerprinting suite
const automationSignals = [
  'webdriver' in navigator,
  navigator.webdriver === true,
  // ... 50+ other checks
];
```

---

## Architecture

### Design Decisions

**Why chromium_src override instead of patch?**

✅ **Advantages:**
1. **Cleaner integration** - No patch maintenance when Chromium updates
2. **Compile-time replacement** - Chromium's build system automatically uses our version
3. **Type safety** - Full C++ template validation
4. **No file conflicts** - Chromium build system handles include path priority
5. **Easier to extend** - Can add more property filters (e.g., `navigator.plugins`, etc.)

❌ **Disadvantages:**
1. Requires understanding of Chromium's IDL system
2. Must maintain template specializations
3. Less obvious than a patch file

**Why not JavaScript-based removal?**

The `navigator.webdriver` property is installed **before any JavaScript executes**, and is defined as **non-configurable** per W3C WebDriver spec. Attempting to delete or override it from JavaScript fails:

```javascript
// These all fail silently or throw errors:
delete navigator.webdriver;  // Returns false
navigator.webdriver = undefined;  // TypeError: Cannot set property
Object.defineProperty(navigator, 'webdriver', {value: undefined});  // TypeError
```

The only reliable approach is to **prevent installation at the C++ level**.

### Integration with Brave Features

**Compatibility:**
- ✅ Works with Brave Shields
- ✅ Works with fingerprinting protection
- ✅ Works in Private/Tor windows
- ✅ Works with all Brave modes (headless, headful, automation)
- ✅ No user configuration required

**Other Navigator Properties Also Filtered:**
1. `navigator.connection` (Network Information API)
   - Reveals connection type, bandwidth, RTT
   - Privacy concern: fingerprinting via network characteristics
   - Filtered via `IsConnectionConfig()` when feature flag disabled

---

## Git History

### Latest Change: 2026-02-07 - Return `false` Instead of `undefined`

**Date:** 2026-02-07
**Author:** Brave Team
**Message:** `fix: navigator.webdriver should return false, not undefined`

**Changes:**
```diff
 namespace {

 bool IsConnectionConfig(const IDLMemberInstaller::AttributeConfig& config) {
   constexpr std::string_view kConnection = "connection";
   return kConnection == config.property_name;
 }

-bool IsWebdriverConfig(const IDLMemberInstaller::AttributeConfig& config) {
-  constexpr std::string_view kWebdriver = "webdriver";
-  return kWebdriver == config.property_name;
-}

 }  // namespace

 template <>
 PLATFORM_EXPORT void IDLMemberInstaller::BraveInstallAttributes<...> {
   for (const auto& config : configs) {
     if (!connection_attribute_enabled && IsConnectionConfig(config)) {
       continue;
     }
-    if (IsWebdriverConfig(config)) {
-      continue;
-    }
     InstallAttribute(...);  // Now installs webdriver normally
   }
 }
```

**Impact:**
- `navigator.webdriver` now returns `false` (Chromium default) instead of being `undefined`
- Property exists and behaves exactly like a normal non-automated browser
- More stealthy: no detection via `'webdriver' in navigator === false`
- Passes all compatibility checks

**Rationale:**
- Previous approach (making property `undefined`) could be detected
- Sites could check: `'webdriver' in navigator === false` to detect removal
- New approach is indistinguishable from a normal browser without automation flags

---

### Previous Commit: 00f4fed3751a088e87d3e18205fa518ad593b88d (Deprecated)

**Date:** 2026-02-04
**Author:** Nirupam Bhowmick <jishu.nirupam@gmail.com>
**Message:** `fix: webdriver override` (DEPRECATED - see 2026-02-07 update)

**Changes:**
- Added `IsWebdriverConfig()` to filter out webdriver attribute
- Made `navigator.webdriver === undefined`
- This approach has been superseded by the 2026-02-07 fix

---

## Related Stealth Modules

This module is part of Brave's comprehensive anti-detection system:

1. **WEBDRIVER.md** (this document) - Remove navigator.webdriver property
2. **HEADLESS.md** - Normalize headless browser fingerprints
3. **WEBRTC.md** - Mask IP addresses in WebRTC APIs
4. **User-Agent** - Consistent user-agent strings
5. **Canvas Fingerprinting** - Brave Shields farbling
6. **WebGL Fingerprinting** - Brave Shields farbling

---

## Future Enhancements

### Potential Additions (Optional)

These could be added using the same chromium_src override pattern:

1. **navigator.plugins (deprecated but still checked)**
   ```javascript
   // Current: Returns empty PluginArray in modern browsers
   // Potential: Could remove property entirely for consistency
   ```

2. **navigator.languages consistency**
   ```javascript
   // Current: May reveal preferred languages
   // Enhancement: Normalize to system default only
   ```

3. **navigator.hardwareConcurrency**
   ```javascript
   // Current: Returns actual CPU core count
   // Privacy: Fingerprinting vector (normalize to common value like 4 or 8)
   ```

4. **navigator.deviceMemory**
   ```javascript
   // Current: Returns actual RAM in GB
   // Privacy: Strong fingerprinting vector (normalize to common value)
   ```

**Implementation Note:** All of these could be added to `idl_member_installer.cc` with additional helper functions like `IsPluginsConfig()`, `IsHardwareConcurrencyConfig()`, etc.

---

## References

### W3C Standards

- [W3C WebDriver Specification](https://w3c.github.io/webdriver/#interface)
- Section 11.2: "If the endpoint is not available, send a session not created error."
- Section 14.1: "The `webdriver` attribute MUST be set to true when the user agent is under remote control."

### Chromium Source

- Original file: `src/third_party/blink/renderer/platform/bindings/idl_member_installer.cc`
- IDL attribute installation: `InstallAttribute()` function
- Navigator interface definition: `src/third_party/blink/renderer/modules/navigatorcontentutils/navigator_content_utils.idl`

### Anti-Bot Services Documentation

- [Cloudflare Bot Management](https://developers.cloudflare.com/bots/)
- [PerimeterX Bot Detection](https://www.humansecurity.com/products/bot-defender)
- [DataDome Bot Detection](https://datadome.co/bot-management/)

---

## Maintenance Notes

**When Chromium Updates:**
1. Check if `idl_member_installer.cc` function signatures changed
2. Verify `AttributeConfig` structure remains compatible
3. Test that template specializations still compile
4. Run detection test script to verify property still removed

**Debugging:**
- If `navigator.webdriver` reappears, check that override is being compiled
- Verify `BraveInstallAttributes` templates are being called (add logging)
- Check that `IsWebdriverConfig()` string comparison matches Chromium's property name

**Known Stable Since:**
- Chromium 144.0 (current)
- No breaking changes expected (stable IDL system)

---

**Last Updated:** 2026-02-07
**Status:** ✅ Production-ready (Updated: returns `false` not `undefined`)
**Maintenance:** Low (minimal override, uses Chromium defaults)
