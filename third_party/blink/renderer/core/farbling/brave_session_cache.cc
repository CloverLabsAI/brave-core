/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/renderer/core/farbling/brave_session_cache.h"

#include <string_view>

#include "base/check.h"
#include "brave/components/brave_fingerprinting/mojom/brave_fingerprinting.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "brave/third_party/blink/renderer/brave_farbling_constants.h"
#include "brave/third_party/blink/renderer/brave_font_whitelist.h"
#include "build/build_config.h"
#include "crypto/hmac.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_list.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "url/url_constants.h"

namespace {

constexpr uint64_t zero = 0;
constexpr double maxUInt64AsDouble = static_cast<double>(UINT64_MAX);

constexpr int kFarbledUserAgentMaxExtraSpaces = 5;

// acceptable letters for generating random strings
constexpr std::string_view kLettersForRandomStrings =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

inline uint64_t lfsr_next(uint64_t v) {
  return ((v >> 1) | (((v << 62) ^ (v << 61)) & (~(~zero << 63) << 62)));
}

// Dynamic iframes without a committed navigation don't have content settings
// rules filled, so we always look for the root frame which has required data
// for shields/farbling to be enabled.
blink::WebContentSettingsClient* GetContentSettingsIfNotEmpty(
    blink::LocalFrame* local_frame) {
  if (!local_frame) {
    return nullptr;
  }

  blink::WebContentSettingsClient* content_settings =
      local_frame->LocalFrameRoot().GetContentSettingsClient();
  if (!content_settings || !content_settings->HasContentSettingsRules()) {
    return nullptr;
  }
  return content_settings;
}

// StorageKey has nonce in 1pes mode and anonymous frames. The nonce is used to
// alter the farbling token.
const blink::BlinkStorageKey* GetStorageKey(blink::ExecutionContext* context) {
  if (!context) {
    return nullptr;
  }

  if (auto* window = blink::DynamicTo<blink::LocalDOMWindow>(context)) {
    return &window->GetStorageKey();
  }

  if (auto* worklet = blink::DynamicTo<blink::WorkletGlobalScope>(context)) {
    if (worklet->IsMainThreadWorkletGlobalScope()) {
      if (auto* frame = worklet->GetFrame()) {
        if (auto* document = frame->DomWindow()) {
          return &document->GetStorageKey();
        }
      }
    }
  }

  return nullptr;
}

}  // namespace

namespace brave {

blink::WebContentSettingsClient* GetContentSettingsClientFor(
    ExecutionContext* context) {
  if (!context) {
    return nullptr;
  }

  // Avoid blocking fingerprinting in WebUI, extensions, etc.
  const blink::String protocol = context->GetSecurityOrigin()
                                     ->GetOriginOrPrecursorOriginIfOpaque()
                                     ->Protocol();
  static constexpr const char* kExcludedProtocols[] = {
      url::kFileScheme,
      "chrome-extension",
      "chrome-untrusted",
  };
  if (protocol.empty() || base::Contains(kExcludedProtocols, protocol) ||
      blink::SchemeRegistry::ShouldTreatURLSchemeAsDisplayIsolated(protocol)) {
    return nullptr;
  }

  if (auto* window = blink::DynamicTo<blink::LocalDOMWindow>(context)) {
    if (auto* content_settings =
            GetContentSettingsIfNotEmpty(window->GetDisconnectedFrame())) {
      return content_settings;
    }

    if (auto* content_settings =
            GetContentSettingsIfNotEmpty(window->GetFrame())) {
      return content_settings;
    }

    // This may happen in some cases, e.g. when IsolatedSVGDocument is used.
    return nullptr;
  }

  if (auto* worker_or_worklet =
          blink::DynamicTo<blink::WorkerOrWorkletGlobalScope>(context)) {
    return worker_or_worklet->ContentSettingsClient();
  }

  DEBUG_ALIAS_FOR_OBJECT(context_alias, context);
  NOTREACHED() << "Unhandled ExecutionContext type";
}

BraveFarblingLevel GetBraveFarblingLevelFor(
    ExecutionContext* context,
    ContentSettingsType webcompat_settings_type,
    BraveFarblingLevel default_value) {
  BraveFarblingLevel value = default_value;
  if (context) {
    value = brave::BraveSessionCache::From(*context).GetBraveFarblingLevel(
        webcompat_settings_type);
  }
  return value;
}

bool AllowFingerprinting(ExecutionContext* context,
                         ContentSettingsType webcompat_settings_type) {
  return (GetBraveFarblingLevelFor(context, webcompat_settings_type,
                                   BraveFarblingLevel::OFF) !=
          BraveFarblingLevel::MAXIMUM);
}

bool AllowFontFamily(ExecutionContext* context,
                     const blink::AtomicString& family_name) {
  if (!context) {
    return true;
  }

  auto* settings = brave::GetContentSettingsClientFor(context);
  if (!settings) {
    return true;
  }

  if (!brave::BraveSessionCache::From(*context).AllowFontFamily(settings,
                                                                family_name)) {
    return false;
  }

  return true;
}

int FarbleInteger(ExecutionContext* context,
                  brave::FarbleKey key,
                  int spoof_value,
                  int min_value,
                  int max_value) {
  BraveSessionCache& cache = BraveSessionCache::From(*context);
  return cache.FarbledInteger(key, spoof_value, min_value, max_value);
}

bool BlockScreenFingerprinting(ExecutionContext* context,
                               bool early /* = false */) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kBraveBlockScreenFingerprinting)) {
    return false;
  }
  BraveFarblingLevel level = GetBraveFarblingLevelFor(
      context,
      early ? ContentSettingsType::BRAVE_WEBCOMPAT_NONE
            : ContentSettingsType::BRAVE_WEBCOMPAT_SCREEN,
      BraveFarblingLevel::OFF);
  return level != BraveFarblingLevel::OFF;
}

