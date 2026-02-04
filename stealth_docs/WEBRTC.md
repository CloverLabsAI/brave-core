# WebRTC IP Leak Prevention - Stealth Module

**Last Updated:** 2026-02-04
**Status:** 🔴 **PARTIALLY PROTECTED** - Critical leaks remain
**Priority:** HIGH

---

## Overview

This document tracks all patches and modifications made to Brave Browser to prevent WebRTC IP address leaks. WebRTC can expose users' real IP addresses even when using VPNs or proxies through multiple JavaScript-accessible APIs.

---

## Current Status

### ✅ Protected Vectors (2/7)

1. **createOffer/createAnswer SDP** - IPs masked in returned promise objects
2. **localDescription.sdp getter** - IPs masked when accessing SDP property

### ❌ Unprotected Vectors (5/7)

3. **getStats() API** - 🔴 **CRITICAL LEAK** - Primary leak vector
4. **onicecandidate event properties** - ⚠️ Partial (address masked, port/url exposed)
5. **icecandidateerror event** - 🔴 **CRITICAL LEAK** - All properties unmasked
6. **RTCIceTransport.getLocalCandidates()** - ⚠️ Same as onicecandidate
7. **toJSON() methods** - ✅ Protected (uses masked getters)

---

## IP Leak Vectors Explained

### Vector 1: createOffer/createAnswer SDP ✅

**How JavaScript accesses:**
```javascript
const offer = await pc.createOffer();
console.log(offer.sdp); // Contains SDP with connection/candidate lines
```

**Leak details:**
- Connection lines: `c=IN IP4 192.168.1.1`
- Candidate lines: `a=candidate:... udp ... 192.168.1.1 5000 typ host`

**Detection code pattern:**
```javascript
const connectionLineIpAddress = ((sdp.match(/(c=IN\s)(.+)\s/ig) || [])[0] || '').trim().split(' ')[2]
const candidateIpAddress = ((sdp.match(/((udp|tcp)\s)((\d|\w)+\s)((\d|\w|(\.|\:))+)(?=\s)/ig) || [])[0] || '').split(' ')[2]
```

### Vector 2: localDescription.sdp ✅

**How JavaScript accesses:**
```javascript
await pc.setLocalDescription(offer);
console.log(pc.localDescription.sdp); // SDP accessed after setting
```

**Leak details:** Same as Vector 1

### Vector 3: getStats() API 🔴 CRITICAL

**How JavaScript accesses:**
```javascript
const stats = await pc.getStats();
stats.forEach(report => {
  if (report.type === 'local-candidate') {
    console.log(report.address);      // LEAKED!
    console.log(report.port);          // LEAKED!
    console.log(report.relatedAddress); // LEAKED!
  }
});
```

**Leak details:**
- Direct access to ICE candidate IP addresses
- Bypasses SDP entirely
- Most reliable detection method (used by BrowserLeaks, ipleak.net)

### Vector 4: onicecandidate Event ⚠️

**How JavaScript accesses:**
```javascript
pc.onicecandidate = (e) => {
  if (e.candidate) {
    console.log(e.candidate.address);      // MASKED ✅
    console.log(e.candidate.port);         // LEAKED! ❌
    console.log(e.candidate.relatedPort);  // LEAKED! ❌
    console.log(e.candidate.url);          // LEAKED! ❌
    console.log(e.candidate.candidate);    // MASKED ✅
  }
};
```

**Leak details:**
- Fires each time an ICE candidate is gathered (after setLocalDescription)
- `address` property masked to `0.0.0.0`
- `port`, `relatedPort`, `url` still expose real values

### Vector 5: icecandidateerror Event 🔴 CRITICAL

**How JavaScript accesses:**
```javascript
pc.addEventListener('icecandidateerror', (e) => {
  console.log(e.address);       // LEAKED!
  console.log(e.port);          // LEAKED!
  console.log(e.hostCandidate); // LEAKED!
  console.log(e.url);           // LEAKED!
});
```

