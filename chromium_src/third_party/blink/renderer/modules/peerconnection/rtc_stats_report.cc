/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "src/third_party/blink/renderer/modules/peerconnection/rtc_stats_report.cc"

namespace blink {

namespace {

// Helper function to mask IP addresses in getStats() API
// This prevents WebRTC IP leaks through the stats reporting mechanism
String MaskStatsIpAddress(const std::string& address) {
  // Convert to String for processing
  String addr = String::FromUTF8(address);

  // Preserve mDNS addresses (*.local)
  if (addr.EndsWith(".local")) {
    return addr;
  }

  // Mask all other IP addresses to 0.0.0.0 (IPv4) or :: (IPv6)
  if (addr.Contains(':')) {
    return "::";  // IPv6
  } else {
    return "0.0.0.0";  // IPv4
  }
}

}  // namespace

// Override the ToV8Stat function for RTCIceCandidateStats
// This intercepts the stats conversion to mask IP addresses before they reach JavaScript
RTCIceCandidateStats* ToV8Stat(
    ScriptState* script_state,
    const webrtc::RTCIceCandidateStats& webrtc_stat) {
  RTCIceCandidateStats* v8_stat =
      MakeGarbageCollected<RTCIceCandidateStats>(script_state->GetIsolate());

  SET_STAT(webrtc_stat.transport_id, v8_stat->setTransportId);

  // Mask IP addresses to prevent leaks
  if (webrtc_stat.address.has_value()) {
    v8_stat->setAddress(MaskStatsIpAddress(*webrtc_stat.address));
  }

  // Mask port to 0 (could reveal network info)
  if (webrtc_stat.port.has_value()) {
    v8_stat->setPort(0);
  }

  SET_STAT(webrtc_stat.protocol, v8_stat->setProtocol);
  SET_STAT_ENUM(webrtc_stat.candidate_type, v8_stat->setCandidateType,
                V8RTCIceCandidateType);
  SET_STAT(webrtc_stat.priority, v8_stat->setPriority);
  SET_STAT(webrtc_stat.url, v8_stat->setUrl);
  SET_STAT_ENUM(webrtc_stat.relay_protocol, v8_stat->setRelayProtocol,
                V8RTCIceServerTransportProtocol);
  SET_STAT(webrtc_stat.foundation, v8_stat->setFoundation);

  // Mask related IP addresses (STUN/TURN reflexive addresses)
  if (webrtc_stat.related_address.has_value()) {
    v8_stat->setRelatedAddress(MaskStatsIpAddress(*webrtc_stat.related_address));
  }

  // Mask related port
  if (webrtc_stat.related_port.has_value()) {
    v8_stat->setRelatedPort(0);
  }

  SET_STAT(webrtc_stat.username_fragment, v8_stat->setUsernameFragment);
  SET_STAT_ENUM(webrtc_stat.tcp_type, v8_stat->setTcpType,
                V8RTCIceTcpCandidateType);
  SET_STAT_ENUM(webrtc_stat.network_type, v8_stat->setNetworkType,
                V8RTCNetworkType);

  // Non-standard and obsolete stats - mask IP field
  SET_STAT(webrtc_stat.is_remote, v8_stat->setIsRemote);
  if (webrtc_stat.ip.has_value()) {
    v8_stat->setIp(MaskStatsIpAddress(*webrtc_stat.ip));
  }

  return v8_stat;
}

}  // namespace blink