int FarbledPointerScreenCoordinate(const DOMWindow* view,
                                   FarbleKey key,
                                   int client_coordinate,
                                   int true_screen_coordinate) {
  const blink::LocalDOMWindow* local_dom_window =
      blink::DynamicTo<blink::LocalDOMWindow>(view);
  if (!local_dom_window) {
    return true_screen_coordinate;
  }
  ExecutionContext* context = local_dom_window->GetExecutionContext();
  if (!BlockScreenFingerprinting(context)) {
    return true_screen_coordinate;
  }
  auto* frame = local_dom_window->GetFrame();
  if (!frame) {
    return true_screen_coordinate;
  }
  double zoom_factor = frame->LayoutZoomFactor();
  return FarbleInteger(context, key, zoom_factor * client_coordinate, 0, 8);
}

blink::String BraveSessionCache::ExtractETLDPlusOne(const GURL& url) {
  // Handle special schemes
  if (url.SchemeIsFile()) {
    return blink::String("file");
  }
  if (url.SchemeIs("chrome-extension")) {
    return blink::String::FromUTF8(url.host());
  }
  if (url.SchemeIs("data") || url.SchemeIs("blob")) {
    return blink::String("data");
  }

  // Extract eTLD+1 using Chromium's public suffix list
  std::string etld_plus_one =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (etld_plus_one.empty()) {
    // Fallback for IP addresses, localhost, etc.
    return blink::String::FromUTF8(url.host());
  }

  return blink::String::FromUTF8(etld_plus_one);
}

base::Token BraveSessionCache::DeriveTokenFromSeed(uint64_t master_seed,
                                                    const GURL& url) {
  // Extract eTLD+1 to normalize subdomains
  blink::String domain = ExtractETLDPlusOne(url);

  // Convert seed to bytes (big-endian)
  uint8_t seed_bytes[8];
  seed_bytes[0] = (master_seed >> 56) & 0xFF;
  seed_bytes[1] = (master_seed >> 48) & 0xFF;
  seed_bytes[2] = (master_seed >> 40) & 0xFF;
  seed_bytes[3] = (master_seed >> 32) & 0xFF;
  seed_bytes[4] = (master_seed >> 24) & 0xFF;
  seed_bytes[5] = (master_seed >> 16) & 0xFF;
  seed_bytes[6] = (master_seed >> 8) & 0xFF;
  seed_bytes[7] = master_seed & 0xFF;

  // HMAC-SHA256(key=seed, message=domain)
  // This ensures deterministic but cryptographically secure derivation
  auto hmac_result =
      crypto::hmac::SignSha256(base::span(seed_bytes),
                               base::as_byte_span(domain.Utf8()));

  // Extract 128-bit token from HMAC result (first 16 bytes)
  uint64_t high = base::U64FromNativeEndian(base::span(hmac_result).first<8u>());
  uint64_t low = base::U64FromNativeEndian(base::span(hmac_result).subspan<8u, 8u>());

  return base::Token(high, low);
}

