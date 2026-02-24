// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_THIRD_PARTY_BLINK_RENDERER_MODULES_BRAVE_FINGERPRINTING_OVERRIDE_H_
#define BRAVE_THIRD_PARTY_BLINK_RENDERER_MODULES_BRAVE_FINGERPRINTING_OVERRIDE_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class LocalDOMWindow;
class ScriptState;

// Per-context fingerprinting seed and WebRTC IP override
// Provides one-time use JavaScript API for setting:
// - Master fingerprinting seed (affects canvas, audio, screen coordinates)
// - WebRTC IPv4 override (replaces 0.0.0.0 default masking)
// - WebRTC IPv6 override (replaces :: default masking)
//
// All functions self-destruct after first call to prevent detection
// by fingerprinting scripts. The functions are installed directly on
// the window object as:
//   - window.setFingerprintingSeed(seed)
//   - window.setWebRTCIPv4(ipv4)
//   - window.setWebRTCIPv6(ipv6)
//
// Implementation connects to BraveSessionCache which provides:
// - Domain-salted seed derivation using HMAC-SHA256
// - Per-ExecutionContext storage for overrides
// - Automatic cache invalidation on seed changes
class MODULES_EXPORT FingerprintingOverride final {
  STATIC_ONLY(FingerprintingOverride);

 public:
  // Set master fingerprinting seed for this ExecutionContext
  // Seeds are domain-salted using HMAC-SHA256(seed, eTLD+1)
  // One-time use: deletes itself from window after first call
  static void setFingerprintingSeed(ScriptState* script_state,
                                    LocalDOMWindow& window,
                                    uint64_t seed);

  // Set custom WebRTC IPv4 address for this ExecutionContext
  // Overrides default 0.0.0.0 masking
  // One-time use: deletes itself from window after first call
  static void setWebRTCIPv4(ScriptState* script_state,
                            LocalDOMWindow& window,
                            const String& ipv4);

  // Set custom WebRTC IPv6 address for this ExecutionContext
  // Overrides default :: masking
  // One-time use: deletes itself from window after first call
  static void setWebRTCIPv6(ScriptState* script_state,
                            LocalDOMWindow& window,
                            const String& ipv6);

 public:
  // Helper methods for V8 callbacks (non-IDL)
  static void SendSeedToBrowser(ExecutionContext* context, uint64_t seed);
  static void SendIPv4ToBrowser(ExecutionContext* context, const String& ipv4);
  static void SendIPv6ToBrowser(ExecutionContext* context, const String& ipv6);
  static void SendTimezoneToBrowser(ExecutionContext* context,
                                    const String& timezone_id);

 private:
  // Helper to remove function from window object after first call
  static void SelfDestruct(ScriptState* script_state, const String& function_name);
};

}  // namespace blink

#endif  // BRAVE_THIRD_PARTY_BLINK_RENDERER_MODULES_BRAVE_FINGERPRINTING_OVERRIDE_H_