**Leak details:**
- Fires when ICE candidate gathering fails
- Completely unmasked - all IP/port info exposed
- Often triggered when STUN/TURN servers fail

### Vector 6: RTCIceTransport.getLocalCandidates() ⚠️

**How JavaScript accesses:**
```javascript
const iceTransport = pc.getSenders()[0].transport.iceTransport;
const candidates = iceTransport.getLocalCandidates();
candidates.forEach(c => console.log(c.address)); // Same as Vector 4
```

**Leak details:** Returns same RTCIceCandidate objects as onicecandidate

### Vector 7: toJSON() Methods ✅

**How JavaScript accesses:**
```javascript
const json = JSON.stringify(candidate);
// Uses masked candidate() and sdp() getters
```

**Status:** Protected (serializes using masked properties)

---

## Files Modified

### Brave Patch Files (Chromium Overrides)

**Location:** `src/brave/patches/`

All patches are applied to Chromium source during `npm run sync` via Brave's patch system.

| Patch File | Target Chromium File | Size | Lines Modified | Status |
|------------|---------------------|------|----------------|--------|
| `third_party-blink-renderer-modules-peerconnection-rtc_session_description_request_promise_impl.cc.patch` | `src/third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_promise_impl.cc` | 4.3KB | ~100 lines | ✅ Applied |
| `third_party-blink-renderer-modules-peerconnection-rtc_session_description_request_impl.cc.patch` | `src/third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_impl.cc` | 4.2KB | ~100 lines | ✅ Applied |
| `third_party-blink-renderer-modules-peerconnection-rtc_ice_candidate.cc.patch` | `src/third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.cc` | 3.9KB | ~100 lines | ✅ Applied |
| `third_party-blink-renderer-modules-peerconnection-rtc_session_description.cc.patch` | `src/third_party/blink/renderer/modules/peerconnection/rtc_session_description.cc` | 2.6KB | ~50 lines | ✅ Applied |

**Patch Details:**

#### 1. rtc_session_description_request_promise_impl.cc.patch
```
Chromium source: src/third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_promise_impl.cc
Patch location: src/brave/patches/third_party-blink-renderer-modules-peerconnection-rtc_session_description_request_promise_impl.cc.patch
Generated: 2026-02-04

Changes:
- Added MaskSdpIpAddresses() helper function (lines 17-103)
- Added required includes: string_builder.h, vector.h (lines 14-15)
- Modified RequestSucceeded() to mask SDP before setting on RTCSessionDescriptionInit (line 132)

Purpose:
Masks IP addresses in SDP when createOffer() or createAnswer() promises resolve.
This is the PRIMARY patch that fixes the user's original detection script.
```

#### 2. rtc_session_description_request_impl.cc.patch
```
Chromium source: src/third_party/blink/renderer/modules/peerconnection/rtc_session_description_request_impl.cc
Patch location: src/brave/patches/third_party-blink-renderer-modules-peerconnection-rtc_session_description_request_impl.cc.patch
Generated: 2026-02-04

Changes:
- Added MaskSdpIpAddresses() helper function (identical to above)
- Added required includes: string_builder.h, vector.h
- Modified RequestSucceeded() to mask SDP before invoking callback (line 168)

Purpose:
Masks IP addresses for legacy callback-based WebRTC API (same functionality as patch #1).
```

#### 3. rtc_ice_candidate.cc.patch
```
Chromium source: src/third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.cc
Patch location: src/brave/patches/third_party-blink-renderer-modules-peerconnection-rtc_ice_candidate.cc.patch
Generated: 2026-02-04

Changes:
- Modified candidate() getter to mask IP addresses in candidate string (lines 87-120)
- Modified address() getter to return "0.0.0.0" for non-mDNS addresses (lines 151-164)
- Modified relatedAddress() getter to return "0.0.0.0" (lines 186-197)
- Modified toJSONForBinding() to use masked candidate() getter instead of platform getter (line 222)

Purpose:
Masks ICE candidate properties when accessed via onicecandidate event or RTCIceTransport API.
Prevents IP leaks through candidate.address, candidate.relatedAddress, and JSON serialization.
```