void BraveSessionCache::SetMasterFingerprintingSeed(uint64_t seed) {
  master_seed_ = seed;
  has_master_seed_ = true;

  // Derive token from master seed + current domain
  GURL url(execution_context_->Url());
  custom_farbling_token_ = DeriveTokenFromSeed(master_seed_, url);

  // Clear cached values to force regeneration
  farbled_integers_.clear();
  audio_farbling_helper_.reset();
}

void BraveSessionCache::SetWebRTCIPv4Override(const blink::String& ipv4) {
  webrtc_ipv4_override_ = ipv4;
  has_webrtc_ip_override_ = true;
}

void BraveSessionCache::SetWebRTCIPv6Override(const blink::String& ipv6) {
  webrtc_ipv6_override_ = ipv6;
  has_webrtc_ip_override_ = true;
}

BraveSessionCache::BraveSessionCache(ExecutionContext& context)
    : execution_context_(context) {
  if (auto* settings_client = GetContentSettingsClientFor(&context)) {
    default_shields_settings_ = settings_client->GetBraveShieldsSettings(
        ContentSettingsType::BRAVE_WEBCOMPAT_NONE);
    if (!default_shields_settings_) {
      DEBUG_ALIAS_FOR_OBJECT(settings_client_alias, settings_client);
      base::debug::DumpWithoutCrashing();
      default_shields_settings_ = brave_shields::mojom::ShieldsSettings::New();
    }
  } else {
    default_shields_settings_ = brave_shields::mojom::ShieldsSettings::New();
  }

  if (const auto* storage_key = GetStorageKey(&context);
      storage_key && storage_key->GetNonce() &&
      !storage_key->GetNonce()->is_empty()) {
    // Use storage key nonce hash to XOR the existing farbling token. Do not use
    // the nonce directly to not accidentaly leak it somehow via farbled values.
    const size_t storage_key_nonce_hash =
        base::FastHash(storage_key->GetNonce()->AsBytes());
    default_shields_settings_->farbling_token =
        base::Token(default_shields_settings_->farbling_token.high() ^
                        storage_key_nonce_hash,
                    default_shields_settings_->farbling_token.low() ^
                        storage_key_nonce_hash);
  }

  // Fetch fingerprinting overrides from browser process on navigation
  // Only fetch from window contexts - workers don't have access to this interface
  // but they can still access overrides set via JavaScript on the parent window
  if (blink::DynamicTo<blink::LocalDOMWindow>(&context)) {
    mojo::Remote<brave::mojom::blink::BraveFingerprintingHost> host;
    context.GetBrowserInterfaceBroker().GetInterface(
        host.BindNewPipeAndPassReceiver());
    if (host) {
      // Synchronous mojo call to ensure overrides are available immediately

      // Fetch master seed
      bool has_seed = false;
      uint64_t seed = 0;
      if (host->GetMasterSeed(&has_seed, &seed)) {
        if (has_seed) {
          // Apply the seed to derive farbling token
          SetMasterFingerprintingSeed(seed);
        }
      }

      // Fetch WebRTC IP overrides
      bool has_override = false;
      blink::String ipv4;
      blink::String ipv6;
      if (host->GetWebRTCIPOverrides(&has_override, &ipv4, &ipv6)) {
        if (has_override) {
          has_webrtc_ip_override_ = true;
          webrtc_ipv4_override_ = ipv4;
          webrtc_ipv6_override_ = ipv6;
        }
      }
    }
  }
}

BraveSessionCache& BraveSessionCache::From(ExecutionContext& context) {
  BraveSessionCache* cache = context.GetBraveSessionCache();
  if (!cache) {
    cache = MakeGarbageCollected<BraveSessionCache>(context);
    context.SetBraveSessionCache(cache);
  }
  return *cache;
}

// static
void BraveSessionCache::Init() {
  RegisterAllowFontFamilyCallback(base::BindRepeating(&brave::AllowFontFamily));
}

