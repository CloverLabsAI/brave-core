/* Copyright (c) 2025 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

#define IncrementCounter(...)                                              \
  IncrementCounter(__VA_ARGS__);                                           \
  if (RuntimeEnabledFeatures::BraveIsInTorContextEnabled()) {              \
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,  \
                                      "RTCPeerConnection is not allowed"); \
    return;                                                                \
  }

// Mask IP addresses in ICE candidate error events to prevent leaks
#define DidFailICECandidate DidFailICECandidate_ChromiumImpl
#define BRAVE_DID_FAIL_ICE_CANDIDATE_DECLARED

#include <third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.cc>

#undef IncrementCounter
#undef DidFailICECandidate

#ifdef BRAVE_DID_FAIL_ICE_CANDIDATE_DECLARED

namespace blink {

namespace {

// Helper function to mask IP addresses in error events
String MaskErrorIpAddress(const String& address) {
  if (address.IsNull() || address.empty()) {
    return address;
  }

  // Preserve mDNS addresses (*.local)
  if (address.EndsWith(".local")) {
    return address;
  }

  // Mask all other IP addresses
  if (address.Contains(':')) {
    return "::";  // IPv6
  } else {
    return "0.0.0.0";  // IPv4
  }
}

// Helper to mask host candidate string (full candidate line with IP)
String MaskHostCandidate(const String& host_candidate) {
  if (host_candidate.IsNull() || host_candidate.empty()) {
    return host_candidate;
  }

  // If already mDNS, return as-is
  if (host_candidate.Contains(".local")) {
    return host_candidate;
  }

  // Parse and mask IP in candidate string
  // Format: candidate:<foundation> <component> <protocol> <priority> <IP> <port> ...
  Vector<String> parts;
  host_candidate.Split(' ', parts);

  if (parts.size() >= 6) {
    String original_ip = parts[4];
    if (!original_ip.EndsWith(".local")) {
      parts[4] = original_ip.Contains(':') ? "::" : "0.0.0.0";

      StringBuilder masked_candidate;
      for (wtf_size_t i = 0; i < parts.size(); i++) {
        if (i > 0) masked_candidate.Append(' ');
        masked_candidate.Append(parts[i]);
      }
      return masked_candidate.ToString();
    }
  }

  return host_candidate;
}

}  // namespace

// Override DidFailICECandidate to mask IP addresses before creating the error event
void RTCPeerConnection::DidFailICECandidate(const String& address,
                                            std::optional<uint16_t> port,
                                            const String& host_candidate,
                                            const String& url,
                                            int error_code,
                                            const String& error_text) {
  DCHECK(!closed_);
  DCHECK(GetExecutionContext()->IsContextThread());

  // Mask IP addresses and host candidate before dispatching error event
  String masked_address = MaskErrorIpAddress(address);
  std::optional<uint16_t> masked_port = port.has_value() ? std::optional<uint16_t>(0) : std::nullopt;
  String masked_host_candidate = MaskHostCandidate(host_candidate);

  MaybeDispatchEvent(RTCPeerConnectionIceErrorEvent::Create(
      masked_address, masked_port, masked_host_candidate, url, error_code, error_text));
}

}  // namespace blink

#undef BRAVE_DID_FAIL_ICE_CANDIDATE_DECLARED
#endif
