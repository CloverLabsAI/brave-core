/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_BRAVE_FINGERPRINTING_HOST_H_
#define BRAVE_BROWSER_BRAVE_FINGERPRINTING_HOST_H_

#include "brave/components/brave_fingerprinting/mojom/brave_fingerprinting.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {
class WebContents;
}

// WebContents-associated host for handling fingerprinting override requests
// from renderer processes. Stores overrides in the BrowserContext-scoped service.
class BraveFingerprintingHost
    : public brave::mojom::BraveFingerprintingHost,
      public content::WebContentsObserver,
      public content::WebContentsUserData<BraveFingerprintingHost> {
 public:
  ~BraveFingerprintingHost() override;

  BraveFingerprintingHost(const BraveFingerprintingHost&) = delete;
  BraveFingerprintingHost& operator=(const BraveFingerprintingHost&) = delete;

  void BindReceiver(
      mojo::PendingReceiver<brave::mojom::BraveFingerprintingHost> receiver);

  // brave::mojom::BraveFingerprintingHost:
  void SetMasterSeed(uint64_t seed) override;
  void SetWebRTCIPv4(const std::string& ipv4) override;
  void SetWebRTCIPv6(const std::string& ipv6) override;
  void GetMasterSeed(GetMasterSeedCallback callback) override;
  void GetWebRTCIPOverrides(GetWebRTCIPOverridesCallback callback) override;
  void SetTimezone(const std::string& timezone_id) override;
  void GetTimezone(GetTimezoneCallback callback) override;

 private:
  explicit BraveFingerprintingHost(content::WebContents* web_contents);
  friend class content::WebContentsUserData<BraveFingerprintingHost>;

  mojo::ReceiverSet<brave::mojom::BraveFingerprintingHost> receivers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // BRAVE_BROWSER_BRAVE_FINGERPRINTING_HOST_H_