std::optional<blink::BraveAudioFarblingHelper>
BraveSessionCache::GetAudioFarblingHelper() {
  const auto audio_farbling_level =
      GetBraveFarblingLevel(ContentSettingsType::BRAVE_WEBCOMPAT_AUDIO);
  if (audio_farbling_level == BraveFarblingLevel::OFF) {
    return std::nullopt;
  }
  if (!audio_farbling_helper_) {
    // Use custom token if master seed is set, otherwise use default
    const base::Token& token = has_master_seed_ ? custom_farbling_token_
                                                 : default_shields_settings_->farbling_token;
    // This call is only expensive the first time; afterwards it returns
    // a cached value:
    const uint64_t fudge = token.high();
    const double fudge_factor = 0.99 + ((fudge / maxUInt64AsDouble) / 100);
    const uint64_t seed = token.low();
    audio_farbling_helper_.emplace(
        fudge_factor, seed,
        audio_farbling_level == BraveFarblingLevel::MAXIMUM);
  }
  return audio_farbling_helper_;
}

void BraveSessionCache::FarbleAudioChannel(base::span<float> dst) {
  const auto& audio_farbling_helper = GetAudioFarblingHelper();
  if (audio_farbling_helper) {
    audio_farbling_helper->FarbleAudioChannel(dst);
  }
}

void BraveSessionCache::PerturbPixels(base::span<uint8_t> data) {
  if (GetBraveFarblingLevel(ContentSettingsType::BRAVE_WEBCOMPAT_CANVAS) ==
      BraveFarblingLevel::OFF) {
    return;
  }
  PerturbPixelsInternal(data);
}

void BraveSessionCache::PerturbPixelsInternal(base::span<uint8_t> data) {
  if (data.empty()) {
    return;
  }

  // This needs to be type size_t because we pass it to std::string_view
  // later for content hashing. This is safe because the maximum canvas
  // dimensions are less than SIZE_T_MAX. (Width and height are each
  // limited to 32,767 pixels.)
  // Four bits per pixel
  const size_t pixel_count = data.size() / 4;

  // Use custom token if master seed is set, otherwise use default
  const base::Token& token = has_master_seed_ ? custom_farbling_token_
                                               : default_shields_settings_->farbling_token;

  // calculate initial seed to find first pixel to perturb, based on session
  // key, domain key, and canvas contents
  auto canvas_key = crypto::hmac::SignSha256(token.AsBytes(), data);
  uint64_t v = base::U64FromNativeEndian(base::span(canvas_key).first<8u>());
  // iterate through 32-byte canvas key and use each bit to determine how to
  // perturb the current pixel
  for (uint8_t key : canvas_key) {
    uint8_t bit = key;
    for (int j = 0; j < 16; j++) {
      if (j % 8 == 0) {
        bit = key;
      }
      // choose which channel (R, G, or B) to perturb
      uint8_t channel = v % 3;
      uint64_t pixel_index = 4 * (v % pixel_count) + channel;
      data[pixel_index] = data[pixel_index] ^ (bit & 0x1);
      bit = bit >> 1;
      // find next pixel to perturb
      v = lfsr_next(v);
    }
  }
}

blink::String BraveSessionCache::GenerateRandomString(
    std::string_view seed,
    blink::wtf_size_t length) {
  // Use custom token if master seed is set, otherwise use default
  const base::Token& token = has_master_seed_ ? custom_farbling_token_
                                               : default_shields_settings_->farbling_token;
  auto key = crypto::hmac::SignSha256(token.AsBytes(), base::as_byte_span(seed));
  // initial PRNG seed based on session key and passed-in seed string
  uint64_t v = base::U64FromNativeEndian(base::span(key).first<8u>());
  base::span<UChar> destination;
  blink::String value = blink::String::CreateUninitialized(length, destination);
  for (auto& c : destination) {
    c = kLettersForRandomStrings.at(v % kLettersForRandomStrings.size());
    v = lfsr_next(v);
  }
  return value;
}

blink::String BraveSessionCache::FarbledUserAgent(
    blink::String real_user_agent) {
  FarblingPRNG prng = MakePseudoRandomGenerator();
  blink::StringBuilder result;
  result.Append(real_user_agent);
  int extra = prng() % kFarbledUserAgentMaxExtraSpaces;
  for (int i = 0; i < extra; i++) {
    result.Append(" ");
  }
  return result.ToString();
}

