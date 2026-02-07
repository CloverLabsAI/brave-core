/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/renderer/modules/brave/fingerprinting_overrides_installer.h"

#include "brave/third_party/blink/renderer/core/farbling/brave_session_cache.h"
#include "brave/third_party/blink/renderer/modules/brave/fingerprinting_override.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "v8/include/v8.h"

namespace brave {

void InstallFingerprintingOverrides(blink::WebLocalFrame* web_frame) {
  if (!web_frame) {
    return;
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate) {
    return;
  }

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context = web_frame->MainWorldScriptContext();
  if (v8_context.IsEmpty()) {
    return;
  }

  v8::Context::Scope context_scope(v8_context);

  blink::LocalDOMWindow* window = blink::CurrentDOMWindow(isolate);
  if (!window) {
    return;
  }

  blink::ExecutionContext* context = window->GetExecutionContext();
  if (!context) {
    return;
  }

  // Check if overrides already exist
  auto& session_cache = brave::BraveSessionCache::From(*context);
  bool has_seed = session_cache.HasMasterSeed();
  bool has_webrtc = session_cache.HasWebRTCIPOverride();

  // Don't install any functions if overrides already exist
  if (has_seed && has_webrtc) {
    return;
  }

  v8::Local<v8::Object> global = v8_context->Global();

  // Install setFingerprintingSeed if no seed exists
  if (!has_seed) {
    auto callback = v8::Function::New(
        v8_context,
        [](const v8::FunctionCallbackInfo<v8::Value>& info) {
          v8::Isolate* isolate = info.GetIsolate();
          if (info.Length() < 1 || !info[0]->IsNumber()) {
            isolate->ThrowException(v8::Exception::TypeError(
                blink::V8String(isolate, "setFingerprintingSeed requires a number argument")));
            return;
          }

          uint64_t seed = info[0]->IntegerValue(isolate->GetCurrentContext()).FromJust();

          blink::LocalDOMWindow* window = blink::CurrentDOMWindow(isolate);
          if (!window) {
            return;
          }

          blink::ExecutionContext* context = window->GetExecutionContext();
          if (!context) {
            return;
          }

          // Update local session cache
          auto& session_cache = brave::BraveSessionCache::From(*context);
          session_cache.SetMasterFingerprintingSeed(seed);

          // Send to browser process
          blink::FingerprintingOverride::SendSeedToBrowser(context, seed);

          // Self-destruct
          v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();
          v8::Local<v8::Object> global = v8_context->Global();
          v8::Local<v8::String> func_name = blink::V8String(isolate, "setFingerprintingSeed");
          (void)global->Delete(v8_context, func_name);
        }).ToLocalChecked();

    v8::Local<v8::String> func_name =
        blink::V8String(isolate, "setFingerprintingSeed");
    (void)global->Set(v8_context, func_name, callback);
  }

  // Install setWebRTCIPv4 and setWebRTCIPv6 if no IPs exist
  if (!has_webrtc) {
    auto ipv4_callback = v8::Function::New(
        v8_context,
        [](const v8::FunctionCallbackInfo<v8::Value>& info) {
          v8::Isolate* isolate = info.GetIsolate();
          if (info.Length() < 1 || !info[0]->IsString()) {
            isolate->ThrowException(v8::Exception::TypeError(
                blink::V8String(isolate, "setWebRTCIPv4 requires a string argument")));
            return;
          }

          blink::String ipv4 = blink::ToCoreString(isolate, info[0].As<v8::String>());

          blink::LocalDOMWindow* window = blink::CurrentDOMWindow(isolate);
          if (!window) {
            return;
          }

          blink::ExecutionContext* context = window->GetExecutionContext();
          if (!context) {
            return;
          }

          // Update local session cache
          auto& session_cache = brave::BraveSessionCache::From(*context);
          session_cache.SetWebRTCIPv4Override(ipv4);

          // Send to browser process
          blink::FingerprintingOverride::SendIPv4ToBrowser(context, ipv4);

          // Self-destruct both IPv4 and IPv6 functions
          v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();
          v8::Local<v8::Object> global = v8_context->Global();
          (void)global->Delete(v8_context, blink::V8String(isolate, "setWebRTCIPv4"));
          (void)global->Delete(v8_context, blink::V8String(isolate, "setWebRTCIPv6"));
        }).ToLocalChecked();

    auto ipv6_callback = v8::Function::New(
        v8_context,
        [](const v8::FunctionCallbackInfo<v8::Value>& info) {
          v8::Isolate* isolate = info.GetIsolate();
          if (info.Length() < 1 || !info[0]->IsString()) {
            isolate->ThrowException(v8::Exception::TypeError(
                blink::V8String(isolate, "setWebRTCIPv6 requires a string argument")));
            return;
          }

          blink::String ipv6 = blink::ToCoreString(isolate, info[0].As<v8::String>());

          blink::LocalDOMWindow* window = blink::CurrentDOMWindow(isolate);
          if (!window) {
            return;
          }

          blink::ExecutionContext* context = window->GetExecutionContext();
          if (!context) {
            return;
          }

          // Update local session cache
          auto& session_cache = brave::BraveSessionCache::From(*context);
          session_cache.SetWebRTCIPv6Override(ipv6);

          // Send to browser process
          blink::FingerprintingOverride::SendIPv6ToBrowser(context, ipv6);

          // Self-destruct both IPv4 and IPv6 functions
          v8::Local<v8::Context> v8_context = isolate->GetCurrentContext();
          v8::Local<v8::Object> global = v8_context->Global();
          (void)global->Delete(v8_context, blink::V8String(isolate, "setWebRTCIPv4"));
          (void)global->Delete(v8_context, blink::V8String(isolate, "setWebRTCIPv6"));
        }).ToLocalChecked();

    v8::Local<v8::String> ipv4_name = blink::V8String(isolate, "setWebRTCIPv4");
    v8::Local<v8::String> ipv6_name = blink::V8String(isolate, "setWebRTCIPv6");
    (void)global->Set(v8_context, ipv4_name, ipv4_callback);
    (void)global->Set(v8_context, ipv6_name, ipv6_callback);
  }
}

}  // namespace brave
