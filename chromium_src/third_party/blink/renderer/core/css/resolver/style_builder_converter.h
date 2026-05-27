/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_BUILDER_CONVERTER_H_
#define BRAVE_CHROMIUM_SRC_THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_BUILDER_CONVERTER_H_

// STEALTH-1: CSS system-color spoofing.
//
// Blink resolves CSS system colors (ActiveText, LinkText, ...) at style-build
// time inside StyleBuilderConverter::ConvertStyleColor(). When no native
// ui::ColorProvider is available (the headless / CDP / no-desktop-theme
// condition that automation runs under), LayoutTheme::DefaultSystemColor()
// returns a bare fallback palette whose `ActiveText` keyword is pure red
// (0xFFFF0000 == rgb(255,0,0)). That value is a well-known automation/headless
// tell. We add a Brave wrapper around ConvertStyleColor() that, for a context
// with a per-context master fingerprinting seed, replaces the resolved
// ActiveText color with a realistic, color-provider-sourced value so the page
// blends in with a normally-themed desktop browser.
//
// We declare a forwarding `_ChromiumImpl` alongside the original member so the
// wrapper (defined in the matching .cc) can call straight through to the
// upstream implementation. The two declarations have distinct names, so all
// existing call sites of ConvertStyleColor() keep compiling unchanged.
#define ConvertStyleColor(...)            \
  ConvertStyleColor(__VA_ARGS__);         \
  static StyleColor ConvertStyleColor_ChromiumImpl(__VA_ARGS__)

#include <third_party/blink/renderer/core/css/resolver/style_builder_converter.h>  // IWYU pragma: export

#undef ConvertStyleColor

#endif  // BRAVE_CHROMIUM_SRC_THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_STYLE_BUILDER_CONVERTER_H_