#### 4. rtc_session_description.cc.patch
```
Chromium source: src/third_party/blink/renderer/modules/peerconnection/rtc_session_description.cc
Patch location: src/brave/patches/third_party-blink-renderer-modules-peerconnection-rtc_session_description.cc.patch
Generated: 2026-02-04

Changes:
- Modified sdp() getter to mask IP addresses in SDP string (lines 82-129)
- Masks c=IN IP4 connection lines
- Masks c=IN IP6 connection lines
- Preserves mDNS (.local) addresses

Purpose:
Masks IP addresses when accessing pc.localDescription.sdp or pc.remoteDescription.sdp.
Covers SDP access after setLocalDescription().
```

### Pending Patches (Need to Create)

| Patch File | Target Chromium File | Priority | Purpose |
|------------|---------------------|----------|---------|
| `third_party-blink-renderer-modules-peerconnection-rtc_stats_report.cc.patch` | `src/third_party/blink/renderer/modules/peerconnection/rtc_stats_report.cc` | 🔴 CRITICAL | Mask IPs in getStats() API (lines 486-515) |
| `third_party-blink-renderer-modules-peerconnection-rtc_peer_connection.cc.patch` | `src/third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.cc` | 🔴 HIGH | Mask icecandidateerror event (lines 2351-2361) |
| `third_party-blink-renderer-modules-peerconnection-rtc_ice_candidate.cc.patch` (v2) | `src/third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.cc` | 🟡 MEDIUM | Add port/relatedPort/url masking |

### Brave-Core Direct Modifications

**Location:** `src/brave/`

These files are part of Brave's own codebase (not Chromium patches).

| File | Path | Changes | Commit | Status |
|------|------|---------|--------|--------|
| `brave_profile_prefs.cc` | `src/brave/browser/brave_profile_prefs.cc` | Removed WebRTC policy lines 62, 283-287 | e4fe222 | ✅ Applied |

**Details:**

#### brave_profile_prefs.cc
```
Full path: src/brave/browser/brave_profile_prefs.cc
Modified lines: 62, 283-287
Commit: e4fe222 (2026-02-04)

Removed:
- #include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h" (line 62)
- registry->SetDefaultPrefValue(::prefs::kWebRTCIPHandlingPolicy, ...) (lines 283-287)

Reason:
The kWebRTCIPHandlingDisableNonProxiedUdp policy completely blocked WebRTC UDP connections,
breaking functionality. Since we're now masking at the Blink API level, the restrictive
policy is not needed.
```

### Chromium Source Files (Read-Only Reference)

**Location:** `src/third_party/blink/renderer/modules/peerconnection/`

These are the original Chromium files we analyzed (not modified directly, only via patches).

| File | Purpose | Key Functions | Lines Referenced |
|------|---------|---------------|------------------|
| `rtc_peer_connection.cc` | Main RTCPeerConnection implementation | `createOffer()`, `localDescription()`, `DidGenerateICECandidate()`, `DidFailICECandidate()` | 737, 1095, 1745, 2342, 2351 |
| `rtc_stats_report.cc` | getStats() API | `ToV8Stat()` for ICE candidate stats | 486-515 |
| `rtc_ice_candidate.cc` | ICE candidate object | `address()`, `port()`, `candidate()`, `toJSONForBinding()` | 87-120, 151-164, 170-172, 186-197, 220-228 |
| `rtc_session_description.cc` | SDP object | `sdp()` getter, `toJSONForBinding()` | 82-129, 135-141 |
| `rtc_session_description_platform.cc` | Platform SDP wrapper | `Sdp()` getter (returns unmasked SDP) | - |
| `rtc_ice_transport.cc` | ICE transport API | `getLocalCandidates()`, `getRemoteCandidates()` | 162-170 |
| `rtc_peer_connection_ice_error_event.h` | ICE error events | Event properties (address, port, hostCandidate) | - |
| `rtc_peer_connection_ice_event.h` | ICE candidate events | Event creation for onicecandidate | - |

