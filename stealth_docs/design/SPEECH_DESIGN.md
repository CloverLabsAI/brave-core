# Speech Synthesis Voice Profile Farbling - Design Document

## Goal
Implement deterministic, seed-based speech synthesis voice profile farbling that integrates with the existing `setFingerprintingSeed()` API.

## Current Implementation

**Brave's existing farbling** (speech_synthesis.cc):
- **OFF**: Returns real voice list
- **BALANCED**: Returns real voices + 1 fake voice (random name from 14 options)
- **MAXIMUM**: Returns empty voice list

**Problem**: Even in BALANCED mode, the fake voice name uses `MakePseudoRandomGenerator()` without domain-specific control, and we only add noise rather than providing full control.

## Proposed Solution

### Priority System (3 Tiers)

```
Priority 1: Per-Context Custom Profiles (when setFingerprintingSeed is set)
  ↓ if HasMasterSeed() == true
  Return deterministic voice list from predefined profiles

Priority 2: Brave's Existing Farbling (when farbling enabled, no seed)
  ↓ if farbling_level == BALANCED
  Return real voices + 1 fake voice (current behavior)

Priority 3: No Farbling
  ↓ if farbling_level == OFF
  Return real voice list unchanged
```

### Voice Profile Database

Create realistic voice profile sets for different "virtual platforms":

**Profile Structure:**
```cpp
struct VoiceProfile {
  const char* voice_uri;
  const char* name;
  const char* lang;
  bool is_local_service;
  bool is_default;
};
```

**Profile Sets (12 total, matching macOS voice patterns):**

1. **macOS Profile 1** (en-US dominant):
   - Samantha (en-US, default)
   - Alex (en-US)
   - Daniel (en-GB)
   - Karen (en-AU)

2. **macOS Profile 2** (multilingual):
   - Victoria (en-US, default)
   - Thomas (fr-FR)
   - Amelie (fr-CA)
   - Joana (pt-PT)

3. **macOS Profile 3** (minimal):
   - Fred (en-US, default)
   - Fiona (en-Scotland)

4-12. More variations with different combinations...

### Selection Algorithm

```cpp
if (cache.HasMasterSeed()) {
  // Use MakePseudoRandomGenerator to get deterministic seed-based selection
  // This uses custom_farbling_token_ which is already domain-salted via HMAC-SHA256
  brave::FarblingPRNG prng = cache.MakePseudoRandomGenerator(
      brave::FarbleKey::kSpeechSynthesis);

  // Select one of the 12 voice profiles deterministically
  size_t profile_index = prng() % kVoiceProfiles.size();
  const VoiceProfile* profile = kVoiceProfiles[profile_index];

  // Build voice list from selected profile
  voice_list_.clear();
  for (const VoiceProfile& voice : profile) {
    auto mojom_voice = mojom::blink::SpeechSynthesisVoice::New();
    mojom_voice->voice_uri = blink::String(voice.voice_uri);
    mojom_voice->name = blink::String(voice.name);
    mojom_voice->lang = blink::String(voice.lang);
    mojom_voice->is_local_service = voice.is_local_service;
    mojom_voice->is_default = voice.is_default;
    voice_list_.push_back(
        MakeGarbageCollected<SpeechSynthesisVoice>(std::move(mojom_voice)));
  }
  VoicesDidChange();
  return;
}
```

### Integration Points

**File to Modify:**
- `/Volumes/BuilderOSteroids/GitHub/brave-browser/src/brave/chromium_src/third_party/blink/renderer/modules/speech/speech_synthesis.cc`

**Pattern (same as screen farbling):**
```cpp
// PRIORITY 1: Custom profile system (when setFingerprintingSeed is called)
if (cache.HasMasterSeed()) {
  // Use per-context voice profile
  return CustomVoiceProfile();
}

// PRIORITY 2: Brave's existing farbling (when farbling enabled)
if (farbling_level == BraveFarblingLevel::BALANCED) {
  // Use existing fake voice logic
  return ExistingFakingVoice();
}

// PRIORITY 3: No farbling
OnSetVoiceList_ChromiumImpl(std::move(mojom_voices));
```

## Benefits

1. **Deterministic**: Same seed + same domain = same voice list
2. **Domain-salted**: Different domains get different voice profiles (privacy)
3. **Subdomain consistency**: www.example.com and api.example.com get same voices
4. **Realistic**: Voice profiles match real macOS/Chrome installations
5. **Non-breaking**: Existing Brave farbling still works as fallback
6. **Automation-friendly**: Full control via setFingerprintingSeed()

## Security Considerations

- **Cross-site tracking**: Mitigated by HMAC-SHA256 domain salting
- **Subdomain detection**: Prevented by eTLD+1 normalization
- **Profile database fingerprinting**: All profiles are authentic macOS voices
- **API detection**: setFingerprintingSeed self-destructs (already implemented)

## Testing

```javascript
// Test 1: Determinism
window.setFingerprintingSeed(12345);
const voices1 = speechSynthesis.getVoices();
// reload page
const voices2 = speechSynthesis.getVoices();
assert.deepEqual(voices1, voices2); // ✅ Same

// Test 2: Subdomain consistency
// On www.example.com
window.setFingerprintingSeed(12345);
const voices_www = speechSynthesis.getVoices();

// On api.example.com
window.setFingerprintingSeed(12345);
const voices_api = speechSynthesis.getVoices();
assert.deepEqual(voices_www, voices_api); // ✅ Same eTLD+1

// Test 3: Cross-domain isolation
// On example.com
window.setFingerprintingSeed(12345);
const voices_ex = speechSynthesis.getVoices();

// On google.com
window.setFingerprintingSeed(12345);
const voices_goog = speechSynthesis.getVoices();
assert.notDeepEqual(voices_ex, voices_goog); // ✅ Different domains
```

## Implementation Steps

1. ✅ Research existing implementation
2. ✅ Design profile database and selection algorithm
3. [ ] Implement custom voice profile system
4. [ ] Update SPEECH.md documentation
5. [ ] Test with automation frameworks
