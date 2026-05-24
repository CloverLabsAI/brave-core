/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "third_party/blink/renderer/core/css/media_values.h"

#include "brave/third_party/blink/renderer/core/farbling/brave_session_cache.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "ui/display/screen_infos.h"

// When a per-context fingerprinting seed is set (window.setFingerprintingSeed),
// the JS `screen.width`/`screen.height` getters report a deterministic,
// seed-selected screen profile via ChromeClient::BraveGetScreenInfos() (see
// chromium_src .../core/frame/screen.cc and .../core/page/chrome_client_impl.cc).
// The CSS `device-width`/`device-height` media features are computed here and
// historically farbled off the *viewport* width, which no longer matches the
// seed-selected screen profile. That mismatch both leaks the real screen width
// through CSS and is itself a Brave-detection signal (screen.width !=
// device-width). When a master seed is present, mirror the exact farbled
// screen rect so the JS Screen API and the CSS media features stay in lockstep.
#define CalculateDeviceWidth(...)                                              \
  CalculateDeviceWidth(__VA_ARGS__, bool early) {                              \
    ExecutionContext* context = frame->DomWindow()->GetExecutionContext();     \
    if (context &&                                                             \
        brave::BraveSessionCache::From(*context).HasMasterSeed()) {            \
      return frame->GetChromeClient()                                          \
          .BraveGetScreenInfos(*frame)                                         \
          .current()                                                           \
          .rect.width();                                                       \
    }                                                                          \
    auto* top_frame = DynamicTo<LocalFrame>(frame->Top());                     \
    return top_frame && brave::BlockScreenFingerprinting(context, early)       \
               ? brave::FarbleInteger(context,                                 \
                                      brave::FarbleKey::kWindowInnerWidth,     \
                                      CalculateViewportWidth(top_frame), 0, 8) \
               : CalculateDeviceWidth_ChromiumImpl(frame);                     \
  }                                                                            \
  int MediaValues::CalculateDeviceWidth_ChromiumImpl(__VA_ARGS__)

#define CalculateDeviceHeight(...)                                         \
  CalculateDeviceHeight(__VA_ARGS__, bool early) {                         \
    ExecutionContext* context = frame->DomWindow()->GetExecutionContext(); \
    if (context &&                                                         \
        brave::BraveSessionCache::From(*context).HasMasterSeed()) {        \
      return frame->GetChromeClient()                                      \
          .BraveGetScreenInfos(*frame)                                     \
          .current()                                                       \
          .rect.height();                                                  \
    }                                                                      \
    auto* top_frame = DynamicTo<LocalFrame>(frame->Top());                 \
    return top_frame && brave::BlockScreenFingerprinting(context, early)   \
               ? brave::FarbleInteger(                                     \
                     context, brave::FarbleKey::kWindowInnerHeight,        \
                     CalculateViewportHeight(top_frame), 0, 8)             \
               : CalculateDeviceHeight_ChromiumImpl(frame);                \
  }                                                                        \
  int MediaValues::CalculateDeviceHeight_ChromiumImpl(__VA_ARGS__)

#include <third_party/blink/renderer/core/css/media_values.cc>

#undef CalculateDeviceWidth
#undef CalculateDeviceHeight