### Other Modified Files (Historical)

These files were modified during development/debugging but are not part of the final stealth module:

| File | Path | Status | Notes |
|------|------|--------|-------|
| `headless_screen.cc.patch` | `src/brave/patches/headless-lib-browser-headless_screen.cc.patch` | ✅ Fixed | Fixed compilation error (display_list_ → display_list()) - unrelated to stealth |
| `idl_member_installer.cc` | `src/brave/chromium_src/third_party/blink/renderer/platform/bindings/idl_member_installer.cc` | ✅ Applied | Webdriver property removal (see WEBDRIVER.md) |
| `dom_window_css.cc.patch` | `src/brave/patches/third_party-blink-renderer-core-css-dom_window_css.cc.patch` | ✅ Applied | CSS.supports() fingerprinting (separate stealth module) |

---

## Implementation Details

### Masking Function (Used in All Patches)

```cpp
String MaskSdpIpAddresses(const String& original_sdp) {
  if (original_sdp.IsNull() || original_sdp.empty()) {
    return original_sdp;
  }

  // Allow mDNS candidates (*.local)
  if (original_sdp.Contains(".local")) {
    return original_sdp;
  }

  String result = original_sdp;

  // Mask IPv4 connection lines: c=IN IP4 x.x.x.x -> c=IN IP4 0.0.0.0
  wtf_size_t pos = 0;
  while ((pos = result.Find("c=IN IP4 ", pos)) != kNotFound) {
    wtf_size_t ip_start = pos + 9;
    wtf_size_t ip_end = result.Find("\n", ip_start);
    if (ip_end == kNotFound) ip_end = result.length();

    String before = result.Substring(0, ip_start);
    String after = result.Substring(ip_end);
    result = before + "0.0.0.0" + after;

    pos = ip_start + 7;
  }

  // Mask IPv6 connection lines: c=IN IP6 x:x -> c=IN IP6 ::
  pos = 0;
  while ((pos = result.Find("c=IN IP6 ", pos)) != kNotFound) {
    wtf_size_t ip_start = pos + 9;
    wtf_size_t ip_end = result.Find("\n", ip_start);
    if (ip_end == kNotFound) ip_end = result.length();

    String before = result.Substring(0, ip_start);
    String after = result.Substring(ip_end);
    result = before + "::" + after;

    pos = ip_start + 2;
  }

  // Mask candidate lines: a=candidate:... <IP> ...
  pos = 0;
  while ((pos = result.Find("a=candidate:", pos)) != kNotFound) {
    wtf_size_t line_end = result.Find("\n", pos);
    if (line_end == kNotFound) line_end = result.length();

    String candidate_line = result.Substring(pos, line_end - pos);
    Vector<String> parts;
    candidate_line.Split(' ', parts);

    if (parts.size() >= 6) {
      String original_ip = parts[4];
      if (!original_ip.EndsWith(".local")) {
        parts[4] = original_ip.Contains(":") ? "::" : "0.0.0.0";

        StringBuilder masked_line;
        for (wtf_size_t i = 0; i < parts.size(); i++) {
          if (i > 0) masked_line.Append(' ');
          masked_line.Append(parts[i]);
        }

        String before = result.Substring(0, pos);
        String after = result.Substring(line_end);
        result = before + masked_line.ToString() + after;
      }
    }

    pos = line_end;
  }

  return result;
}
```

**Key features:**
- Preserves mDNS obfuscated addresses (`.local` hostnames)
- Masks both IPv4 and IPv6 addresses
- Handles both connection lines and candidate attributes
- Used consistently across all SDP-related patches

---

## Code Flow Analysis

### Flow 1: createOffer/createAnswer (Patched ✅)

