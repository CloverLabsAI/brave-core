/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_BRAVE_FINGERPRINTING_SERVICE_H_
#define BRAVE_BROWSER_BRAVE_FINGERPRINTING_SERVICE_H_

#include <optional>
#include <string>

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

// Browser context-scoped service for storing fingerprinting overrides
// that persist across navigations and page reloads.
class BraveFingerprintingService : public KeyedService,
                                    public base::SupportsUserData::Data {
 public:
  explicit BraveFingerprintingService(content::BrowserContext* context);
  ~BraveFingerprintingService() override;

  BraveFingerprintingService(const BraveFingerprintingService&) = delete;
  BraveFingerprintingService& operator=(const BraveFingerprintingService&) =
      delete;

  // Get the service instance for a given browser context
  static BraveFingerprintingService* GetForBrowserContext(
      content::BrowserContext* context);

  // Master seed management
  void SetMasterSeed(uint64_t seed);
  std::optional<uint64_t> GetMasterSeed() const;
  bool HasMasterSeed() const { return master_seed_.has_value(); }

  // WebRTC IP override management
  void SetWebRTCIPv4(const std::string& ipv4);
  void SetWebRTCIPv6(const std::string& ipv6);
  std::string GetWebRTCIPv4() const;
  std::string GetWebRTCIPv6() const;
  bool HasWebRTCIPOverride() const;

  // Timezone override management
  void SetTimezone(const std::string& timezone_id);
  std::string GetTimezone() const;
  bool HasTimezoneOverride() const { return timezone_id_.has_value(); }

 private:
  std::optional<uint64_t> master_seed_;
  std::optional<std::string> webrtc_ipv4_;
  std::optional<std::string> webrtc_ipv6_;
  std::optional<std::string> timezone_id_;
};

#endif  // BRAVE_BROWSER_BRAVE_FINGERPRINTING_SERVICE_H_
