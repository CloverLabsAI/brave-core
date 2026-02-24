/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_THIRD_PARTY_BLINK_RENDERER_MODULES_BRAVE_FINGERPRINTING_OVERRIDES_INSTALLER_H_
#define BRAVE_THIRD_PARTY_BLINK_RENDERER_MODULES_BRAVE_FINGERPRINTING_OVERRIDES_INSTALLER_H_

namespace blink {
class WebLocalFrame;
}

namespace brave {

// Install fingerprinting override JavaScript functions on window
// only if overrides don't already exist for this browser context
void InstallFingerprintingOverrides(blink::WebLocalFrame* web_frame);

}  // namespace brave

#endif  // BRAVE_THIRD_PARTY_BLINK_RENDERER_MODULES_BRAVE_FINGERPRINTING_OVERRIDES_INSTALLER_H_