```
JavaScript: await pc.createOffer()
    ↓
RTCPeerConnection::createOffer() (rtc_peer_connection.cc:737)
    ↓
peer_handler_->CreateOffer() [native WebRTC]
    ↓
RTCSessionDescriptionRequestPromiseImpl::RequestSucceeded()
    ↓
String masked_sdp = MaskSdpIpAddresses(platform_session_description->Sdp())
    ↓
description->setSdp(masked_sdp) [Line 132]
    ↓
Promise resolves with RTCSessionDescriptionInit
    ↓
JavaScript receives: offer.sdp = "c=IN IP4 0.0.0.0\na=candidate:... 0.0.0.0 ..."
```

**Patch location:** `rtc_session_description_request_promise_impl.cc:132`

### Flow 2: onicecandidate Event (Partial ⚠️)

```
JavaScript: pc.onicecandidate = (e) => { ... }
    ↓
Native ICE gathering produces candidate
    ↓
RTCPeerConnection::DidGenerateICECandidate() (rtc_peer_connection.cc:2342)
    ↓
RTCIceCandidate::Create(platform_candidate)
    ↓
RTCPeerConnectionIceEvent::Create(ice_candidate)
    ↓
MaybeDispatchEvent(event)
    ↓
JavaScript access: e.candidate.address → RTCIceCandidate::address() getter
    ↓
Returns "0.0.0.0" for non-mDNS (rtc_ice_candidate.cc:151-164) ✅
    ↓
JavaScript access: e.candidate.port → RTCIceCandidate::port() getter
    ↓
Returns raw port number (rtc_ice_candidate.cc:170-172) ❌ LEAKED
```

**Patch location:** `rtc_ice_candidate.cc:151-164` (address only)
**Missing:** Port/relatedPort/url masking

### Flow 3: getStats() API (Unpatched 🔴)

```
JavaScript: await pc.getStats()
    ↓
RTCPeerConnection::getStats() (rtc_peer_connection.cc:1745)
    ↓
peer_handler_->GetStats() [native WebRTC]
    ↓
RTCStatsReport created from native stats
    ↓
RTCStatsReport::AddStats() → ToV8Stat() (rtc_stats_report.cc:486)
    ↓
For ICE candidate stats:
  result->addMember("address", String::FromUTF8(address)) [Line 492] ❌ LEAKED
  result->addMember("port", port) [Line 493] ❌ LEAKED
  result->addMember("relatedAddress", String::FromUTF8(related_address)) [Line 502] ❌ LEAKED
    ↓
Promise resolves with RTCStatsReport
    ↓
JavaScript receives: report.address = "192.168.1.1" (REAL IP!)
```

**No patch applied** - This is the PRIMARY LEAK

### Flow 4: icecandidateerror Event (Unpatched 🔴)

```
JavaScript: pc.addEventListener('icecandidateerror', (e) => { ... })
    ↓
ICE candidate gathering fails (e.g., STUN timeout)
    ↓
RTCPeerConnection::DidFailICECandidate() (rtc_peer_connection.cc:2351)
    ↓
Parameters passed directly from native:
  - const String& address ❌ UNMASKED
  - std::optional<uint16_t> port ❌ UNMASKED
  - const String& host_candidate ❌ UNMASKED
    ↓
RTCPeerConnectionIceErrorEvent::Create(address, port, host_candidate, ...)
    ↓
MaybeDispatchEvent(event)
    ↓
JavaScript receives: e.address = "192.168.1.1" (REAL IP!)
```

**No patch applied** - CRITICAL LEAK

---

## Git Commits

### Applied Changes

| Commit Hash | Date | Description |
|-------------|------|-------------|
| `e4fe222` | 2026-02-04 | Fix WebRTC IP leak by masking IPs in toJSONForBinding |
| `355fee1` | 2026-02-04 | Fix WebRTC IP leak in createOffer/createAnswer SDP |

**Commit details:**