int BraveSessionCache::FarbledInteger(FarbleKey key,
                                      int spoof_value,
                                      int min_random_offset,
                                      int max_random_offset) {
  // When master seed is active, always derive fresh values - don't cache
  // This ensures fingerprint changes immediately when seed is set
  if (has_master_seed_) {
    FarblingPRNG prng = MakePseudoRandomGenerator(key);
    int offset = base::checked_cast<int>(
        prng() % (1 + max_random_offset - min_random_offset) +
        min_random_offset);
    return offset + spoof_value;
  }

  // For default farbling (no master seed), use cache for consistency
  auto item = farbled_integers_.find(key);
  if (item == farbled_integers_.end()) {
    FarblingPRNG prng = MakePseudoRandomGenerator(key);
    auto added = farbled_integers_.insert(
        key, base::checked_cast<int>(
                 prng() % (1 + max_random_offset - min_random_offset) +
                 min_random_offset));

    return added.stored_value->value + spoof_value;
  }
  return item->value + spoof_value;
}

bool BraveSessionCache::AllowFontFamily(
    blink::WebContentSettingsClient* settings,
    const blink::AtomicString& family_name) {
  if (!settings ||
      GetBraveFarblingLevel(ContentSettingsType::BRAVE_WEBCOMPAT_FONT) ==
          BraveFarblingLevel::OFF ||
      !settings->IsReduceLanguageEnabled()) {
    return true;
  }
  switch (default_shields_settings_->farbling_level) {
    case BraveFarblingLevel::OFF:
      return true;
    case BraveFarblingLevel::BALANCED:
    case BraveFarblingLevel::MAXIMUM: {
      if (AllowFontByFamilyName(family_name,
                                blink::DefaultLanguage().GetString().Left(2))) {
        return true;
      }
      if (IsFontAllowedForFarbling(family_name)) {
        FarblingPRNG prng = MakePseudoRandomGenerator();
        prng.discard(family_name.Impl()->GetHash() % 16);
        return ((prng() % 20) == 0);
      } else {
        return false;
      }
    }
  }
  NOTREACHED();
}

FarblingPRNG BraveSessionCache::MakePseudoRandomGenerator(FarbleKey key) {
  // Use custom token if master seed is set, otherwise use default
  const base::Token& token = has_master_seed_ ? custom_farbling_token_
                                               : default_shields_settings_->farbling_token;
  uint64_t seed = token.high() ^ token.low() ^ static_cast<uint64_t>(key);
  return FarblingPRNG(seed);
}

BraveFarblingLevel BraveSessionCache::GetBraveFarblingLevel(
    ContentSettingsType webcompat_content_settings) {
  if (default_shields_settings_->farbling_level == BraveFarblingLevel::OFF) {
    return BraveFarblingLevel::OFF;
  }
  auto item = farbling_levels_.find(webcompat_content_settings);
  if (item != farbling_levels_.end()) {
    return item->value;
  }
  // The farbling level for webcompat_content_settings is not known yet,
  // so we will make a more expensive call to learn what it is.
  if (webcompat_content_settings > ContentSettingsType::BRAVE_WEBCOMPAT_NONE &&
      webcompat_content_settings < ContentSettingsType::BRAVE_WEBCOMPAT_ALL) {
    if (auto* settings_client =
            GetContentSettingsClientFor(execution_context_)) {
      auto shields_settings =
          settings_client->GetBraveShieldsSettings(webcompat_content_settings);
      // https://github.com/brave/brave-browser/issues/41889 debug.
      if (!shields_settings) {
        DEBUG_ALIAS_FOR_OBJECT(settings_client_alias, settings_client);
        base::debug::DumpWithoutCrashing();
        return default_shields_settings_->farbling_level;
      }
      farbling_levels_.insert(webcompat_content_settings,
                              shields_settings->farbling_level);
      return shields_settings->farbling_level;
    }
  }
  return default_shields_settings_->farbling_level;
}

void BraveSessionCache::Trace(blink::Visitor* visitor) const {
  visitor->Trace(execution_context_);
}

}  // namespace brave
