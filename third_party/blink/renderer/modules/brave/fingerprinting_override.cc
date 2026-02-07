// Copyright (c) 2026 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/third_party/blink/renderer/modules/brave/fingerprinting_override.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "brave/components/brave_fingerprinting/mojom/brave_fingerprinting.mojom-blink.h"
#include "brave/third_party/blink/renderer/core/farbling/brave_session_cache.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Get Mojo interface to browser-side fingerprinting service
mojo::Remote<brave::mojom::blink::BraveFingerprintingHost>&
GetFingerprintingHost(ExecutionContext* context) {
  static base::NoDestructor<mojo::Remote<brave::mojom::blink::BraveFingerprintingHost>> remote;

  if (!remote->is_bound() && context) {
    context->GetBrowserInterfaceBroker().GetInterface(
        remote->BindNewPipeAndPassReceiver());
  }

  return *remote;
}

}  // namespace

// static
void FingerprintingOverride::setFingerprintingSeed(ScriptState* script_state,
                                                   LocalDOMWindow& window,
                                                   uint64_t seed) {
  ExecutionContext* execution_context = window.GetExecutionContext();
  if (!execution_context) {
    return;
  }

  // Check if seed already exists - if so, just self-destruct without changing it
  auto& session_cache = brave::BraveSessionCache::From(*execution_context);
  if (session_cache.HasMasterSeed()) {
    SelfDestruct(script_state, "setFingerprintingSeed");
    return;
  }

  // Update local session cache immediately for current context
  session_cache.SetMasterFingerprintingSeed(seed);

  // Send seed to browser process for storage in BrowserContext
  auto& host = GetFingerprintingHost(execution_context);
  if (host) {
    host->SetMasterSeed(seed);
  }

  // Self-destruct: remove function from window after first call
  SelfDestruct(script_state, "setFingerprintingSeed");
}

// static
void FingerprintingOverride::setWebRTCIPv4(ScriptState* script_state,
                                           LocalDOMWindow& window,
                                           const String& ipv4) {
  ExecutionContext* execution_context = window.GetExecutionContext();
  if (!execution_context) {
    return;
  }

  // Check if WebRTC IP already exists - if so, just self-destruct without changing it
  auto& session_cache = brave::BraveSessionCache::From(*execution_context);
  if (session_cache.HasWebRTCIPOverride()) {
    SelfDestruct(script_state, "setWebRTCIPv4");
    return;
  }

  // Update local session cache immediately for current context
  session_cache.SetWebRTCIPv4Override(ipv4);

  // Send IPv4 to browser process for storage in BrowserContext
  auto& host = GetFingerprintingHost(execution_context);
  if (host) {
    host->SetWebRTCIPv4(ipv4);
  }

  // Self-destruct: remove function from window after first call
  SelfDestruct(script_state, "setWebRTCIPv4");
}

// static
void FingerprintingOverride::setWebRTCIPv6(ScriptState* script_state,
                                           LocalDOMWindow& window,
                                           const String& ipv6) {
  ExecutionContext* execution_context = window.GetExecutionContext();
  if (!execution_context) {
    return;
  }

  // Check if WebRTC IP already exists - if so, just self-destruct without changing it
  auto& session_cache = brave::BraveSessionCache::From(*execution_context);
  if (session_cache.HasWebRTCIPOverride()) {
    SelfDestruct(script_state, "setWebRTCIPv6");
    return;
  }

  // Update local session cache immediately for current context
  session_cache.SetWebRTCIPv6Override(ipv6);

  // Send IPv6 to browser process for storage in BrowserContext
  auto& host = GetFingerprintingHost(execution_context);
  if (host) {
    host->SetWebRTCIPv6(ipv6);
  }

  // Self-destruct: remove function from window after first call
  SelfDestruct(script_state, "setWebRTCIPv6");
}

// static
void FingerprintingOverride::SendSeedToBrowser(ExecutionContext* context,
                                               uint64_t seed) {
  if (!context) {
    return;
  }

  auto& host = GetFingerprintingHost(context);
  if (host) {
    host->SetMasterSeed(seed);
  }
}

// static
void FingerprintingOverride::SendIPv4ToBrowser(ExecutionContext* context,
                                               const String& ipv4) {
  if (!context) {
    return;
  }

  auto& host = GetFingerprintingHost(context);
  if (host) {
    host->SetWebRTCIPv4(ipv4);
  }
}

// static
void FingerprintingOverride::SendIPv6ToBrowser(ExecutionContext* context,
                                               const String& ipv6) {
  if (!context) {
    return;
  }

  auto& host = GetFingerprintingHost(context);
  if (host) {
    host->SetWebRTCIPv6(ipv6);
  }
}

// static
void FingerprintingOverride::SelfDestruct(ScriptState* script_state,
                                          const String& function_name) {
  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Object> global = context->Global();

  // Delete the function from window object
  // After this, typeof window.setFingerprintingSeed === "undefined"
  v8::Local<v8::String> key = V8String(isolate, function_name);
  global->Delete(context, key).Check();
}

}  // namespace blink
