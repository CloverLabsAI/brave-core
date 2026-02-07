/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <array>

#include "brave/third_party/blink/renderer/core/farbling/brave_session_cache.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"
#include "ui/gfx/geometry/rect.h"

#include <third_party/blink/renderer/core/page/chrome_client_impl.cc>

namespace blink {

namespace {

// Brave's existing 7-row farbling system (kept for fallback)
constexpr auto allowed_desktop_screen_sizes = std::to_array<const gfx::Size>({
    gfx::Size(1280, 800),
    gfx::Size(1366, 768),
    gfx::Size(1440, 900),
    gfx::Size(1680, 1050),
    gfx::Size(1920, 1080),
    gfx::Size(2560, 1440),
    gfx::Size(3840, 2160),
});

// Custom screen profile for per-context farbling
struct ScreenProfile {
  int width;
  int height;
  float device_pixel_ratio;
  int color_depth;
  int avail_width;
  int avail_height;
};

// Mac screen profiles matching Brave's existing 7-row resolutions
// Covers all common Mac resolutions with realistic DPR and color depth values
constexpr auto kCustomScreenProfiles = std::to_array<const ScreenProfile>({
  {1280, 800, 2.0f, 30, 1280, 800},
  {1366, 768, 2.0f, 24, 1366, 768},
  {1440, 900, 1.0f, 24, 1440, 900},
  {1440, 900, 2.0f, 30, 1440, 900},
  {1680, 1050, 2.0f, 24, 1680, 1050},
  {1680, 1050, 2.0f, 30, 1680, 1050},
  {1920, 1080, 2.0f, 30, 1920, 1080},
  {2560, 1440, 1.0f, 24, 2560, 1440},
  {2560, 1440, 2.0f, 24, 2560, 1440},
  {2560, 1440, 2.0f, 30, 2560, 1440},
  {3840, 2160, 1.0f, 24, 3840, 2160},
  {3840, 2160, 2.0f, 24, 3840, 2160},
});

}  // namespace

const display::ScreenInfos& ChromeClientImpl::BraveGetScreenInfos(
    LocalFrame& frame) const {
  LocalDOMWindow* dom_window = frame.DomWindow();
  if (!dom_window) {
    return GetScreenInfos(frame);
  }
  ExecutionContext* context = dom_window->GetExecutionContext();
  if (!context) {
    return GetScreenInfos(frame);
  }

  // Get BraveSessionCache to check for per-context overrides
  brave::BraveSessionCache& cache = brave::BraveSessionCache::From(*context);

  // PRIORITY 1: Custom profile system (when setFingerprintingSeed is called)
  if (cache.HasMasterSeed()) {
    // Use MakePseudoRandomGenerator to get deterministic seed-based selection
    // This uses custom_farbling_token_ which is already domain-salted via HMAC-SHA256
    brave::FarblingPRNG prng = cache.MakePseudoRandomGenerator(
        brave::FarbleKey::kWindowInnerWidth);

    // Select one of the profiles deterministically
    size_t selected_index = prng() % kCustomScreenProfiles.size();
    const ScreenProfile& profile = kCustomScreenProfiles[selected_index];

    // Build farbled ScreenInfo with all 6 properties
    display::ScreenInfo screen_info = GetScreenInfo(frame);

    // Set screen dimensions
    screen_info.rect = gfx::Rect(profile.width, profile.height);
    screen_info.available_rect = gfx::Rect(profile.avail_width,
                                            profile.avail_height);

    // Set device pixel ratio
    screen_info.device_scale_factor = profile.device_pixel_ratio;

    // Set color depth
    screen_info.depth = profile.color_depth;
    screen_info.depth_per_component = profile.color_depth / 3;  // RGB components

    // Ensure single screen (hide multi-monitor)
    screen_info.is_extended = false;
    screen_info.is_primary = true;

    screen_infos_ = display::ScreenInfos(screen_info);
    return screen_infos_;
  }

  // PRIORITY 2: Brave's existing 7-row system (when farbling is enabled)
  if (!brave::BlockScreenFingerprinting(context)) {
    return GetScreenInfos(frame);  // No farbling
  }

  // Use Brave's existing 7-row farbling logic (unchanged)
  display::ScreenInfo screen_info = GetScreenInfo(frame);
  screen_info.rect = gfx::Rect(allowed_desktop_screen_sizes.back().width(),
                               allowed_desktop_screen_sizes.back().height());
  const int outerWidth = dom_window->outerWidth();
  const int outerHeight = dom_window->outerHeight();
  for (const auto& size : allowed_desktop_screen_sizes) {
    if (size.width() >= outerWidth && size.height() >= outerHeight) {
      screen_info.rect = gfx::Rect(size.width(), size.height());
      break;
    }
  }
  screen_info.available_rect = screen_info.rect;
  screen_info.is_extended = false;
  screen_info.is_primary = false;
  screen_infos_ = display::ScreenInfos(screen_info);
  return screen_infos_;
}

}  // namespace blink