#### e4fe222 - toJSONForBinding Fix
```
Fix WebRTC IP leak by masking IPs in toJSONForBinding

The previous approach broke WebRTC functionality by blocking all non-proxied
UDP. The real issue was that toJSONForBinding() in rtc_ice_candidate.cc was
bypassing our masking logic by calling platform_candidate_->Candidate()
directly instead of the masked candidate() method.

This fix:
- Changes toJSONForBinding to use the masked candidate() method
- Removes the restrictive kWebRTCIPHandlingDisableNonProxiedUdp policy
- Keeps WebRTC functional while masking all IPs to 0.0.0.0

Files changed:
- src/brave/browser/brave_profile_prefs.cc
- src/brave/patches/third_party-blink-renderer-modules-peerconnection-rtc_ice_candidate.cc.patch
```

#### 355fee1 - createOffer/createAnswer SDP Fix
```
Fix WebRTC IP leak in createOffer/createAnswer SDP

The previous patches missed the critical code path where createOffer() and
createAnswer() expose SDP to JavaScript. These methods return
RTCSessionDescriptionInit objects with unmasked SDP, completely bypassing
the RTCSessionDescription::sdp() getter that we had patched.

Root cause:
- createOffer/createAnswer call RequestSucceeded() in the request handlers
- RequestSucceeded() directly sets unmasked SDP on RTCSessionDescriptionInit
- JavaScript receives the promise with raw IP addresses in both:
  1. Connection lines (c=IN IP4 x.x.x.x)
  2. Candidate lines (a=candidate:... udp ... <IP> ...)

This fix:
- Added MaskSdpIpAddresses() function to both request handler files
- Masks BOTH connection lines AND candidate lines in SDP
- Applied masking at the point where platform SDP converts to JS object
- Covers both promise-based and callback-based code paths

Files changed:
- src/brave/patches/third_party-blink-renderer-modules-peerconnection-rtc_session_description_request_promise_impl.cc.patch
- src/brave/patches/third_party-blink-renderer-modules-peerconnection-rtc_session_description_request_impl.cc.patch
```

---

## Testing

### Detection Script (User Provided)

```javascript
const getIPAddress = (sdp) => {
  const blocked = '0.0.0.0'
  const candidateEncoding = /((udp|tcp)\s)((\d|\w)+\s)((\d|\w|(\.|\:))+)(?=\s)/ig
  const connectionLineEncoding = /(c=IN\s)(.+)\s/ig

  const connectionLineIpAddress = ((sdp.match(connectionLineEncoding) || [])[0] || '').trim().split(' ')[2]
  if (connectionLineIpAddress && (connectionLineIpAddress != blocked)) {
    return connectionLineIpAddress
  }

  const candidateIpAddress = ((sdp.match(candidateEncoding) || [])[0] || '').split(' ')[2]
  return candidateIpAddress && (candidateIpAddress != blocked) ? candidateIpAddress : undefined
}
```

**Status:** ✅ This specific test now passes (SDP-based detection blocked)
**Issue:** Still leaks via getStats() API which this script doesn't test

### Comprehensive Test Script

