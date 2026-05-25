/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"

#include "brave/third_party/blink/renderer/core/farbling/brave_session_cache.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_keywords.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"

// Compile the upstream implementation as ConvertStyleColor_ChromiumImpl(). The
// matching chromium_src header (style_builder_converter.h) has already declared
// both ConvertStyleColor() and ConvertStyleColor_ChromiumImpl() on the class,
// so renaming the definition here keeps everything in sync. In-file callers of
// ConvertStyleColor() (shadow/auto-color/SVG paint/scrollbar) are likewise
// redirected to the upstream impl; the Brave wrapper below is what every
// out-of-file color property apply (e.g. Color::ApplyValue for the `color`
// property) calls, which is the path the fingerprinting harness probes.
#define ConvertStyleColor ConvertStyleColor_ChromiumImpl
#include <third_party/blink/renderer/core/css/resolver/style_builder_converter.cc>
#undef ConvertStyleColor

namespace blink {

StyleColor StyleBuilderConverter::ConvertStyleColor(
    const StyleResolverState& state,
    const CSSValue& value,
    bool for_visited_link) {
  StyleColor result =
      ConvertStyleColor_ChromiumImpl(state, value, for_visited_link);

  // STEALTH-1: `ActiveText` is the one system-color keyword whose
  // no-color-provider fallback (LayoutTheme::DefaultSystemColor) is the
  // automation-default red rgb(255,0,0). The rest of the fallback palette
  // (canvas/text/link/visited) already matches a normal browser, so leave it
  // untouched. Detect the keyword from the input value: a resolved system-color
  // StyleColor reports IsNumeric(), so StyleColor::GetColorKeyword() would
  // DCHECK; the parsed CSSIdentifierValue is the reliable, upstream-style
  // signal.
  const auto* ident = DynamicTo<CSSIdentifierValue>(value);
  if (!ident || ident->GetValueID() != CSSValueID::kActivetext) {
    return result;
  }

  // Gate strictly on the fork's per-context master fingerprinting seed: a real
  // user's session (no seed) is never altered, so this cannot itself become a
  // divergence signal for ordinary browsing. Only contexts that opted into the
  // managed fingerprint (setFingerprintingSeed) get the blended value.
  ExecutionContext* context = state.GetDocument().GetExecutionContext();
  if (!context || !brave::BraveSessionCache::From(*context).HasMasterSeed()) {
    return result;
  }

  // Use Chromium's own CSS-system "hotlight"/active-text values from
  // ui/color/css_system_color_mixer.cc, i.e. the value a normally-themed
  // desktop color provider yields. These are deterministic (system colors are
  // a theme property, not per-load entropy) and, critically, are not the
  // headless red the harness flags.
  const bool dark = state.StyleBuilder().UsedColorScheme() ==
                    mojom::blink::ColorScheme::kDark;
  const RGBA32 active_text = dark ? 0xFF8080FF   // rgb(128,128,255)
                                  : 0xFF0063B3;  // rgb(0,99,179)
  return StyleColor(Color::FromRGBA32(active_text), CSSValueID::kActivetext);
}

}  // namespace blink
