// Copyright (c) 2024 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/renderer/brave_render_frame_observer.h"

#include <string>

#include "brave/third_party/blink/renderer/modules/brave/fingerprinting_overrides_installer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/render_frame.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/web/web_local_frame.h"

BraveRenderFrameObserver::BraveRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

BraveRenderFrameObserver::~BraveRenderFrameObserver() = default;

void BraveRenderFrameObserver::OnDestruct() {
  delete this;
}

void BraveRenderFrameObserver::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

void BraveRenderFrameObserver::DidClearWindowObject() {
  // Install fingerprinting override functions only if overrides don't already
  // exist. Kept in addition to DidCreateScriptContext() below for resilience;
  // InstallFingerprintingOverrides() is idempotent (it only installs setters
  // that are not already accounted for by an existing override).
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  if (web_frame) {
    brave::InstallFingerprintingOverrides(web_frame);
  }
}

void BraveRenderFrameObserver::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int32_t world_id) {
  // Install the per-context fingerprinting setters as early as possible: at
  // main-world V8 context creation, before the first page script (including
  // injected document_start scripts) runs. DidClearWindowObject() installed
  // them too late (only by readyState=interactive), so a synchronous,
  // guarded `if (typeof window.setFingerprintingSeed === 'function')` call at
  // document_start saw `undefined` and silently skipped. Only the main world
  // (ISOLATED_WORLD_ID_GLOBAL) should expose these functions.
  if (world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  if (web_frame) {
    brave::InstallFingerprintingOverrides(web_frame);
  }
}