```javascript
async function testAllWebRTCLeaks() {
  const results = {
    sdp_createOffer: null,
    sdp_localDescription: null,
    getStats_address: null,
    onicecandidate_address: null,
    onicecandidate_port: null,
    icecandidateerror_address: null
  };

  const pc = new RTCPeerConnection({
    iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
  });
  pc.createDataChannel("");

  // Test 1: SDP in createOffer
  const offer = await pc.createOffer();
  results.sdp_createOffer = extractIPFromSDP(offer.sdp);

  // Test 2: localDescription.sdp
  await pc.setLocalDescription(offer);
  results.sdp_localDescription = extractIPFromSDP(pc.localDescription.sdp);

  // Test 3: getStats() API
  await new Promise(resolve => setTimeout(resolve, 2000)); // Wait for ICE
  const stats = await pc.getStats();
  stats.forEach(report => {
    if (report.type === 'local-candidate' && report.address && report.address !== '0.0.0.0') {
      results.getStats_address = report.address;
    }
  });

  // Test 4: onicecandidate event
  const pc2 = new RTCPeerConnection({ iceServers: [] });
  pc2.createDataChannel("");
  pc2.onicecandidate = (e) => {
    if (e.candidate) {
      if (e.candidate.address && e.candidate.address !== '0.0.0.0') {
        results.onicecandidate_address = e.candidate.address;
      }
      if (e.candidate.port) {
        results.onicecandidate_port = e.candidate.port;
      }
    }
  };
  await pc2.createOffer().then(o => pc2.setLocalDescription(o));

  // Test 5: icecandidateerror event
  pc.addEventListener('icecandidateerror', (e) => {
    if (e.address && e.address !== '0.0.0.0') {
      results.icecandidateerror_address = e.address;
    }
  });

  await new Promise(resolve => setTimeout(resolve, 3000));

  console.log('=== WebRTC Leak Test Results ===');
  console.log('SDP createOffer:', results.sdp_createOffer || '✅ Blocked');
  console.log('SDP localDescription:', results.sdp_localDescription || '✅ Blocked');
  console.log('getStats() address:', results.getStats_address || '✅ Blocked');
  console.log('onicecandidate address:', results.onicecandidate_address || '✅ Blocked');
  console.log('onicecandidate port:', results.onicecandidate_port || '✅ Blocked');
  console.log('icecandidateerror:', results.icecandidateerror_address || '✅ Blocked');

  return results;
}

function extractIPFromSDP(sdp) {
  const ipRegex = /((c=IN IP4 |a=candidate:.+ )((?:\d{1,3}\.){3}\d{1,3}))/;
  const match = sdp.match(ipRegex);
  return match && match[3] !== '0.0.0.0' ? match[3] : null;
}
```

**Expected results after all patches:**
- ✅ All values should be `null` or `'✅ Blocked'`

**Current results:**
- ✅ SDP tests pass
- ❌ getStats() leaks
- ⚠️ onicecandidate port leaks
- ❌ icecandidateerror leaks

---

## Known Issues

### Issue 1: getStats() API Bypass 🔴

**Severity:** CRITICAL
**Affected:** All Chromium-based browsers
**Impact:** Primary detection method used by leak test sites

**Details:**
The `getStats()` API provides direct access to ICE candidate statistics including IP addresses. This completely bypasses SDP-based masking because stats are retrieved directly from the native WebRTC stack without going through the SDP serialization path.

**Fix required:** Patch `rtc_stats_report.cc:486-515` to mask address fields

### Issue 2: Port Number Leaks ⚠️

**Severity:** MEDIUM
**Affected:** RTCIceCandidate properties
**Impact:** Port numbers can help fingerprint networks

**Details:**
While IP addresses are masked to `0.0.0.0`, port numbers are still exposed through:
- `candidate.port`
- `candidate.relatedPort`

**Fix required:** Patch `rtc_ice_candidate.cc` to return dummy port values (e.g., `0`)

### Issue 3: TURN Server URL Leaks ⚠️

**Severity:** LOW-MEDIUM
**Affected:** `candidate.url` property
**Impact:** Can reveal TURN server infrastructure

**Details:**
The `url` property exposes TURN/STUN server addresses which could leak information about the user's WebRTC infrastructure.

**Fix required:** Sanitize or redact `url` property in `rtc_ice_candidate.cc`

### Issue 4: icecandidateerror Unmasked 🔴

**Severity:** CRITICAL
**Affected:** Error event handlers
**Impact:** Complete IP exposure on ICE failures

**Details:**
When ICE candidate gathering fails (common with STUN/TURN timeouts), the error event contains completely unmasked IP addresses, ports, and candidate strings.

**Fix required:** Patch `rtc_peer_connection.cc:2351-2361` to mask error event parameters

---

## Next Steps

### Immediate (CRITICAL)

1. **Patch getStats() API**
   - File: `rtc_stats_report.cc`
   - Function: `ToV8Stat()` at lines 486-515
   - Mask: `address`, `relatedAddress`, `ip` fields to `"0.0.0.0"`
   - Test: Verify BrowserLeaks.com no longer detects IP

2. **Patch icecandidateerror Event**
   - File: `rtc_peer_connection.cc`
   - Function: `DidFailICECandidate()` at lines 2351-2361
   - Mask: `address`, `host_candidate` parameters before creating event
   - Test: Trigger STUN failures and verify no IP leak

