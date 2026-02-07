/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_fingerprinting_host.h"

#include "brave/browser/brave_fingerprinting_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

BraveFingerprintingHost::BraveFingerprintingHost(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BraveFingerprintingHost>(*web_contents) {}

BraveFingerprintingHost::~BraveFingerprintingHost() = default;

void BraveFingerprintingHost::BindReceiver(
    mojo::PendingReceiver<brave::mojom::BraveFingerprintingHost> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void BraveFingerprintingHost::SetMasterSeed(uint64_t seed) {
  auto* browser_context = web_contents()->GetBrowserContext();
  if (!browser_context) {
    return;
  }

  auto* service =
      BraveFingerprintingService::GetForBrowserContext(browser_context);
  if (service) {
    service->SetMasterSeed(seed);
  }
}

void BraveFingerprintingHost::SetWebRTCIPv4(const std::string& ipv4) {
  auto* browser_context = web_contents()->GetBrowserContext();
  if (!browser_context) {
    return;
  }

  auto* service =
      BraveFingerprintingService::GetForBrowserContext(browser_context);
  if (service) {
    service->SetWebRTCIPv4(ipv4);
  }
}

void BraveFingerprintingHost::SetWebRTCIPv6(const std::string& ipv6) {
  auto* browser_context = web_contents()->GetBrowserContext();
  if (!browser_context) {
    return;
  }

  auto* service =
      BraveFingerprintingService::GetForBrowserContext(browser_context);
  if (service) {
    service->SetWebRTCIPv6(ipv6);
  }
}

void BraveFingerprintingHost::GetMasterSeed(GetMasterSeedCallback callback) {
  auto* browser_context = web_contents()->GetBrowserContext();
  if (!browser_context) {
    std::move(callback).Run(false, 0);
    return;
  }

  auto* service =
      BraveFingerprintingService::GetForBrowserContext(browser_context);
  if (!service) {
    std::move(callback).Run(false, 0);
    return;
  }

  bool has_seed = service->HasMasterSeed();
  uint64_t seed = service->GetMasterSeed().value_or(0);

  std::move(callback).Run(has_seed, seed);
}

void BraveFingerprintingHost::GetWebRTCIPOverrides(
    GetWebRTCIPOverridesCallback callback) {
  auto* browser_context = web_contents()->GetBrowserContext();
  if (!browser_context) {
    std::move(callback).Run(false, "0.0.0.0", "::");
    return;
  }

  auto* service =
      BraveFingerprintingService::GetForBrowserContext(browser_context);
  if (!service) {
    std::move(callback).Run(false, "0.0.0.0", "::");
    return;
  }

  bool has_override = service->HasWebRTCIPOverride();
  std::string ipv4 = service->GetWebRTCIPv4();
  std::string ipv6 = service->GetWebRTCIPv6();

  std::move(callback).Run(has_override, ipv4, ipv6);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BraveFingerprintingHost);
