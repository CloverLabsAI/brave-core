/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_fingerprinting_service.h"

#include "content/public/browser/browser_context.h"

namespace {
const char kBraveFingerprintingServiceKey[] = "brave_fingerprinting_service";
}

BraveFingerprintingService::BraveFingerprintingService(
    content::BrowserContext* context) {}

BraveFingerprintingService::~BraveFingerprintingService() = default;

// static
BraveFingerprintingService* BraveFingerprintingService::GetForBrowserContext(
    content::BrowserContext* context) {
  if (!context) {
    return nullptr;
  }

  auto* service = static_cast<BraveFingerprintingService*>(
      context->GetUserData(kBraveFingerprintingServiceKey));

  if (!service) {
    auto new_service = std::make_unique<BraveFingerprintingService>(context);
    service = new_service.get();
    context->SetUserData(kBraveFingerprintingServiceKey,
                         std::move(new_service));
  }

  return service;
}

void BraveFingerprintingService::SetMasterSeed(uint64_t seed) {
  master_seed_ = seed;
}

std::optional<uint64_t> BraveFingerprintingService::GetMasterSeed() const {
  return master_seed_;
}

void BraveFingerprintingService::SetWebRTCIPv4(const std::string& ipv4) {
  webrtc_ipv4_ = ipv4;
}

void BraveFingerprintingService::SetWebRTCIPv6(const std::string& ipv6) {
  webrtc_ipv6_ = ipv6;
}

std::string BraveFingerprintingService::GetWebRTCIPv4() const {
  return webrtc_ipv4_.value_or("0.0.0.0");
}

std::string BraveFingerprintingService::GetWebRTCIPv6() const {
  return webrtc_ipv6_.value_or("::");
}

bool BraveFingerprintingService::HasWebRTCIPOverride() const {
  return webrtc_ipv4_.has_value() || webrtc_ipv6_.has_value();
}
