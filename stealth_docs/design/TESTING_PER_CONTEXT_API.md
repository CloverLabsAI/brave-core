# Testing Per-Context Fingerprinting API

## Overview

This document explains how to test the per-context fingerprinting seed and WebRTC IP override functionality.

## JavaScript API

The following functions are exposed on `window`:

```javascript
window.setFingerprintingSeed(seed)  // seed: uint64 number
window.setWebRTCIPv4(ipv4)          // ipv4: string like "192.0.2.1"
window.setWebRTCIPv6(ipv6)          // ipv6: string like "2001:db8::1"
```

**Important**: Each function can only be called ONCE per page load. After the first call, the function deletes itself from the window object.

## Testing WebRTC IP Override

### Step 1: Open Browser Console

Open Brave browser and navigate to a test page (or about:blank).

### Step 2: Verify Functions Exist

```javascript
console.log(typeof window.setWebRTCIPv4);  // Should print "function"
console.log(typeof window.setWebRTCIPv6);  // Should print "function"
```

### Step 3: Set Custom IPs BEFORE Creating WebRTC Connection

```javascript
// IMPORTANT: Call these BEFORE creating RTCPeerConnection
window.setWebRTCIPv4("192.0.2.100");
window.setWebRTCIPv6("2001:db8::cafe");

// Verify functions self-destructed
console.log(typeof window.setWebRTCIPv4);  // Should print "undefined"
console.log(typeof window.setWebRTCIPv6);  // Should print "undefined"
```

### Step 4: Create WebRTC Connection and Check IPs

```javascript
const pc = new RTCPeerConnection({
  iceServers: []
});

pc.onicecandidate = (event) => {
  if (event.candidate) {
    console.log("ICE Candidate:", event.candidate.candidate);
    console.log("Address:", event.candidate.address);
    console.log("Related Address:", event.candidate.relatedAddress);

    // Should see 192.0.2.100 for IPv4 and 2001:db8::cafe for IPv6
    // instead of 0.0.0.0 or ::
  }
};

// Create offer to trigger ICE gathering
pc.createOffer().then(offer => {
  console.log("SDP:", offer.sdp);
  // Should see 192.0.2.100 in c= and a=candidate lines
  return pc.setLocalDescription(offer);
});
```

### Step 5: Verify IP Replacement

Check the console output:
- ICE candidate strings should contain `192.0.2.100` instead of `0.0.0.0`
- IPv6 candidates should contain `2001:db8::cafe` instead of `::`
- SDP should show custom IPs in `c=IN IP4` lines
- SDP should show custom IPs in `a=candidate:` lines

## Testing Fingerprinting Seed

### Step 1: Set Seed BEFORE Fingerprinting

```javascript
// Must call before any canvas/audio farbling operations
window.setFingerprintingSeed(12345n);  // Note: use BigInt notation

// Verify function self-destructed
console.log(typeof window.setFingerprintingSeed);  // Should print "undefined"
```

### Step 2: Test Canvas Fingerprinting

```javascript
const canvas = document.createElement('canvas');
canvas.width = 100;
canvas.height = 100;
const ctx = canvas.getContext('2d');

ctx.fillStyle = 'rgb(255, 0, 0)';
ctx.fillRect(0, 0, 100, 100);
ctx.fillStyle = 'rgb(0, 255, 0)';
ctx.fillRect(10, 10, 80, 80);

// Get fingerprint
const dataUrl = canvas.toDataURL();
console.log("Canvas fingerprint:", dataUrl);

// On same domain with same seed, this should be identical
// On different domain or different seed, it will be different
```

### Step 3: Test Reproducibility

1. Reload the page
2. Call `window.setFingerprintingSeed(12345n)` again with the SAME seed
3. Run the same canvas test
4. The fingerprint should be IDENTICAL to step 2

### Step 4: Test Domain Salting

1. Navigate to a different domain (e.g., example.com)
2. Call `window.setFingerprintingSeed(12345n)` with the SAME seed
3. Run the same canvas test
4. The fingerprint should be DIFFERENT (because seed is domain-salted)

## Common Issues

### Issue 1: Functions Return "undefined"

**Problem**: `typeof window.setWebRTCIPv4 === "undefined"`

**Cause**: One of the following:
1. Functions not compiled into build (check build output)
2. Already called once (functions self-destruct)
3. Page loaded before build completed

**Solution**:
1. Rebuild with `npm run build`
2. Reload page (hard refresh with Cmd+Shift+R or Ctrl+Shift+R)
3. Check if functions exist IMMEDIATELY after page load

### Issue 2: WebRTC Still Shows 0.0.0.0

**Problem**: ICE candidates still show `0.0.0.0` instead of custom IP

**Possible Causes**:
1. **Called functions AFTER creating RTCPeerConnection** - Must call BEFORE
2. **ExecutionContext mismatch** - Ensure same browsing context
3. **Build issue** - Patches not applied correctly

**Debug Steps**:

```javascript
// Add debug logging to check if override is set
const pc = new RTCPeerConnection({
  iceServers: []
});

// This should trigger the WebRTC code path
pc.createOffer().then(offer => {
  console.log("=== FULL SDP ===");
  console.log(offer.sdp);
  console.log("================");

  // Search for IP addresses in SDP
  const ipv4Matches = offer.sdp.match(/\d+\.\d+\.\d+\.\d+/g);
  console.log("IPv4 addresses found:", ipv4Matches);

  return pc.setLocalDescription(offer);
});
```

### Issue 3: Fingerprint Not Changing

**Problem**: Canvas fingerprint is same even with different seeds

**Possible Causes**:
1. **Farbling disabled** - Check Brave Shields settings
2. **Seed not being applied** - Check BraveSessionCache
3. **Called after fingerprinting** - Must call BEFORE canvas operations

**Solution**:
1. Check Shields are enabled (brave://settings/shields)
2. Set fingerprinting protection to "Standard" or "Strict"
3. Reload page and set seed BEFORE any canvas operations

## Verification Checklist

- [ ] Functions exist on window object after page load
- [ ] Functions self-destruct after first call
- [ ] Custom WebRTC IPv4 appears in ICE candidates
- [ ] Custom WebRTC IPv6 appears in ICE candidates
- [ ] Custom IPs appear in SDP c= lines
- [ ] Custom IPs appear in SDP a=candidate lines
- [ ] Same seed produces same fingerprint on same domain
- [ ] Same seed produces different fingerprint on different domain
- [ ] Different seeds produce different fingerprints

## Example Complete Test Script

```javascript
// Complete test script - paste into console
(async function testPerContextAPI() {
  console.log("=== Testing Per-Context API ===");

  // Test 1: Check functions exist
  console.log("1. Functions exist:", {
    setFingerprintingSeed: typeof window.setFingerprintingSeed,
    setWebRTCIPv4: typeof window.setWebRTCIPv4,
    setWebRTCIPv6: typeof window.setWebRTCIPv6
  });

  // Test 2: Set overrides
  console.log("2. Setting overrides...");
  window.setFingerprintingSeed(999999999n);
  window.setWebRTCIPv4("198.51.100.42");
  window.setWebRTCIPv6("2001:db8::42");

  // Test 3: Verify self-destruct
  console.log("3. After self-destruct:", {
    setFingerprintingSeed: typeof window.setFingerprintingSeed,
    setWebRTCIPv4: typeof window.setWebRTCIPv4,
    setWebRTCIPv6: typeof window.setWebRTCIPv6
  });

  // Test 4: WebRTC
  console.log("4. Testing WebRTC...");
  const pc = new RTCPeerConnection({ iceServers: [] });

  pc.onicecandidate = (event) => {
    if (event.candidate) {
      console.log("ICE Candidate:", event.candidate.candidate);
      if (event.candidate.candidate.includes("198.51.100.42")) {
        console.log("✓ Custom IPv4 found!");
      }
    }
  };

  const offer = await pc.createOffer();
  console.log("SDP contains custom IP:",
    offer.sdp.includes("198.51.100.42"));

  await pc.setLocalDescription(offer);

  // Test 5: Canvas fingerprint
  console.log("5. Testing canvas fingerprint...");
  const canvas = document.createElement('canvas');
  canvas.width = 100;
  canvas.height = 100;
  const ctx = canvas.getContext('2d');
  ctx.fillRect(0, 0, 100, 100);
  const fp = canvas.toDataURL();
  console.log("Canvas FP (first 100 chars):", fp.substring(0, 100));

  console.log("=== Test Complete ===");
})();
```

## Expected Output

When the test script runs successfully, you should see:

```
=== Testing Per-Context API ===
1. Functions exist: {setFingerprintingSeed: "function", setWebRTCIPv4: "function", setWebRTCIPv6: "function"}
2. Setting overrides...
3. After self-destruct: {setFingerprintingSeed: "undefined", setWebRTCIPv4: "undefined", setWebRTCIPv6: "undefined"}
4. Testing WebRTC...
ICE Candidate: candidate:... 198.51.100.42 ...
✓ Custom IPv4 found!
SDP contains custom IP: true
5. Testing canvas fingerprint...
Canvas FP (first 100 chars): data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVU...
=== Test Complete ===
```

## Advanced Testing

### Test in iframe

```javascript
const iframe = document.createElement('iframe');
document.body.appendChild(iframe);

// Each iframe has its own ExecutionContext
iframe.contentWindow.setWebRTCIPv4("203.0.113.1");

// Parent window has different context
window.setWebRTCIPv4("203.0.113.2");

// Each should use their own IP
```

### Test subdomain salting

```javascript
// On www.example.com
window.setFingerprintingSeed(12345n);
// Get canvas fingerprint

// On api.example.com (different subdomain, same eTLD+1)
window.setFingerprintingSeed(12345n);
// Get canvas fingerprint

// Should be IDENTICAL (same eTLD+1 = example.com)
```
