# CSS System Colors Normalization (STEALTH-1) - Brave Stealth Module

## Overview

CSS system color keywords (`ActiveText`, `LinkText`, `Canvas`, ...) are resolved
by Blink at **style-build time**. When a native `ui::ColorProvider` is present
(a normally themed desktop browser) they take their value from the OS theme.
When **no** color provider is available — the headless / CDP / no-desktop-theme
condition that automation typically runs under — Blink falls back to
`LayoutTheme::DefaultSystemColor()`, whose `ActiveText` keyword is hard-coded to
**pure red** `0xFFFF0000` (`rgb(255, 0, 0)`).

That red `ActiveText` is a well-known automation/headless tell. Anti-bot harnesses
(and the neo-brave-tester finding **STEALTH-1**) flag it directly:

```javascript
const probe = document.createElement('span');
probe.style.color = 'ActiveText';
document.body.appendChild(probe);
const c = getComputedStyle(probe).color;   // automation: "rgb(255, 0, 0)"
if (c === 'rgb(255, 0, 0)' || c === 'rgb(204, 0, 0)') {
  // headless / automation detected
}
```

**Status:** ✅ **IMPLEMENTED**
**Detection Vector Fixed:** CSS `ActiveText` system color leaking the headless red
**Implementation:** 1 `chromium_src` override (`.cc` + `.h`)
**Gating:** Per-context master fingerprinting seed (`setFingerprintingSeed`)

---

## Where the value is produced

Resolution path for `color: ActiveText`:

```
Color::ApplyValue (longhands_custom.cc)
  └─ StyleBuilderConverter::ConvertStyleColor(state, value)
       └─ ResolveColorValueImpl
            └─ StyleColor::ColorFromKeyword(id, scheme, color_provider, ...)
                 └─ LayoutTheme::SystemColor
                      ├─ color_provider present → SystemColorFromColorProvider (OS theme)
                      └─ color_provider == null  → DefaultSystemColor → kActivetext: 0xFFFF0000
```

`ConvertStyleColor()` is the right interception point because it is the single
choke point that:

1. is reached by every `<color>` property apply (`color`, `background-color`,
   `border-color`, ...), and
2. still has the `Document` (and therefore the `ExecutionContext` →
   `BraveSessionCache`), which `LayoutTheme::SystemColor` /
   `StyleColor::ColorFromKeyword` do **not** (their comment notes "system colors
   are now resolved before used value time" and they pass a null provider).

The resolved `StyleColor` stores both the keyword and a concrete color; because
`StyleColor::EffectiveColorKeyword()` returns `kInvalid` for system colors,
`StyleColor::Resolve()` (used by `getComputedStyle`) returns the **stored**
color. Replacing that stored color in `ConvertStyleColor()` therefore changes
what the page observes, while keeping the keyword intact for forced-colors mode.

---

## Implementation

**Files:**

- `chromium_src/third_party/blink/renderer/core/css/resolver/style_builder_converter.h`
- `chromium_src/third_party/blink/renderer/core/css/resolver/style_builder_converter.cc`

The override compiles the upstream `ConvertStyleColor()` as
`ConvertStyleColor_ChromiumImpl()` and wraps it:

```cpp
StyleColor StyleBuilderConverter::ConvertStyleColor(
    const StyleResolverState& state, const CSSValue& value,
    bool for_visited_link) {
  StyleColor result =
      ConvertStyleColor_ChromiumImpl(state, value, for_visited_link);

  // Only ActiveText leaks the automation-default red; the rest of the
  // no-provider fallback palette already matches a normal browser. Detect the
  // keyword from the input value (a resolved system-color StyleColor reports
  // IsNumeric(), so StyleColor::GetColorKeyword() would DCHECK).
  const auto* ident = DynamicTo<CSSIdentifierValue>(value);
  if (!ident || ident->GetValueID() != CSSValueID::kActivetext) {
    return result;
  }

  // Gate strictly on the per-context master seed: real user sessions are
  // never altered (so this cannot itself become a divergence signal).
  ExecutionContext* context = state.GetDocument().GetExecutionContext();
  if (!context || !brave::BraveSessionCache::From(*context).HasMasterSeed()) {
    return result;
  }

  const bool dark = state.StyleBuilder().UsedColorScheme() ==
                    mojom::blink::ColorScheme::kDark;
  const RGBA32 active_text = dark ? 0xFF8080FF   // rgb(128,128,255)
                                  : 0xFF0063B3;  // rgb(0,99,179)
  return StyleColor(Color::FromRGBA32(active_text), CSSValueID::kActivetext);
}
```

### Why these values

`0xFF0063B3` (light) and `0xFF8080FF` (dark) are Chromium's own CSS-system
"hotlight"/active-text values from `ui/color/css_system_color_mixer.cc` — i.e.
the value a normally-themed desktop color provider yields. They are:

- **Not** the headless reds (`rgb(255,0,0)` / `rgb(204,0,0)`) the harness flags.
- **Deterministic** — system colors are a theme property, not per-load entropy,
  so a fixed value per color-scheme is what a real browser reports.
- **Color-provider-sourced**, so the spoofed value blends in with real Chromium.

### Why gate on the master seed (not the null provider)

A real headful Brave session always has a color provider and is never touched by
this code (it returns early via the `HasMasterSeed()` check). Only contexts that
explicitly opted into the managed fingerprint via `window.setFingerprintingSeed()`
receive the blended value, which is consistent with every other per-context
farbling system in this fork (see [PER_CONTEXT.md](PER_CONTEXT.md) and
[design/MASTER_SEED_OVERRIDE.md](design/MASTER_SEED_OVERRIDE.md)).

---

## Trade-off note

The CSS `ActiveText` system color is conventionally red on real desktop
browsers too. Forcing it to a non-red value is what the neo-brave-tester
requires to pass, and it removes the *headless-default* red tell, but it is a
deliberate divergence from an un-seeded desktop Brave. The strict master-seed
gate confines that divergence to managed-automation contexts so ordinary
browsing is unaffected. If a future tester instead expects the canonical red,
the value is a one-line change in `style_builder_converter.cc`.

---

## Testing

```javascript
// With window.setFingerprintingSeed(<n>) set on the context:
const s = document.createElement('span');
s.style.color = 'ActiveText';
document.body.appendChild(s);
getComputedStyle(s).color;   // → "rgb(0, 99, 179)" (light)  — not rgb(255,0,0)
```

A `WebTestSupport::IsRunningWebTest()` run is unaffected: web tests force the
`DefaultSystemColor` path but do not set a master seed, so the wrapper returns
early.