### High Priority

3. **Enhance RTCIceCandidate Masking**
   - File: `rtc_ice_candidate.cc`
   - Functions: `port()`, `relatedPort()`, `url()`
   - Mask: Return `0` for ports, sanitize URLs
   - Test: Check `candidate.port` returns masked value

### Optional (Defense in Depth)

4. **Add Tor Context Detection**
   - Check if user is in Tor mode
   - Block WebRTC entirely or force relay-only mode
   - Reference: Firefox Tor Browser implementation

5. **Add Privacy Budget Integration**
   - Use Brave's Privacy Budget system
   - Rate-limit or block getStats() calls from tracking origins
   - Reference: Brave's existing privacy budget implementation

---

## References

### External Resources

- [WebRTC IP Leak Test - BrowserLeaks](https://browserleaks.com/webrtc)
- [WebRTC Specification - W3C](https://w3c.github.io/webrtc-pc/)
- [RTCIceCandidate API - MDN](https://developer.mozilla.org/en-US/docs/Web/API/RTCIceCandidate)
- [WebRTC IP Leak Prevention Guide - 2025](https://www.videosdk.live/developer-hub/webrtc/webrtc-ip-leaks)
- [Firefox WebRTC Privacy Mitigations](https://bugzilla.mozilla.org/show_bug.cgi?id=959893)

### Related Brave Issues

- Check Brave's GitHub for existing WebRTC privacy issues
- Reference Tor mode WebRTC blocking implementation

### Chromium Source Files

All file paths relative to `/Volumes/BuilderOSteroids/GitHub/brave-browser/src/`

| File | Purpose |
|------|---------|
| `third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.cc` | Main RTCPeerConnection implementation |
| `third_party/blink/renderer/modules/peerconnection/rtc_ice_candidate.cc` | ICE candidate object and properties |
| `third_party/blink/renderer/modules/peerconnection/rtc_session_description.cc` | SDP object and getters |
| `third_party/blink/renderer/modules/peerconnection/rtc_stats_report.cc` | getStats() API implementation |
| `third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_ice_error_event.h` | ICE error event object |
| `third_party/blink/renderer/modules/peerconnection/rtc_ice_transport.cc` | ICE transport API |

---

## Changelog

### 2026-02-04
- ✅ Created stealth_docs documentation system
- ✅ Patched createOffer/createAnswer SDP masking (commit 355fee1)
- ✅ Patched toJSONForBinding ICE candidate leak (commit e4fe222)
- ✅ Patched localDescription.sdp getter
- ✅ Removed restrictive WebRTC policy from brave_profile_prefs.cc
- 🔴 Identified getStats() API as primary remaining leak
- 🔴 Identified icecandidateerror event as critical unpatched leak
- ⚠️ Identified port/url properties as minor leaks

### Previous Work
- Initial WebRTC IP masking attempts
- Experimented with `kWebRTCIPHandlingPolicy` (failed - broke functionality)
- Fixed headless_screen.cc compilation error
- Fixed navigator.webdriver property exposure

---

## Build Instructions

After applying patches:

```bash
# Sync dependencies
npm run sync

# Build Brave
npm run build

# Test WebRTC
# Open browser and run comprehensive test script in console
```

Expected build time: ~30-60 minutes (full rebuild)

---

## Notes

- **mDNS Preservation:** All patches preserve mDNS-obfuscated addresses (`.local` hostnames) which are already privacy-protecting
- **IPv6 Support:** Masking works for both IPv4 (`0.0.0.0`) and IPv6 (`::`)
- **VPN Compatibility:** Masking happens at browser level, doesn't interfere with actual WebRTC connections
- **Trickle ICE:** Candidates are gathered asynchronously after `setLocalDescription()`, so event-based leaks happen over time
- **STUN Server Bypass:** Even with IP masking, STUN servers are still contacted (network-level leak outside browser scope)

---

**End of Document**
